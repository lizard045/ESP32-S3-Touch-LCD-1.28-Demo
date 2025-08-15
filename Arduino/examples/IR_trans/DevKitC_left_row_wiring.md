### ESP32‑S（DevKitC）接線說明（IR 傳輸端）-

本說明對應 `IR_trans/IR_trans_ESP32S.ino`，針對經典 ESP32‑S DevKitC 腳位（如附圖）。

#### 電源分配
- **BAT**: 3.7V 鋰電池（若使用）
- **3V3**: 供 3.3V 外設（VS1838B、IR LED 驅動端）
- **VSYS**: 3.0–4.2V，可供短段 WS2812B 燈條
- **GND**: 公共地

#### 腳位配置
- **IR 發射 (GPIO18，建議)**: `GPIO23 → 1kΩ → 2N2222 B`，`E → GND`，`C → IR LED 負極`，`IR LED 正極 → 100Ω → 3V3`
  - 若 23 腳被占用，可改用具 LEDC 的腳位：`4/5/12/13/14/15/21/22/23/25/26/27/32/33`。
- （可選）**IR 接收 (任一支援 IO，如 GPIO16/17/21 等)**: VS1838B `OUT → 選定 GPIO`，`VCC → 3V3`，`GND → GND`
- （可選）**WS2812B**: `DIN → 330Ω → 任一普通 IO`，`VCC → 5V 或 VSYS`，`GND → GND`

#### 備註
- 程式以 IRremote 送出 NEC 32-bit，ESP32 內部用 LEDC 產生 **38 kHz** 載波。
- 於 `setup()` 顯式鎖定 `38 kHz` 與約 `33%` duty：`setSendCarrier(38)`, `setSendDutyCycle(33)`。
- 範例會固定每 300ms 送出 `CMD_MATCH_REQ`（玩家ID=0），接收端可顯示配對成功。

#### 需要的電阻
- 1 kΩ × 1（2N2222 基極）
- 100 Ω × 1（IR LED 限流）
- 330 Ω × 1（WS2812B 資料線）


