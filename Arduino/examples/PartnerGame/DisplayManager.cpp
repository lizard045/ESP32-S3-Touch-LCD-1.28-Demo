#include "DisplayManager.h"

// 全域變數供LVGL回調使用
static TFT_eSPI* g_tft = nullptr;
static CST816S* g_touch = nullptr;

DisplayManager::DisplayManager(TFT_eSPI* tftInstance, CST816S* touchInstance) {
    tft = tftInstance;
    touch = touchInstance;
    g_tft = tftInstance;
    g_touch = touchInstance;
    
    currentMode = MODE_SPLASH;
    mainScreen = nullptr;
    currentContainer = nullptr;
    titleLabel = nullptr;
    contentLabel = nullptr;
    statusLabel = nullptr;
    buttonContainer = nullptr;
    progressBar = nullptr;
    dataManager = nullptr;
    
    // 分配顯示緩衝區
    buf = new lv_color_t[DISPLAY_BUFFER_SIZE];
}

DisplayManager::~DisplayManager() {
    if (buf) {
        delete[] buf;
    }
}

bool DisplayManager::begin() {
    // 初始化TFT
    tft->begin();
    tft->setRotation(0);  // 圓形螢幕通常不需要旋轉
    
    // 初始化觸控
    touch->begin();
    
    // 初始化LVGL
    initLVGL();
    
    // 設置顯示和觸控
    setupDisplay();
    setupTouch();
    
    // 創建主螢幕
    createMainScreen();
    
    // 顯示啟動畫面
    showSplashScreen();
    
    return true;
}

void DisplayManager::initLVGL() {
    lv_init();
    
    // 初始化顯示緩衝區
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, DISPLAY_BUFFER_SIZE);
}

void DisplayManager::setupDisplay() {
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = displayFlushCallback;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}

void DisplayManager::setupTouch() {
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchReadCallback;
    lv_indev_drv_register(&indev_drv);
}

void DisplayManager::createMainScreen() {
    mainScreen = lv_scr_act();
    lv_obj_set_style_bg_color(mainScreen, COLOR_BACKGROUND, 0);
}

void DisplayManager::setDataManager(PartnerDataManager* dm) {
    dataManager = dm;
}

void DisplayManager::update() {
    // 移除複雜的更新限制，讓主循環直接調用lv_timer_handler()
    // 這個方法現在主要用於其他顯示相關的更新
}

void DisplayManager::setMode(DisplayMode mode) {
    if (currentMode == mode) return;
    
    Serial.print("Changing display mode to: ");
    Serial.println(mode);
    
    currentMode = mode;
    clearScreen();
    
    switch (mode) {
        case MODE_SPLASH:
            showSplashScreen();
            break;
        case MODE_MENU:
            showMenuScreen();
            break;
        case MODE_GAME_INFO:
            showGameInfo();
            break;
        case MODE_MATCHING:
            // showMatchingScreen(); // 暫時註解，使用 showMessage 代替
            break;
        case MODE_RESULT:
            // 結果顯示需要額外參數，由外部調用 showResultScreen
            break;
        case MODE_ERROR:
            // 錯誤顯示需要額外參數，由外部調用 showErrorScreen
            break;
    }
}

DisplayMode DisplayManager::getMode() {
    return currentMode;
}

void DisplayManager::clearScreen() {
    // 減少螢幕清除操作，只在真正需要時清除
    if (currentContainer) {
        Serial.println("Clearing screen container");
        lv_obj_del(currentContainer);
        currentContainer = nullptr;
    }
    
    titleLabel = nullptr;
    contentLabel = nullptr;
    statusLabel = nullptr;
    buttonContainer = nullptr;
    progressBar = nullptr;
}

lv_obj_t* DisplayManager::createContainer(lv_obj_t* parent) {
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 20);
    lv_obj_center(container);
    lv_obj_set_style_bg_color(container, COLOR_BACKGROUND, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 10, 0);
    return container;
}

lv_obj_t* DisplayManager::createLabel(lv_obj_t* parent, const char* text, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

void DisplayManager::showSplashScreen() {
    currentContainer = createContainer(mainScreen);
    
    // 標題
    titleLabel = createLabel(currentContainer, "Party Match Game", COLOR_PRIMARY);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_20, 0);
    lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, -40);
    
    // 副標題
    lv_obj_t* subtitle = createLabel(currentContainer, "Find Your Partner", COLOR_TEXT);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, -10);
    
    // 載入提示
    statusLabel = createLabel(currentContainer, "Loading...", COLOR_WARNING);
    lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 30);
}

void DisplayManager::showMenuScreen() {
    currentContainer = createContainer(mainScreen);
    
    titleLabel = createLabel(currentContainer, "選擇遊戲模式", COLOR_PRIMARY);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_16, 0);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 20);
    
    // 按鈕容器
    buttonContainer = lv_obj_create(currentContainer);
    lv_obj_set_size(buttonContainer, 200, 120);
    lv_obj_center(buttonContainer);
    lv_obj_set_style_bg_color(buttonContainer, COLOR_BACKGROUND, 0);
    lv_obj_set_style_border_width(buttonContainer, 0, 0);
    
    // 開始遊戲按鈕
    lv_obj_t* startBtn = lv_btn_create(buttonContainer);
    lv_obj_set_size(startBtn, 160, 40);
    lv_obj_align(startBtn, LV_ALIGN_CENTER, 0, -20);
    lv_obj_t* startLabel = lv_label_create(startBtn);
    lv_label_set_text(startLabel, "開始遊戲");
    lv_obj_center(startLabel);
    
    // 查看資料按鈕
    lv_obj_t* dataBtn = lv_btn_create(buttonContainer);
    lv_obj_set_size(dataBtn, 160, 40);
    lv_obj_align(dataBtn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_t* dataLabel = lv_label_create(dataBtn);
    lv_label_set_text(dataLabel, "查看資料");
    lv_obj_center(dataLabel);
}

void DisplayManager::showGameInfo() {
    if (!dataManager) return;
    
    Serial.println("Creating game info UI");
    
    // 只在沒有容器時創建新的
    if (!currentContainer) {
        currentContainer = createContainer(mainScreen);
        
        titleLabel = createLabel(currentContainer, "Partner Traits", COLOR_PRIMARY);
        lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_16, 0);
        lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 10);
        
        // 可滾動的內容區域
        lv_obj_t* scrollArea = lv_obj_create(currentContainer);
        lv_obj_set_size(scrollArea, SCREEN_WIDTH - 40, 140);
        lv_obj_align(scrollArea, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_style_bg_color(scrollArea, COLOR_BACKGROUND, 0);
        lv_obj_set_style_border_width(scrollArea, 1, 0);
        lv_obj_set_style_border_color(scrollArea, COLOR_PRIMARY, 0);
        
        contentLabel = createLabel(scrollArea, "", COLOR_TEXT);
        lv_obj_set_width(contentLabel, lv_pct(95));
        lv_obj_align(contentLabel, LV_ALIGN_TOP_LEFT, 5, 5);
        
        // 狀態標籤
        statusLabel = createLabel(currentContainer, "", COLOR_WARNING);
        lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -10);
        
        Serial.println("Game info UI created");
    }
}

void DisplayManager::showPlayerTraits(int playerId, bool forceUpdate) {
    static String lastContent = "";
    static String lastStatus = "";
    static int lastPlayerId = -1;
    
    if (!contentLabel) {
        Serial.println("ERROR: contentLabel is NULL");
        return;
    }
    
    if (dataManager) {
        String traits = dataManager->getVisibleTraits(playerId);
        
        // 強制更新或內容真的改變時才更新
        if (forceUpdate || traits != lastContent || playerId != lastPlayerId) {
            if (traits.length() > 0) {
                lv_label_set_text(contentLabel, traits.c_str());
                lastContent = traits;
            } else {
                lv_label_set_text(contentLabel, "No data available");
                lastContent = "No data available";
            }
            lastPlayerId = playerId;
        }
        
        // 狀態標籤顯示解鎖進度
        int unlockedCount = dataManager->getUnlockedTraitCount();
        int totalCount = dataManager->getTotalTraitCount();
        String status = "Unlocked: " + String(unlockedCount) + "/" + String(totalCount);
        
        // 強制更新或狀態改變時才更新
        if (forceUpdate || status != lastStatus) {
            if (statusLabel) lv_label_set_text(statusLabel, status.c_str());
            lastStatus = status;
        }
    } else {
        Serial.println("ERROR: dataManager is NULL");
        lv_label_set_text(contentLabel, "ERROR: No data manager");
        if (statusLabel) lv_label_set_text(statusLabel, "ERROR");
    }
}

void DisplayManager::showMatchResult(bool success) {
    currentMode = MODE_RESULT;
    clearScreen();
    
    currentContainer = createContainer(mainScreen);
    
    if (success) {
        titleLabel = createLabel(currentContainer, "MATCH!", COLOR_SUCCESS);
        lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_24, 0);
        
        contentLabel = createLabel(currentContainer, "恭喜找到完美夥伴!", COLOR_SUCCESS);
        showMatchAnimation();
    } else {
        titleLabel = createLabel(currentContainer, "遊戲結束", COLOR_ERROR);
        lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_20, 0);
        
        contentLabel = createLabel(currentContainer, "很遺憾，請再試一次", COLOR_ERROR);
        showErrorAnimation();
    }
    
    lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, -30);
    lv_obj_align(contentLabel, LV_ALIGN_CENTER, 0, 10);
}

void DisplayManager::showMessage(const String& message, lv_color_t color) {
    if (statusLabel) {
        lv_label_set_text(statusLabel, message.c_str());
        lv_obj_set_style_text_color(statusLabel, color, 0);
    }
}

bool DisplayManager::isTouched() {
    return touch->available();
}

void DisplayManager::fadeIn(lv_obj_t* obj, uint32_t duration) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, 0, LV_OPA_COVER);
    lv_anim_set_time(&anim, duration);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_start(&anim);
}

void DisplayManager::showMatchAnimation() {
    // 簡單的縮放動畫
    if (titleLabel) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, titleLabel);
        lv_anim_set_values(&anim, LV_IMG_ZOOM_NONE / 2, LV_IMG_ZOOM_NONE);
        lv_anim_set_time(&anim, 500);
        lv_anim_set_path_cb(&anim, lv_anim_path_bounce);
        lv_anim_start(&anim);
    }
}

void DisplayManager::showErrorAnimation() {
    // 震動效果
    if (titleLabel) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, titleLabel);
        lv_anim_set_values(&anim, -5, 5);
        lv_anim_set_time(&anim, 100);
        lv_anim_set_playback_time(&anim, 100);
        lv_anim_set_repeat_count(&anim, 3);
        lv_anim_start(&anim);
    }
}

void DisplayManager::refreshDisplay() {
    lv_obj_invalidate(mainScreen);
}

// 觸控處理實作
bool DisplayManager::isButtonPressed() {
    // 簡單實作：任何觸控都視為按鈕按下
    return isTouched();
}

void DisplayManager::handleTouch() {
    if (!touch || !touch->available()) return;
    
    // 獲取觸控資料
    int x = touch->data.x;
    int y = touch->data.y;
    byte gestureID = touch->data.gestureID;
    
    Serial.print("Touch detected: x=");
    Serial.print(x);
    Serial.print(", y=");
    Serial.print(y);
    Serial.print(", gesture=");
    Serial.println(gestureID);
}

// LVGL回調函數實作
void displayFlushCallback(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    if (!g_tft) {
        lv_disp_flush_ready(disp_drv);
        return;
    }
    
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    g_tft->startWrite();
    g_tft->setAddrWindow(area->x1, area->y1, w, h);
    g_tft->pushColors((uint16_t*)&color_p->full, w * h, true);
    g_tft->endWrite();
    
    // 重要：告訴LVGL刷新完成
    lv_disp_flush_ready(disp_drv);
}

void touchReadCallback(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    if (!g_touch) return;
    
    bool touched = g_touch->available();
    
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = g_touch->data.x;
        data->point.y = g_touch->data.y;
    }
}
