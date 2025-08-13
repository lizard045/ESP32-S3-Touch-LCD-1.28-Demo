/*
 * 派對交流遊戲 - ESP32-S3 圓形LCD版本
 * 
 * 硬體需求:
 * - ESP32-S3 Touch LCD 1.28" 圓形螢幕
 * - CST816S 電容觸控
 * - 5mm 紅外線發射器 (GPIO15)
 * - VS1838B 紅外線接收器 (GPIO16)  
 * - 狀態指示LED (GPIO17)
 * - 3.7V 1200mAh 鋰電池
 * 
 * 遊戲流程:
 * 1. 載入CSV玩家資料
 * 2. 顯示部分特徵資訊 (隱藏5個)
 * 3. 透過紅外線與其他玩家配對
 * 4. 配對錯誤時揭露隱藏特徵
 * 5. 最多錯3次，成功則顯示MATCH!
 */

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "lv_conf.h"
#include "CST816S.h"
#include <esp_timer.h>

// 自定義程式庫
#include "PartnerData.h"
#include "DisplayManager.h"
#include "IRCommunication.h"
#include "Config.h"

// 硬體設定
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
#define TOUCH_SDA     6
#define TOUCH_SCL     7
#define TOUCH_RST     13
#define TOUCH_IRQ     5

#define IR_SEND_PIN   15
#define IR_RECV_PIN   16
#define IR_LED_PIN    17

#define BATTERY_PIN   33  // 電池電壓監測

// LVGL計時器設定
#define LVGL_TICK_PERIOD_MS 2

// 遊戲狀態
enum GamePhase {
    PHASE_INIT,        // 初始化
    PHASE_LOADING,     // 載入資料
    PHASE_MENU,        // 主選單
    PHASE_GAME_START,  // 遊戲開始
    PHASE_DISPLAYING,  // 顯示特徵
    PHASE_SCANNING,    // 掃描其他玩家
    PHASE_MATCHING,    // 配對中
    PHASE_RESULT,      // 結果顯示
    PHASE_ERROR        // 錯誤處理
};

// 全域物件
TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);
CST816S touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);

// LVGL相關
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * SCREEN_HEIGHT / 10];
esp_timer_handle_t lvgl_tick_timer = NULL;

// 移除不需要的複雜物件和變數

// 計時器句柄 (已在上面定義)

// 移除不需要的CSV測試資料

// 函數宣告 (極簡版本，大部分已移除)
void lvglTickCallback(void* arg);
void my_disp_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);
void my_touchpad_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);

// LVGL計時器回調
void lvglTickCallback(void* arg) {
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

// LVGL顯示回調
void my_disp_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

// LVGL觸控回調
void my_touchpad_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    bool touched = touch.available();
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touch.data.x;
        data->point.y = touch.data.y;
    }
}

// 全域變數
lv_obj_t* label_main;
int current_trait = 0;
String traits[10] = {
    "Partner: Single",
    "Pet: Don't have", 
    "Bad Habit: Not",
    "E/I: Introversion(I)",
    "N/S: Intuition(N)",
    "T/F: Thinking(T)",
    "J/P: Perceiving(P)",
    "Gender: Male",
    "Height: <=170cm",
    "Accessories: Wear glasses"
};

void setup() {
    Serial.begin(115200);
    Serial.println("=== 簡化特徵查看程式 ===");
    
    // 初始化TFT螢幕
    tft.begin();
    tft.setRotation(0);
    
    // 初始化觸控
    touch.begin();
    
    // 初始化LVGL (參考LVGL_Arduino範例)
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * SCREEN_HEIGHT / 10);
    
    // 設置顯示驅動
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // 設置觸控驅動
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    // 創建LVGL計時器
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvglTickCallback,
        .name = "lvgl_tick"
    };
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000);
    
    // 創建主標籤
    label_main = lv_label_create(lv_scr_act());
    lv_label_set_text(label_main, traits[current_trait].c_str());
    lv_obj_set_style_text_align(label_main, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_main, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(label_main, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_main, SCREEN_WIDTH - 20);
    
    Serial.println("✅ 初始化完成，觸控切換特徵");
}

void loop() {
    lv_timer_handler(); /* let the GUI do its work */
    
    // 簡單的觸控檢測
    if (touch.available()) {
        current_trait = (current_trait + 1) % 10;  // 循環切換特徵
        lv_label_set_text(label_main, traits[current_trait].c_str());
        Serial.print("Switched to trait ");
        Serial.print(current_trait);
        Serial.print(": ");
        Serial.println(traits[current_trait]);
        delay(300);  // 防止重複觸發
    }
    
    delay(5);
}

// 極簡版本完成！所有舊函數已移除
