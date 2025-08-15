#define USER_SETUP_ID 207
#define USER_SETUP_INFO "ESP32-S3-Touch-LCD-1.28"

#ifdef ILI9341_DRIVER
#undef ILI9341_DRIVER
#endif

#define GC9A01_DRIVER

// For ESP32 Dev board (only tested with GC9A01 display)
// The hardware SPI can be mapped to any pins

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// MISO 未使用（顯示器為寫入為主），避免與 SCLK 混用
// #define TFT_MISO 12
#define TFT_MOSI 11      // mosi - 11
#define TFT_SCLK 12      // sclk - 12
#define TFT_CS   10      // lcd_cs - 10
#define TFT_DC   9       // lcd_dc - 9
#define TFT_RST  17      // lcd_rst - 17
// 背光接至 3.3V，無需程式控制
//#define TFT_BL	2
//#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT

#define SPI_FREQUENCY  40000000