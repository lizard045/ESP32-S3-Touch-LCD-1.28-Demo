#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "lv_conf.h"
#include "CST816S.h"
#include "PartnerData.h"

// 顯示相關常數
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
#define DISPLAY_BUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10)

// 顏色定義
#define COLOR_PRIMARY   lv_color_hex(0x2196F3)
#define COLOR_SUCCESS   lv_color_hex(0x4CAF50)
#define COLOR_ERROR     lv_color_hex(0xF44336)
#define COLOR_WARNING   lv_color_hex(0xFF9800)
#define COLOR_BACKGROUND lv_color_hex(0x121212)
#define COLOR_TEXT      lv_color_hex(0xFFFFFF)
#define COLOR_HIDDEN    lv_color_hex(0x757575)

// 顯示模式枚舉
enum DisplayMode {
    MODE_SPLASH,        // 啟動畫面
    MODE_MENU,          // 主選單
    MODE_GAME_INFO,     // 遊戲資訊顯示
    MODE_MATCHING,      // 配對中
    MODE_RESULT,        // 結果顯示
    MODE_ERROR          // 錯誤顯示
};

class DisplayManager {
private:
    // 硬體相關
    TFT_eSPI* tft;
    CST816S* touch;
    
    // LVGL相關
    lv_disp_draw_buf_t draw_buf;
    lv_color_t* buf;
    lv_disp_drv_t disp_drv;
    lv_indev_drv_t indev_drv;
    
    // 顯示狀態
    DisplayMode currentMode;
    lv_obj_t* mainScreen;
    lv_obj_t* currentContainer;
    
    // UI元件
    lv_obj_t* titleLabel;
    lv_obj_t* contentLabel;
    lv_obj_t* statusLabel;
    lv_obj_t* buttonContainer;
    lv_obj_t* progressBar;
    
    // 資料管理器引用
    PartnerDataManager* dataManager;
    
    // 私有方法
    void initLVGL();
    void setupDisplay();
    void setupTouch();
    void createMainScreen();
    void clearScreen();
    
    // UI創建方法
    lv_obj_t* createContainer(lv_obj_t* parent);
    lv_obj_t* createLabel(lv_obj_t* parent, const char* text, lv_color_t color);
    lv_obj_t* createButton(lv_obj_t* parent, const char* text, lv_event_cb_t callback);
    void createProgressIndicator();
    
    // 顯示內容方法
    void showSplashScreen();
    void showMenuScreen();
    void showGameInfo();
    void showMatchingScreen();
    void showResultScreen(bool isMatch, const String& message);
    void showErrorScreen(const String& error);
    
    // 文字處理
    String wrapText(const String& text, int maxWidth);
    void updateScrollableContent(const String& content);

public:
    DisplayManager(TFT_eSPI* tftInstance, CST816S* touchInstance);
    ~DisplayManager();
    
    // 初始化
    bool begin();
    void setDataManager(PartnerDataManager* dm);
    
    // 顯示控制
    void update();
    void setMode(DisplayMode mode);
    DisplayMode getMode();
    
    // 內容顯示
    void showPlayerTraits(int playerId);
    void showMatchResult(bool success);
    void showErrorCount(int current, int max);
    void showMessage(const String& message, lv_color_t color = COLOR_TEXT);
    void showProgress(int percentage);
    
    // 互動處理
    bool isTouched();
    bool isButtonPressed();
    void handleTouch();
    
    // 動畫效果
    void fadeIn(lv_obj_t* obj, uint32_t duration = 500);
    void fadeOut(lv_obj_t* obj, uint32_t duration = 500);
    void slideIn(lv_obj_t* obj, lv_dir_t direction, uint32_t duration = 500);
    
    // 特效
    void showMatchAnimation();
    void showErrorAnimation();
    void showLoadingAnimation();
    
    // 工具方法
    void setBrightness(uint8_t level);
    void clearDisplay();
    void refreshDisplay();
};

// LVGL回調函數
void displayFlushCallback(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);
void touchReadCallback(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);
void lvglTickCallback(void* arg);

#endif
