/*
  IR_trans - ESP32-S3 傳輸端 (LEDC 38kHz + NEC 32-bit 擴展)

  硬體對應 (ESP32-S3-N16R8 圓形LCD板)：
  - 紅外線發射 (2N2222 + 940nm LED)
    GPIO18 -> 1kΩ -> 2N2222 B，E -> GND，C -> IR LED 負極，IR LED 正極 -> 100Ω -> 3V3
  - 紅外線接收 VS1838B：VCC->3V3、GND->GND、OUT->GPIO16
  - WS2812B：VCC->VSYS、GND->GND、DIN->(330Ω)->GPIO14  // DevKitC-1 左側同排
  - 供電：BAT 接 3.7V 鋰電池，3V3 供 3.3V 外設，VSYS 供短燈條(3.0–4.2V)

  本範例只實作發射端，採用 LEDC 產生 38kHz 載波並以 NEC 擴展(32-bit)格式：
  [address 16-bit LSB-first][data 16-bit LSB-first]
  receiver 端在 PartnerGame/ 以 IRremote 解析 (address = 0x1234)。
*/

#include <Arduino.h>
#include <IRremote.h>

// ---- 腳位設定（DevKitC-1 左側同一排）----
static const int PIN_IR_TX = 18;   // 經 1kΩ 連到 2N2222 基極（左側）
static const int PIN_IR_RX = 16;   // VS1838B OUT（左側）
static const int PIN_WS2812 = 14;  // WS2812B 資料腳（左側，同排）

// ---- 通訊協定 (與 PartnerGame 對應) ----
#define IR_ADDRESS 0x1234

enum IRCommand : uint8_t {
  CMD_HANDSHAKE  = 0x01,
  CMD_PLAYER_ID  = 0x02,
  CMD_MATCH_REQ  = 0x03,
  CMD_MATCH_ACK  = 0x04,
  CMD_MATCH_FAIL = 0x05,
  CMD_HEARTBEAT  = 0x06,
  CMD_RESET      = 0x07
};

// 玩家ID：寫死為 0，與接收端預設一致，確保顯示 "It's Match"
static uint8_t g_myPlayerId = 0;

IRsend irsend;

// 與 PartnerGame 相容的 32-bit payload 編碼
static uint32_t encodeMessage(uint8_t command, uint8_t playerId, uint16_t extra) {
  uint32_t m = 0;
  m |= (uint32_t)command << 24;
  m |= (uint32_t)playerId << 16;
  m |= (uint32_t)extra;
  return m;
}

static void sendCommand(IRCommand cmd, uint16_t extra = 0) {
  uint32_t message = encodeMessage((uint8_t)cmd, g_myPlayerId, extra);
  // 以 IRremote 的 32-bit NEC 送法，address 與接收端一致
  irsend.sendNEC(IR_ADDRESS, message, 0);
}

void setup() {
  Serial.begin(115200);

  // IRremote 會在 ESP32 上使用 LEDC 產生 38kHz 載波
  irsend.begin(PIN_IR_TX);
  // 顯式鎖定 38kHz 與約 33% duty，避免庫預設被改動
  irsend.setSendCarrier(38);       // 單位 kHz
  irsend.setSendDutyCycle(33);     // 百分比

  Serial.println("IR TX 初始化完成 (IRremote NEC 32-bit)");
  Serial.println("本發射端將每 300ms 送出配對請求，確保接收端顯示 MATCH");
}

void loop() {
  // 固定以自己為目標送出配對請求，確保接收端顯示 MATCH
  sendCommand(CMD_MATCH_REQ, g_myPlayerId);
  delay(300);
}
