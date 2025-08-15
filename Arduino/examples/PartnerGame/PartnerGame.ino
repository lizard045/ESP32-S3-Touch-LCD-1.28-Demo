/*
 * 派對交流遊戲 - ESP32-S3 圓形LCD版本
 * 參照 LVGL_Arduino.ino 結構設計
 * 
 * 功能:
 * - 初始顯示前5個特徵 (Partner, Pet, Bad Habit, E/I, N/S)
 * - 左上角顯示解鎖進度 CR : (5/10)
 * - 觸控切換特徵，感應失敗時解鎖新特徵
 */

#include <lvgl.h>
// 強制使用專案內的螢幕設定（GC9A01 與你提供的腳位相符）
// 使用工作區內的客製化腳位設定檔（相對於此 .ino 的路徑）
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include "lv_conf.h"
#include "CST816S.h"
#include "Config.h"

// 自定義程式庫
#include "PartnerData.h"
#include "IRCommunication.h"

#define LVGL_TICK_PERIOD_MS 2

// 螢幕解析度
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 240;

// LVGL 緩衝區
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

// 硬體物件
// 腳位統一由 Config.h 提供
Adafruit_GC9A01A tft(LCD_CS_PIN, LCD_DC_PIN, LCD_MOSI_PIN, LCD_SCLK_PIN, LCD_RST_PIN);
CST816S touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);  // 與 Config.h 同步

// 遊戲管理器
PartnerDataManager dataManager;

// 紅外線通訊
IRCommunication irComm;

static bool irMatchedShown = false;

// 遊戲狀態
enum GamePhase {
    PHASE_DISPLAYING,  // 顯示特徵
    PHASE_SCANNING,    // 掃描配對
    PHASE_RESULT       // 結果顯示
};

static GamePhase currentPhase = PHASE_DISPLAYING;
static int currentPlayerId = 0;
static int currentTraitIndex = 0;  // 當前顯示的特徵索引
static unsigned long lastUpdate = 0;

// LVGL 物件
static lv_obj_t* mainLabel = nullptr;
static lv_obj_t* statusLabel = nullptr;
// 錯誤大 X 覆蓋層（使用兩條紅色對角線）
static lv_obj_t* errorLine1 = nullptr;
static lv_obj_t* errorLine2 = nullptr;
static bool errorXShowing = false;
static unsigned long errorShownAt = 0;
static const uint16_t errorDisplayMs = 1200;
// Unlock +1 提示
static lv_obj_t* unlockLabel = nullptr;
static bool unlockShowing = false;
static unsigned long unlockShownAt = 0;
static const uint16_t unlockDisplayMs = 1000;

// 失敗時 LED 提示（以 IR_LED_PIN 控制，維持 3 秒）
static bool ledErrorOn = false;
static unsigned long ledErrorSince = 0;
static const uint16_t ledErrorDurationMs = 3000;

// 測試CSV資料
const String testCSV = 
    "時間戳記,Partner,Pet,Bad habit,MBTI(E/I),MBTI(N/S),MBTI(T/F),MBTI(J/P),Gender,Height,Accessories\n"
    "2025/8/13 下午 5:36:35,Single,Don't have,Smoker,Introversion(I),Intuition(N),Thinking(T),Judging(J),Male,<=170cm,Wear glasses\n"
    "2025/8/13 下午 5:39:31,Single,Don't have,Not,Introversion(I),Intuition(N),Thinking(T),Perceiving(P),Male,<=170cm,Wear glasses\n";

// LVGL 計時器回調
void lvgl_tick_callback(void *arg) {
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

// 顯示刷新回調
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.drawRGBBitmap(area->x1, area->y1, (uint16_t*)&color_p->full, w, h);

    lv_disp_flush_ready(disp_drv);
}

// 觸控讀取回調
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    bool touched = touch.available();
    
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touch.data.x;
        data->point.y = touch.data.y;
    }
}

// 更新顯示內容
void updateDisplay() {
    static String lastContent = "";
    static String lastStatus = "";
    static int lastTraitIndex = -1;
    
    // 顯示單個特徵
    String content = dataManager.getSingleTrait(currentPlayerId, currentTraitIndex);
    
    // 添加特徵導航提示
    // String navigationInfo = "(" + String(currentTraitIndex + 1) + "/10)\n\nSwipe to navigate";
    // content = content + "\n\n" + navigationInfo;
    
    // 只有內容或索引改變時才輸出及更新顯示
    if (content != lastContent || currentTraitIndex != lastTraitIndex) {
        Serial.print("updateDisplay -> trait index: ");
        Serial.print(currentTraitIndex);
        Serial.print(", text: ");
        Serial.println(content);
        if (mainLabel) {
            lv_label_set_text(mainLabel, content.c_str());
            lastContent = content;
            lastTraitIndex = currentTraitIndex;
        }
    }
    
    // 更新狀態 - CR計數器
    int unlocked = dataManager.getUnlockedTraitCount();
    int total = dataManager.getTotalTraitCount();
    String status = "CR : (" + String(unlocked) + "/" + String(total) + ")";
    
    if (status != lastStatus) {
        if (statusLabel) {
            lv_label_set_text(statusLabel, status.c_str());
            lastStatus = status;
        }
    }
}

// 建立或顯示紅色大 X
void showErrorX() {
    static bool style_inited = false;
    static lv_style_t style_line;
    if (!style_inited) {
        lv_style_init(&style_line);
        lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_RED));
        lv_style_set_line_width(&style_line, 10);
        lv_style_set_line_rounded(&style_line, true);
        style_inited = true;
    }

    // 對角線座標
    static lv_point_t line1_points[2] = {{20, 20}, {220, 220}};
    static lv_point_t line2_points[2] = {{220, 20}, {20, 220}};

    if (!errorLine1) {
        errorLine1 = lv_line_create(lv_scr_act());
        lv_obj_add_style(errorLine1, &style_line, 0);
        lv_line_set_points(errorLine1, line1_points, 2);
    }
    if (!errorLine2) {
        errorLine2 = lv_line_create(lv_scr_act());
        lv_obj_add_style(errorLine2, &style_line, 0);
        lv_line_set_points(errorLine2, line2_points, 2);
    }
    lv_obj_clear_flag(errorLine1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(errorLine2, LV_OBJ_FLAG_HIDDEN);

    errorXShowing = true;
    errorShownAt = millis();
}

void hideErrorX() {
    if (errorLine1) lv_obj_add_flag(errorLine1, LV_OBJ_FLAG_HIDDEN);
    if (errorLine2) lv_obj_add_flag(errorLine2, LV_OBJ_FLAG_HIDDEN);
}

// 動畫回呼：透明度
static void anim_set_opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
}

// 動畫回呼：縮放
static void anim_set_zoom_cb(void* obj, int32_t v) {
    lv_obj_set_style_transform_zoom((lv_obj_t*)obj, (lv_coord_t)v, 0);
}

// 顯示 Unlock +1 提示（淡入 + 輕微縮放）
void showUnlockToast() {
    if (!unlockLabel) {
        // 建立在最上層圖層，避免被任何物件遮住
        unlockLabel = lv_label_create(lv_layer_top());
        lv_label_set_text(unlockLabel, "Unlock +1");
        // 文字樣式
        lv_obj_set_style_text_color(unlockLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_set_style_text_font(unlockLabel, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_opa(unlockLabel, 255, 0);
        // 半透明黑底與圓角，增加可讀性
        lv_obj_set_style_bg_color(unlockLabel, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(unlockLabel, 128, 0);
        lv_obj_set_style_radius(unlockLabel, 8, 0);
        lv_obj_set_style_pad_all(unlockLabel, 8, 0);
        // 位置稍微高於中心，避免與大 X 完全重疊
        lv_obj_align(unlockLabel, LV_ALIGN_CENTER, 0, -60);
        // 置於最前景
        lv_obj_move_foreground(unlockLabel);
    }
    lv_obj_clear_flag(unlockLabel, LV_OBJ_FLAG_HIDDEN);
    // 初始狀態：透明且 100% 縮放
    lv_obj_set_style_opa(unlockLabel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_transform_zoom(unlockLabel, 256, 0); // 256 = 原始大小

    // 透明度動畫：0 -> 255
    lv_anim_t a_opa;
    lv_anim_init(&a_opa);
    lv_anim_set_var(&a_opa, unlockLabel);
    lv_anim_set_exec_cb(&a_opa, anim_set_opa_cb);
    lv_anim_set_values(&a_opa, 0, 255);
    lv_anim_set_time(&a_opa, 250);
    lv_anim_set_path_cb(&a_opa, lv_anim_path_ease_out);
    lv_anim_start(&a_opa);

    // 縮放動畫：256 -> 320（約 125%）
    lv_anim_t a_zoom;
    lv_anim_init(&a_zoom);
    lv_anim_set_var(&a_zoom, unlockLabel);
    lv_anim_set_exec_cb(&a_zoom, anim_set_zoom_cb);
    lv_anim_set_values(&a_zoom, 256, 320);
    lv_anim_set_time(&a_zoom, 300);
    lv_anim_set_path_cb(&a_zoom, lv_anim_path_ease_out);
    lv_anim_start(&a_zoom);

    // 後備：若未啟用動畫，仍確保可見
    lv_obj_set_style_opa(unlockLabel, 255, 0);
    lv_obj_move_foreground(unlockLabel);

    unlockShowing = true;
    unlockShownAt = millis();
}

// 處理滑動切換特徵
void handleSwipe() {
    static unsigned long lastSwipeTime = 0;
    unsigned long now = millis();
    
    // 防抖動
    if (now - lastSwipeTime < 200) return;
    lastSwipeTime = now;
    
    // 獲取手勢ID
    byte gestureID = touch.data.gestureID;
    
    if (currentPhase == PHASE_DISPLAYING) {
        switch (gestureID) {
            case SWIPE_LEFT:
                // 向左滑動 - 下一個特徵
                currentTraitIndex = (currentTraitIndex + 1) % 10;
                Serial.print("Swipe left - trait ");
                Serial.println(currentTraitIndex);
                break;
                
            case SWIPE_RIGHT:
                // 向右滑動 - 上一個特徵
                currentTraitIndex = (currentTraitIndex - 1 + 10) % 10;
                Serial.print("Swipe right - trait ");
                Serial.println(currentTraitIndex);
                break;
                
            case SINGLE_CLICK:
                // 單擊進入掃描模式
                Serial.println("Enter scanning mode");
                currentPhase = PHASE_SCANNING;
                // 開始 IR 掃描，於序列埠輸出接收情況
                irComm.startScanning();
                if (mainLabel) {
                    lv_label_set_text(mainLabel, "SCANNING...\n\nTap again to match");
                }
                break;
        }
    }
}

// 處理觸控事件
void handleTouch() {
    static unsigned long lastTouchTime = 0;
    unsigned long now = millis();
    
    // 防抖動
    if (now - lastTouchTime < 300) return;
    lastTouchTime = now;
    
    switch (currentPhase) {
        case PHASE_SCANNING: {
            Serial.println("Simulate pairing check");
            bool matchResult = random(0, 2);
            
            if (matchResult) {
                Serial.println("Match successful!");
                // 結束 IR 掃描
                irComm.stopScanning();
                currentPhase = PHASE_RESULT;
                if (mainLabel) {
                    lv_label_set_text(mainLabel, "MATCH!\n\nCongratulations!");
                }
                lastUpdate = now;
            } else {
                Serial.println("Match failed, unlock new trait");
                dataManager.processWrongMatch();
                
                if (dataManager.isGameOver()) {
                    // 結束 IR 掃描
                    irComm.stopScanning();
                    currentPhase = PHASE_RESULT;
                    if (mainLabel) {
                        lv_label_set_text(mainLabel, "PLEASE LEAVE~\n\nTry Again!");
                    }
                    lastUpdate = now;
                } else {
                    // 結束 IR 掃描
                    irComm.stopScanning();
                    currentPhase = PHASE_DISPLAYING;
                    // 強制更新顯示，因為解鎖了新特徵
                    updateDisplay();
                    
                    Serial.print("New unlocked traits: ");
                    Serial.print(dataManager.getUnlockedTraitCount());
                    Serial.print("/");
                    Serial.println(dataManager.getTotalTraitCount());
                }
            }
            break;
        }
            
        case PHASE_RESULT:
            Serial.println("Restart game");
            dataManager.resetGame();
            dataManager.startGame(currentPlayerId, currentPlayerId);
            currentPhase = PHASE_DISPLAYING;
            currentTraitIndex = 0;  // 重置到第一個特徵
            updateDisplay();
            break;
            
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("=== Party Match Game Start ===");
    
    // 先初始化 TFT，確保面板供電與 SPI Ready，再啟動 LVGL
    Serial.println("before tft.reset");

    pinMode(LCD_RST_PIN, OUTPUT);
    digitalWrite(LCD_RST_PIN, HIGH);
    delay(5);
    digitalWrite(LCD_RST_PIN, LOW);
    delay(10);
    digitalWrite(LCD_RST_PIN, HIGH);
    delay(120);
    Serial.println("before tft.begin");
    tft.begin();
    tft.setRotation(0);
    Serial.println("TFT init OK");
    delay(50);

    // 初始化 LVGL
    lv_init();
    Serial.println("lv_init OK");

    // 初始化觸控（可選）
    #if TOUCH_ENABLED
    touch.begin();
    Serial.println("Touch init OK");
    #else
    Serial.println("Touch disabled");
    #endif
    
    // LED 腳位初始化（若 IR_LED_PIN 可用）
    #ifdef IR_LED_PIN
    pinMode(IR_LED_PIN, OUTPUT);
    digitalWrite(IR_LED_PIN, LOW);
    #endif
    
    // 初始化 LVGL 顯示緩衝區
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);
    
    // 註冊顯示驅動
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    Serial.println("LVGL display driver registered");
    
    // 註冊觸控驅動
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    Serial.println("LVGL indev driver registered");
    
    // 使用 loop() 內以 millis() 推進 LVGL tick，避免 esp_timer 相容性問題
    
    // 載入測試資料（或從檔案載入）
    if (!dataManager.loadFromCSV(testCSV)) {
        Serial.println("ERROR: CSV data loading failed");
    } else {
        Serial.print("Loaded players: ");
        Serial.println(dataManager.getPlayerCount());
    }
    
    Serial.print("Loaded players: ");
    Serial.println(dataManager.getPlayerCount());
    
    // 開始遊戲（確保 currentPlayerId 合法）
    currentPlayerId = 0;
    dataManager.startGame(currentPlayerId, currentPlayerId);
    irComm.begin(currentPlayerId);
    
    // 創建 UI 元素
    // CR計數器在上方置中
    statusLabel = lv_label_create(lv_scr_act());
    lv_obj_set_width(statusLabel, screenWidth - 20);
    lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_WRAP);
    lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, 0);
    
    // 主要內容置中顯示
    mainLabel = lv_label_create(lv_scr_act());
    lv_obj_set_width(mainLabel, screenWidth - 20);
    lv_label_set_long_mode(mainLabel, LV_LABEL_LONG_WRAP);
    lv_obj_align(mainLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_align(mainLabel, LV_TEXT_ALIGN_CENTER, 0);
    // 設定較大的字體
    lv_obj_set_style_text_font(mainLabel, &lv_font_montserrat_20, 0);
    
    // 初始顯示（先顯示面板就緒，再切到特徵畫面）
    lv_label_set_text(mainLabel, "GC9A01 OK\nLoading traits...");
    lv_obj_set_style_text_font(mainLabel, &lv_font_montserrat_20, 0);
    lv_timer_handler();
    // 切換到特徵顯示
    updateDisplay();
    Serial.println("Displayed first trait");
    
    Serial.println("Initialization complete");
    Serial.print("Unlocked traits: ");
    Serial.print(dataManager.getUnlockedTraitCount());
    Serial.print("/");
    Serial.println(dataManager.getTotalTraitCount());

    // IR 接收由通訊模組負責
}

void loop() {
    static bool lastTouchState = false;
    static unsigned long lastTickMs = 0;
    
    // 更新 LVGL
    unsigned long nowMs = millis();
    unsigned long elapsed = nowMs - lastTickMs;
    if (elapsed > 0) {
        lv_tick_inc(elapsed);
        lastTickMs = nowMs;
    }
    lv_timer_handler();
    irComm.update();
    // 取出並列印所有新收到的 IR 訊息（監控視窗可見）
    IRMessage msg;
    while (irComm.hasNewMessage()) {
        if (irComm.getNextMessage(msg)) {
            irComm.printMessage(msg);
            // 收到任何 NEC 訊息即視為配對成功
            irComm.stopScanning();
            currentPhase = PHASE_RESULT;
            Serial.println("[UI] Show: It's Match ! (NEC received)");
            if (mainLabel) {
                lv_label_set_text(mainLabel, "It's Match !");
            }
            lastUpdate = millis();
        }
    }

    // 監聽 IR 連續兩次錯誤事件：顯示大 X 並處理解鎖/結束
    if (irComm.consumeWrongUnlockEvent()) {
        Serial.println("[UI] Two wrong signals -> show big X");
        irComm.stopScanning();
        showErrorX();
        showUnlockToast();
        // 亮起外接 LED 3 秒（紅色代表錯誤）
        #ifdef IR_LED_PIN
        digitalWrite(IR_LED_PIN, HIGH);
        ledErrorOn = true;
        ledErrorSince = millis();
        #endif
        // 暫時進入結果狀態，用於覆蓋顯示
        currentPhase = PHASE_RESULT;
        lastUpdate = millis();
    }

    // IR 訊號由通訊模組處理（此處不直接存取 IRremote 以避免多重定義）
    
    // 檢查觸控狀態變化
    #if TOUCH_ENABLED
    bool currentTouchState = touch.available();
    if (currentTouchState && !lastTouchState) {
        handleSwipe();
        if (touch.data.gestureID == SINGLE_CLICK) {
            handleTouch();
        }
    }
    lastTouchState = currentTouchState;
    #endif
    
    // 定期更新顯示（只在需要時）
    static unsigned long lastDisplayUpdate = 0;
    unsigned long now = millis();
    if (now - lastDisplayUpdate > 100) {
        updateDisplay();
        lastDisplayUpdate = now;
    }
    
    // Unlock +1 提示自動隱藏
    if (unlockShowing && (now - unlockShownAt > unlockDisplayMs)) {
        if (unlockLabel) lv_obj_add_flag(unlockLabel, LV_OBJ_FLAG_HIDDEN);
        unlockShowing = false;
    }

    // LED 錯誤提示自動關閉
    #ifdef IR_LED_PIN
    if (ledErrorOn && (now - ledErrorSince > ledErrorDurationMs)) {
        digitalWrite(IR_LED_PIN, HIGH);
        ledErrorOn = false;
    }
    #endif

    // 結果顯示自動返回
    if (currentPhase == PHASE_RESULT && errorXShowing && (now - errorShownAt > errorDisplayMs)) {
        // 大 X 展示完畢，根據遊戲狀態決定下一步
        hideErrorX();
        errorXShowing = false;
        if (dataManager.isGameOver()) {
            Serial.println("[UI] Please leave after wrong signals");
            if (mainLabel) {
                lv_label_set_text(mainLabel, "PLEASE LEAVE~\n\nTry Again!");
            }
            lastUpdate = now;
            // 保持在 PHASE_RESULT，交由原本的自動重啟計時處理
        } else {
            // 跳轉到剛解鎖的特徵索引
            int justUnlocked = dataManager.getLastUnlockedTraitIndex();
            if (justUnlocked >= 0 && justUnlocked < 10) {
                currentTraitIndex = justUnlocked;
            }
            currentPhase = PHASE_DISPLAYING;
            updateDisplay();
        }
    }

    if (currentPhase == PHASE_RESULT && !errorXShowing && (now - lastUpdate > 5000)) {
        Serial.println("Auto restart game");
        dataManager.resetGame();
        dataManager.startGame(currentPlayerId, currentPlayerId);
        currentPhase = PHASE_DISPLAYING;
        currentTraitIndex = 0;  // 重置到第一個特徵
        updateDisplay();
    }
    
    delay(5);
}