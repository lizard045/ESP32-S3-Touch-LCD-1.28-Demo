#ifndef CONFIG_H
#define CONFIG_H

// 硬體配置
#define HARDWARE_VERSION "ESP32-S3-LCD-1.28"

// 螢幕設定
#define SCREEN_WIDTH     240
#define SCREEN_HEIGHT    240
#define SCREEN_ROTATION  0

// SPI / LCD 腳位（依最新接線）
// SCLK -> GPIO12, MOSI -> GPIO11
// LCD_BL 直連 3.3V（非由 GPIO 控制）
#define LCD_SCLK_PIN     12
#define LCD_MOSI_PIN     11
#define LCD_CS_PIN       10  // LCD_CS - 10
#define LCD_DC_PIN        9   // LCD_DC-9
#define LCD_RST_PIN       8   // LCD_RST-8
// 若需以 GPIO 控制背光，請額外接至可用 GPIO 並新增定義；目前為常亮：3.3V

// 觸控設定（依你提供的最新接線）
#define TOUCH_SDA        16  // TP_SDA - 16
#define TOUCH_SCL        21  // TP_SCL - 21
#define TOUCH_IRQ        13  // TP_INT - 13
#define TOUCH_RST        18  // TP_RST - 18

// 是否啟用觸控（疑難排解時可暫時關閉以排除中斷/I2C 造成的崩潰）
#define TOUCH_ENABLED     1

// 紅外線設定 (使用可用的GPIO)
#define IR_SEND_PIN      15  // 紅外線發射（未變更）
#define IR_RECV_PIN      7   // Receiver out - 7
#define IR_LED_PIN       5   // LED DIN - 5（狀態指示/外接LED）

// 電池監測 (使用ADC功能的GPIO)
#define BATTERY_PIN      33  // GPIO33 - 支援ADC和觸控功能
#define BATTERY_DIVIDER  2.0  // 分壓比例

// 遊戲設定
#define MAX_PLAYERS      10
#define MAX_ERRORS       3
#define HIDDEN_TRAITS    5
#define TOTAL_TRAITS     10

// 計時設定 (毫秒)
#define LVGL_TICK_PERIOD    2
#define SPLASH_DURATION     2000
#define INFO_DISPLAY_TIME   3000
#define SCAN_TIMEOUT        10000
#define RESULT_DISPLAY_TIME 5000
#define STATUS_PRINT_INTERVAL 5000

// 紅外線通訊設定
#define IR_PROTOCOL      NEC
#define IR_ADDRESS       0x1234
#define IR_TIMEOUT       5000
#define IR_MESSAGE_QUEUE 10

// 除錯設定
#define DEBUG_ENABLED    1
#define DEBUG_IR         1
#define DEBUG_TOUCH      1
#define DEBUG_GAME       1

// 顏色主題
#define THEME_PRIMARY    0x2196F3
#define THEME_SUCCESS    0x4CAF50
#define THEME_ERROR      0xF44336
#define THEME_WARNING    0xFF9800
#define THEME_BACKGROUND 0x121212
#define THEME_TEXT       0xFFFFFF
#define THEME_HIDDEN     0x757575

// 字體設定
#if SCREEN_WIDTH >= 240
    #define FONT_TITLE   &lv_font_montserrat_20
    #define FONT_NORMAL  &lv_font_montserrat_16
    #define FONT_SMALL   &lv_font_montserrat_12
#else
    #define FONT_TITLE   &lv_font_montserrat_16
    #define FONT_NORMAL  &lv_font_montserrat_12
    #define FONT_SMALL   &lv_font_montserrat_10
#endif

// 除錯巨集
#if DEBUG_ENABLED
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(x, ...) Serial.printf(x, __VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(x, ...)
#endif

#if DEBUG_IR
    #define IR_DEBUG_PRINT(x) Serial.print("[IR] " x)
    #define IR_DEBUG_PRINTLN(x) Serial.println("[IR] " x)
#else
    #define IR_DEBUG_PRINT(x)
    #define IR_DEBUG_PRINTLN(x)
#endif

#if DEBUG_TOUCH
    #define TOUCH_DEBUG_PRINT(x) Serial.print("[TOUCH] " x)
    #define TOUCH_DEBUG_PRINTLN(x) Serial.println("[TOUCH] " x)
#else
    #define TOUCH_DEBUG_PRINT(x)
    #define TOUCH_DEBUG_PRINTLN(x)
#endif

#if DEBUG_GAME
    #define GAME_DEBUG_PRINT(x) Serial.print("[GAME] " x)
    #define GAME_DEBUG_PRINTLN(x) Serial.println("[GAME] " x)
#else
    #define GAME_DEBUG_PRINT(x)
    #define GAME_DEBUG_PRINTLN(x)
#endif

// 版本資訊
#define FIRMWARE_VERSION "1.0.0"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// 記憶體設定
#define LVGL_BUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10)
#define CSV_BUFFER_SIZE  2048

// 電源管理
#define LOW_BATTERY_THRESHOLD    3.3  // V
#define SLEEP_TIMEOUT           300   // 秒 (5分鐘無操作進入休眠)
#define WAKE_UP_SOURCES         (ESP_SLEEP_WAKEUP_TOUCHPAD | ESP_SLEEP_WAKEUP_EXT0)

#endif
