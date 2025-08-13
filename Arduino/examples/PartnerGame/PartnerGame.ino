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
#include <TFT_eSPI.h>
#include "lv_conf.h"
#include "CST816S.h"

// 自定義程式庫
#include "PartnerData.h"

#define LVGL_TICK_PERIOD_MS 2

// 螢幕解析度
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 240;

// LVGL 緩衝區
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

// 硬體物件
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);
CST816S touch(6, 7, 13, 5);  // sda, scl, rst, irq

// 遊戲管理器
PartnerDataManager dataManager;

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

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();

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
    
    // 只有內容改變時才更新
    if (content != lastContent || currentTraitIndex != lastTraitIndex) {
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
                currentPhase = PHASE_RESULT;
                if (mainLabel) {
                    lv_label_set_text(mainLabel, "MATCH!\n\nCongratulations!");
                }
                lastUpdate = now;
            } else {
                Serial.println("Match failed, unlock new trait");
                dataManager.processWrongMatch();
                
                if (dataManager.isGameOver()) {
                    currentPhase = PHASE_RESULT;
                    if (mainLabel) {
                        lv_label_set_text(mainLabel, "GAME OVER\n\nTry Again!");
                    }
                    lastUpdate = now;
                } else {
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
    
    // 初始化 LVGL
    lv_init();
    
    // 初始化 TFT
    tft.begin();
    tft.setRotation(0);
    
    // 初始化觸控
    touch.begin();
    
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
    
    // 註冊觸控驅動
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    // 創建 LVGL 計時器
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick_callback,
        .name = "lvgl_tick"
    };
    
    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000);
    
    // 載入測試資料
    if (!dataManager.loadFromCSV(testCSV)) {
        Serial.println("ERROR: CSV data loading failed");
        return;
    }
    
    Serial.print("Loaded players: ");
    Serial.println(dataManager.getPlayerCount());
    
    // 開始遊戲
    dataManager.startGame(0, 0);
    
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
    
    // 初始顯示
    updateDisplay();
    
    Serial.println("Initialization complete");
    Serial.print("Unlocked traits: ");
    Serial.print(dataManager.getUnlockedTraitCount());
    Serial.print("/");
    Serial.println(dataManager.getTotalTraitCount());
}

void loop() {
    static bool lastTouchState = false;
    
    // 更新 LVGL
    lv_timer_handler();
    
    // 檢查觸控狀態變化
    bool currentTouchState = touch.available();
    if (currentTouchState && !lastTouchState) {
        // 觸控按下 - 先檢查滑動，再處理點擊
        handleSwipe();
        
        // 如果沒有滑動手勢，處理點擊
        if (touch.data.gestureID == SINGLE_CLICK) {
            handleTouch();
        }
    }
    lastTouchState = currentTouchState;
    
    // 定期更新顯示（只在需要時）
    static unsigned long lastDisplayUpdate = 0;
    unsigned long now = millis();
    if (now - lastDisplayUpdate > 100) {
        updateDisplay();
        lastDisplayUpdate = now;
    }
    
    // 結果顯示自動返回
    if (currentPhase == PHASE_RESULT && (now - lastUpdate > 5000)) {
        Serial.println("Auto restart game");
        dataManager.resetGame();
        dataManager.startGame(currentPlayerId, currentPlayerId);
        currentPhase = PHASE_DISPLAYING;
        currentTraitIndex = 0;  // 重置到第一個特徵
        updateDisplay();
    }
    
    delay(5);
}