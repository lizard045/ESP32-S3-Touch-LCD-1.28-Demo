/*
  IR_trans_ESP32S - 單向紅外線發射（940nm LED + 2N2222）

  開發板：ESP32 DevKitC（經典 ESP32-S 系列腳位，如附圖）
  功能：以 IRremote 產生 38 kHz 載波，送 NEC 32-bit 封包
        [command:8][playerId:8][extra:16]，address 固定 0x1234

  硬體連接（單純發射，不含接收器）：
  - IR 發射腳（GPIO23）→ 1kΩ → 2N2222 基極
  - 2N2222：E → GND，C → 紅外線 LED 負極
  - 紅外線 LED 正極 → 100Ω → 3V3
  - 所有地共地

  備註：GPIO23 為 VSPI_MOSI，支援 LEDC，適合作為載波輸出；若板上 SPI 佔用，
        亦可改用 4/5/12/13/14/15/18/21/22/25/26/27/32/33 等支援 LEDC 的腳。
*/

#include <Arduino.h>
#include <IRremote.h>

// 腳位設定（可依實際需要修改為其他 LEDC 支援腳）
static const int PIN_IR_TX = 23;
static const int STATUS_LED_PIN = 2;   // 板載 LED（若無此腳會被忽略）
static const int PIN_IR_RX = 21;       // VS1838B OUT 接收腳（自我對測）

// 通訊協定（需與接收端一致）
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

static uint8_t g_myPlayerId = 0;  // 單機發射端固定 ID

IRsend irsend;
#if 1
// IRremote 傳統 IRrecv 介面（相容多數版本）
#include <IRremote.h>
IRrecv irrecv(PIN_IR_RX);
decode_results irResults;
#endif

// 傳送節奏與測試控制
static uint32_t sendIntervalMs = 300;
static unsigned long lastSendMs = 0;
static uint32_t sendCount = 0;
static bool carrierTestMode = false;  // 序列埠輸入 'c' 切換
static const int CARRIER_CH = 2;      // LEDC 測試使用的通道（避免與 IRremote 衝突）

static uint32_t encodeMessage(uint8_t command, uint8_t playerId, uint16_t extra) {
  uint32_t m = 0;
  m |= (uint32_t)command << 24;
  m |= (uint32_t)playerId << 16;
  m |= (uint32_t)extra;
  return m;
}

static void sendCommand(IRCommand cmd, uint16_t extra = 0) {
  uint32_t message = encodeMessage((uint8_t)cmd, g_myPlayerId, extra);
  irsend.sendNEC(IR_ADDRESS, message, 0);
}

void setup() {
  Serial.begin(921600);

  // 以 LEDC 輸出 38 kHz 載波
  irsend.begin(PIN_IR_TX);
  // 使用 IRremote 既有預設（NEC 在 ESP32 會使用 38kHz 載波）

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  // 啟動接收（自測）
  irrecv.enableIRIn();
  Serial.print("IR RX ready @GPIO");
  Serial.println(PIN_IR_RX);

  Serial.println("IR TX ready: ESP32-S @GPIO23, NEC 32-bit");
  Serial.println("Commands: 'c' toggle carrier test (0.5s on/off). Normal mode sends NEC every 300ms.");
}

void loop() {
  // 序列埠指令處理
  if (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == 'c' || ch == 'C') {
      carrierTestMode = !carrierTestMode;
      if (carrierTestMode) {
        // 進入連續載波測試模式：將 PIN 交給 LEDC
        ledcSetup(CARRIER_CH, 38000, 8);
        ledcAttachPin(PIN_IR_TX, CARRIER_CH);
        Serial.println("Carrier test: ON (toggle every 500ms)");
      } else {
        // 離開測試模式：關閉載波並把 PIN 交回 IRremote
        ledcWrite(CARRIER_CH, 0);
        ledcDetachPin(PIN_IR_TX);
        irsend.begin(PIN_IR_TX);
        Serial.println("Carrier test: OFF, back to NEC sender");
        lastSendMs = 0; // 立即觸發一次傳送
      }
    }
  }

  if (carrierTestMode) {
    static unsigned long lastToggle = 0;
    static bool on = false;
    unsigned long now = millis();
    if (now - lastToggle >= 500) {
      on = !on;
      lastToggle = now;
      ledcWrite(CARRIER_CH, on ? 128 : 0); // ~50% duty 連續載波
      digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
    }
    // 測試模式下仍可接收，所以下方的接收列印不 return
  }

  // 正常模式：每 sendIntervalMs 發一次，板載 LED 同步閃爍
  unsigned long now = millis();
  if (now - lastSendMs >= sendIntervalMs) {
    lastSendMs = now;
    digitalWrite(STATUS_LED_PIN, HIGH);
    sendCommand(CMD_MATCH_REQ, g_myPlayerId);
    digitalWrite(STATUS_LED_PIN, LOW);
    sendCount++;
  }

  // 每 2 秒報一次傳送次數
  static unsigned long lastReport = 0;
  if (now - lastReport >= 2000) {
    lastReport = now;
    Serial.print("IR sends: ");
    Serial.println(sendCount);
  }

  // 接收列印（NEC）
  if (irrecv.decode(&irResults)) {
    // 簡要輸出結果
    Serial.print("RX: type=");
    Serial.print(irResults.decode_type);
    Serial.print(", addr=0x");
    Serial.print(irResults.address, HEX);
    Serial.print(", value=0x");
    Serial.print(irResults.value, HEX);
    Serial.print(", bits=");
    Serial.println(irResults.bits);
    irrecv.resume();
  }
}


