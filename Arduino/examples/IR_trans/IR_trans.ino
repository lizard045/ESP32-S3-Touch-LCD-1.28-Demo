/*
  IR_trans_ESP32S - 單向紅外線發射（940nm LED + 2N2222）

  開發板：ESP32 DevKitC（經典 ESP32-S 系列腳位，如附圖）
  功能：以 IRremoteESP8266 產生 38 kHz 載波（RMT），送 NEC 32-bit 封包
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

// IRremoteESP8266 標頭
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

// 腳位設定（可依實際需要修改為其他 LEDC 支援腳）
static const int PIN_IR_TX = 23;
static const int STATUS_LED_PIN = 2;   // 板載 LED（若無此腳會被忽略）
static const int PIN_IR_RX = 21;       // VS1838B OUT 接收腳（自我對測）

// IR 物件
IRsend irsend(PIN_IR_TX);
IRrecv irrecv(PIN_IR_RX);
decode_results results;

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

// 傳送節奏與測試控制
static uint32_t sendIntervalMs = 3000;
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
  // 採用兩封包方案（NEC 32位）：
  // 封包1：address16 高8=IR_ADDRESS>>8，低8=playerId；command=IRCommand，最後 8 位為 ~command
  uint16_t address16 = ((uint16_t)(IR_ADDRESS & 0xFF00)) | g_myPlayerId;
  if (cmd == CMD_MATCH_REQ) {
    uint8_t c = (uint8_t)CMD_MATCH_REQ;
    uint32_t frame1 = ((uint32_t)address16 << 16) | ((uint32_t)c << 8) | ((uint32_t)(~c) & 0xFF);
    irsend.sendNEC(frame1, 32, 0);
    delay(30);
    // 封包2：以 1-byte 資料（targetId 或 extra 低 8 位）放在 command 欄位
    uint8_t d = (uint8_t)(extra & 0xFF);
    uint32_t frame2 = ((uint32_t)address16 << 16) | ((uint32_t)d << 8) | ((uint32_t)(~d) & 0xFF);
    irsend.sendNEC(frame2, 32, 0);
  } else {
    uint8_t c = (uint8_t)cmd;
    uint32_t frame = ((uint32_t)address16 << 16) | ((uint32_t)c << 8) | ((uint32_t)(~c) & 0xFF);
    irsend.sendNEC(frame, 32, 0);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // 初始化 IR 傳送/接收
  irsend.begin();

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  // 啟動接收（自測）
  irrecv.enableIRIn();
  Serial.print("IR RX ready @GPIO");
  Serial.println(PIN_IR_RX);

  Serial.println("IR TX ready: ESP32-S @GPIO23, NEC 32-bit (IRremoteESP8266)");
  Serial.println("Commands: 'c' toggle carrier test (0.5s on/off). Normal mode sends NEC every 300ms.");
  Serial.println("Tip: Point your TATUNG AC remote to the receiver to see decoded or RAW output.");
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
        // 離開測試模式：關閉載波並把 PIN 交回 IR 發送
        ledcWrite(CARRIER_CH, 0);
        ledcDetachPin(PIN_IR_TX);
        // 還原 IR 發送（RMT）
        irsend.begin();
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

  // 接收列印（詳細 + RAW，便於空調遙控器分析）
  if (irrecv.decode(&results)) {
    // 基本可讀資訊
    Serial.println(resultToHumanReadableBasic(&results));
    // 輸出 RAW 訊號（受限於快取長度）
    Serial.println(resultToTimingInfo(&results));
    Serial.println();
    irrecv.resume();
  }
}
