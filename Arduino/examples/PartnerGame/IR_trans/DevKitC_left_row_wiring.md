### ESP32-S3 DevKitC-1 左側同一排接線說明（IR 傳輸端）

本說明對應 `IR_trans/IR_trans.ino` 目前的 GPIO 設定，便於在 DevKitC-1 左側同一排快速接線。

#### 電源分配
- **BAT**: 3.7V 鋰電池（若使用）
- **3V3**: 供 3.3V 外設（VS1838B、IR LED 驅動端）
- **VSYS**: 3.0–4.2V，可供短段 WS2812B 燈條
- **GND**: 公共地

#### 腳位配置（左側同排）
- **IR 發射 (GPIO18)**: `GPIO18 → 1kΩ → 2N2222 B`，`E → GND`，`C → IR LED 負極`，`IR LED 正極 → 100Ω → 3V3`
- **IR 接收 (GPIO16)**: VS1838B `OUT → GPIO16`，`VCC → 3V3`，`GND → GND`
- **WS2812B (GPIO14)**: `DIN → 330Ω → GPIO14`，`VCC → VSYS`，`GND → GND`

#### 備註
- 發射端以 IRremote 送出 NEC 32-bit，ESP32 內部用 LEDC 產生 **38 kHz** 載波。
- 於 `setup()` 顯式鎖定 `38 kHz` 與約 `33%` duty：`setSendCarrier(38)`, `setSendDutyCycle(33)`。
- 目前程式會固定每 300ms 送出 `CMD_MATCH_REQ`（玩家ID=0），接收端會顯示 "It's Match"。

#### 需要的電阻
- 1 kΩ × 1（2N2222 基極）
- 100 Ω × 1（IR LED 限流）
- 330 Ω × 1（WS2812B 資料線）


