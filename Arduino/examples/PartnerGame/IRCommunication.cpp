#include "IRCommunication.h"

#include "PartnerData.h"

extern PartnerDataManager dataManager;

IRCommunication::IRCommunication(int send_pin, int recv_pin, int led_pin) {
    m_sendPin = send_pin;
    m_recvPin = recv_pin;
    m_ledPin = led_pin;
    irsend = nullptr;
    irrecv = nullptr;
    
    currentState = STATE_IDLE;
    myPlayerId = 0;
    targetPlayerId = 0;
    lastSendTime = 0;
    lastReceiveTime = 0;
    
    // 初始化佇列
    queueHead = 0;
    queueTail = 0;
    queueCount = 0;

    // 多封包暫存
    pendingMatchReq = false;
    pendingSenderId = 0;
    pendingReqTime = 0;
}

IRCommunication::~IRCommunication() {
    end();
}

bool IRCommunication::begin(uint8_t playerId) {
    myPlayerId = playerId;
    
    // 初始化硬體
    initHardware();
    
    // 初始化 IRremoteESP8266 物件
    if (!irsend) {
        irsend = new IRsend(m_sendPin);
    }
    irsend->begin();
    if (!irrecv) {
        // 使用預設緩衝與 timeout，開啟 RMT 接收
        irrecv = new IRrecv(m_recvPin);
    }
    irrecv->enableIRIn();
    
    currentState = STATE_IDLE;
    clearQueue();
    
    Serial.println("IRCommunication: 初始化完成");
    Serial.print("玩家ID: ");
    Serial.println(myPlayerId);
    
    return true;
}

void IRCommunication::end() {
    // 可選：停用接收
    if (irrecv) {
        // IRremoteESP8266 沒有明確 disable API，保留指標
    }
    currentState = STATE_IDLE;
}

void IRCommunication::initHardware() {
    // 設置腳位
    pinMode(m_sendPin, OUTPUT);
    pinMode(m_recvPin, INPUT);
    pinMode(m_ledPin, OUTPUT);
    
    // 初始化LED
    digitalWrite(m_ledPin, LOW);
    
    Serial.print("IR發射腳: GPIO");
    Serial.println(m_sendPin);
    Serial.print("IR接收腳: GPIO");
    Serial.println(m_recvPin);
    Serial.print("狀態LED: GPIO");
    Serial.println(m_ledPin);
}

bool IRCommunication::sendHandshake() {
    sendRawCommand(CMD_HANDSHAKE, myPlayerId);
    Serial.println("發送握手信號");
    return true;
}

bool IRCommunication::sendPlayerID(uint8_t playerId) {
    sendRawCommand(CMD_PLAYER_ID, playerId);
    Serial.print("發送玩家ID: ");
    Serial.println(playerId);
    return true;
}

bool IRCommunication::sendMatchRequest(uint8_t targetId) {
    targetPlayerId = targetId;
    // 先送 MATCH_REQ，再用第二包送目標 ID（以 CMD_PLAYER_ID 攜帶）
    sendRawCommand(CMD_MATCH_REQ, myPlayerId);
    sendDataByte(myPlayerId, targetId);
    currentState = STATE_MATCHING;
    Serial.print("發送配對請求給玩家: ");
    Serial.println(targetId);
    return true;
}

bool IRCommunication::sendMatchResponse(bool success) {
    uint8_t command = success ? CMD_MATCH_ACK : CMD_MATCH_FAIL;
    sendRawCommand(command, myPlayerId);
    
    Serial.print("發送配對回應: ");
    Serial.println(success ? "成功" : "失敗");
    return true;
}

bool IRCommunication::sendHeartbeat() {
    sendRawCommand(CMD_HEARTBEAT, myPlayerId);
    return true;
}

void IRCommunication::sendRawCommand(uint8_t command, uint8_t playerId, uint16_t data) {
    // 以 NEC 32 位：[addr16][cmd8][~cmd8]
    uint16_t address16 = ((uint16_t)(IR_ADDRESS & 0xFF00)) | playerId;
    uint8_t cmd8 = command;
    uint32_t frame = ((uint32_t)address16 << 16) | ((uint32_t)cmd8 << 8) | ((uint32_t)(~cmd8) & 0xFF);
    if (irsend) {
        irsend->sendNEC(frame, 32, 0);
    }
    lastSendTime = millis();
    updateLED();
    delay(50);  // 短暫延遲避免衝突
}

void IRCommunication::sendDataByte(uint8_t playerId, uint8_t dataByte) {
    // 使用 NEC，將 1-byte 資料放在 command 欄位
    uint16_t address16 = ((uint16_t)(IR_ADDRESS & 0xFF00)) | playerId;
    uint8_t cmd8 = dataByte;
    uint32_t frame = ((uint32_t)address16 << 16) | ((uint32_t)cmd8 << 8) | ((uint32_t)(~cmd8) & 0xFF);
    if (irsend) {
        irsend->sendNEC(frame, 32, 0);
    }
    lastSendTime = millis();
    updateLED();
    delay(30);
}

bool IRCommunication::receiveMessage(IRMessage& message) {
    decode_results results;
    if (!irrecv || !irrecv->decode(&results)) {
        return false;
    }
    
    // 僅處理 NEC
    if (results.decode_type != decode_type_t::NEC) {
        irrecv->resume();
        dataManager.processWrongMatch();
        return false;
    }
    uint32_t v = (uint32_t)results.value;
    uint16_t addr16 = (uint16_t)((v >> 16) & 0xFFFF);
    uint8_t senderId = (uint8_t)(addr16 & 0xFF);
    uint8_t cmd = (uint8_t)((v >> 8) & 0xFF);

    bool produced = false;
    // 多封包合併：先收到 MATCH_REQ，等待下一個來自同 sender 的任意封包之 command 當作 1-byte data
    if (cmd == CMD_MATCH_REQ) {
        pendingMatchReq = true;
        pendingSenderId = senderId;
        pendingReqTime = millis();
        produced = false; // 先不產生訊息，等待 data
    } else if (pendingMatchReq && pendingSenderId == senderId && (millis() - pendingReqTime) < 600) {
        // 合併為完整 MATCH_REQ，將本封包之 command 當作資料位元組
        message.command = CMD_MATCH_REQ;
        message.playerId = senderId;
        message.data = cmd; // 1-byte 資料
        message.timestamp = millis();
        message.isValid = true;
        lastReceiveTime = message.timestamp;
        produced = true;
        // 清除暫存
        pendingMatchReq = false;
    } else {
        // 其他單包指令：直接產生訊息
        message.command = cmd;
        message.playerId = senderId;
        message.data = 0;
        message.timestamp = millis();
        message.isValid = true;
        lastReceiveTime = message.timestamp;
        produced = true;
    }

    irrecv->resume();
    if (!produced) {
        return false;
    }
    return true;
}

void IRCommunication::update() {
    // 接收訊息
    IRMessage newMessage;
    if (receiveMessage(newMessage)) {
        enqueueMessage(newMessage);
        processMessage(newMessage);
    }

    // 更新LED狀態
    updateLED();

    // 檢查超時
    if (currentState == STATE_CONNECTING || currentState == STATE_MATCHING) {
        if (isTimeout(lastSendTime, IR_TIMEOUT)) {
            Serial.println("通訊超時");
            currentState = STATE_ERROR;
        }
    }
}

void IRCommunication::processMessage(const IRMessage& message) {
    Serial.print("接收到訊息 - 指令: ");
    Serial.print(getCommandString(message.command));
    Serial.print(", 玩家ID: ");
    Serial.println(message.playerId);
    
    switch (message.command) {
        case CMD_HANDSHAKE:
            // 回應握手
            sendPlayerID(myPlayerId);
            currentState = STATE_CONNECTING;
            break;
            
        case CMD_PLAYER_ID:
            if (currentState == STATE_SCANNING || currentState == STATE_CONNECTING) {
                targetPlayerId = message.playerId;
                currentState = STATE_CONNECTED;
                Serial.print("與玩家連接: ");
                Serial.println(targetPlayerId);
            }
            break;
            
        case CMD_MATCH_REQ:
            // 處理配對請求
            if (message.data == myPlayerId) {
                // 正確的配對
                sendMatchResponse(true);
                Serial.println("配對成功!");
            } else {
                // 錯誤的配對:累加錯誤次數
                sendMatchResponse(false);
                Serial.println("配對失敗!");
                dataManager.processWrongMatch();
            }
            break;
            
        case CMD_MATCH_ACK:
            if (currentState == STATE_MATCHING) {
                currentState = STATE_CONNECTED;
                Serial.println("對方確認配對成功!");
            }
            break;
            
        case CMD_MATCH_FAIL:
            if (currentState == STATE_MATCHING) {
                currentState = STATE_CONNECTED;
                Serial.println("對方回應配對失敗!");
            }
            break;
            
        case CMD_HEARTBEAT:
            // 更新最後接收時間
            lastReceiveTime = millis();
            break;
            
        case CMD_RESET:
            reset();
            break;
    }
}

bool IRCommunication::startScanning() {
    currentState = STATE_SCANNING;
    clearQueue();
    
    Serial.println("開始掃描其他玩家...");
    
    // 發送握手信號
    sendHandshake();
    
    return true;
}

bool IRCommunication::stopScanning() {
    if (currentState == STATE_SCANNING) {
        currentState = STATE_IDLE;
        Serial.println("停止掃描");
        return true;
    }
    return false;
}

bool IRCommunication::connectToPlayer(uint8_t playerId) {
    targetPlayerId = playerId;
    currentState = STATE_CONNECTING;
    
    sendHandshake();
    
    Serial.print("嘗試連接到玩家: ");
    Serial.println(playerId);
    
    return true;
}

bool IRCommunication::performMatch(uint8_t targetId) {
    if (currentState != STATE_CONNECTED) {
        Serial.println("未連接，無法進行配對");
        return false;
    }
    
    return sendMatchRequest(targetId);
}

IRCommState IRCommunication::getState() {
    return currentState;
}

bool IRCommunication::isConnected() {
    return currentState == STATE_CONNECTED;
}

bool IRCommunication::hasNewMessage() {
    return queueCount > 0;
}

uint8_t IRCommunication::getConnectedPlayerId() {
    return targetPlayerId;
}

bool IRCommunication::getNextMessage(IRMessage& message) {
    return dequeueMessage(message);
}

void IRCommunication::reset() {
    currentState = STATE_IDLE;
    targetPlayerId = 0;
    clearQueue();
    
    Serial.println("IR通訊重置");
}

void IRCommunication::setPlayerId(uint8_t id) {
    myPlayerId = id;
}

uint8_t IRCommunication::getPlayerId() {
    return myPlayerId;
}

String IRCommunication::getStateString() {
    return irStateToString(currentState);
}

String IRCommunication::getCommandString(uint8_t command) {
    return irCommandToString((IRCommand)command);
}

void IRCommunication::updateLED() {
    static uint32_t lastBlink = 0;
    static bool ledState = false;
    
    uint32_t now = millis();
    
    switch (currentState) {
        case STATE_IDLE:
            digitalWrite(m_ledPin, LOW);
            break;
            
        case STATE_SCANNING:
        case STATE_CONNECTING:
        case STATE_MATCHING:
            // 快速閃爍
            if (now - lastBlink > 200) {
                ledState = !ledState;
                digitalWrite(m_ledPin, ledState);
                lastBlink = now;
            }
            break;
            
        case STATE_CONNECTED:
            digitalWrite(m_ledPin, HIGH);
            break;
            
        case STATE_ERROR:
            // 慢速閃爍
            if (now - lastBlink > 1000) {
                ledState = !ledState;
                digitalWrite(m_ledPin, ledState);
                lastBlink = now;
            }
            break;
    }
}

bool IRCommunication::isTimeout(uint32_t startTime, uint32_t timeout) {
    return (millis() - startTime) > timeout;
}

uint32_t IRCommunication::encodeMessage(uint8_t command, uint8_t playerId, uint16_t data) {
    // 編碼格式: [指令8位][玩家ID8位][資料16位]
    uint32_t encoded = 0;
    encoded |= (uint32_t)command << 24;
    encoded |= (uint32_t)playerId << 16;
    encoded |= (uint32_t)data;
    return encoded;
}

bool IRCommunication::decodeMessage(uint32_t rawData, IRMessage& message) {
    message.command = (rawData >> 24) & 0xFF;
    message.playerId = (rawData >> 16) & 0xFF;
    message.data = rawData & 0xFFFF;
    message.timestamp = millis();
    message.isValid = true;
    
    // 驗證指令範圍
    if (message.command < CMD_HANDSHAKE || message.command > CMD_RESET) {
        message.isValid = false;
        return false;
    }
    
    return true;
}

void IRCommunication::enqueueMessage(const IRMessage& message) {
    if (queueCount >= 10) {
        // 佇列滿了，移除最舊的訊息
        queueHead = (queueHead + 1) % 10;
        queueCount--;
    }
    
    messageQueue[queueTail] = message;
    queueTail = (queueTail + 1) % 10;
    queueCount++;
}

bool IRCommunication::dequeueMessage(IRMessage& message) {
    if (queueCount == 0) return false;
    
    message = messageQueue[queueHead];
    queueHead = (queueHead + 1) % 10;
    queueCount--;
    
    return true;
}

void IRCommunication::clearQueue() {
    queueHead = 0;
    queueTail = 0;
    queueCount = 0;
}

void IRCommunication::printMessage(const IRMessage& message) {
    Serial.print("訊息 - 指令: ");
    Serial.print(getCommandString(message.command));
    Serial.print(", 玩家: ");
    Serial.print(message.playerId);
    Serial.print(", 資料: ");
    Serial.print(message.data);
    Serial.print(", 時間: ");
    Serial.println(message.timestamp);
}

void IRCommunication::printState() {
    Serial.print("IR狀態: ");
    Serial.print(getStateString());
    Serial.print(", 我的ID: ");
    Serial.print(myPlayerId);
    Serial.print(", 目標ID: ");
    Serial.println(targetPlayerId);
}

bool IRCommunication::testConnection() {
    Serial.println("測試IR連接...");
    sendHeartbeat();
    delay(100);
    // 在 v4 僅做簡單送出測試
    Serial.println("IR 4.x 發送測試已觸發");
    return true;
}

//（已移除重複且舊版的 update() 實作）

// 工具函數實作
String irCommandToString(IRCommand cmd) {
    switch (cmd) {
        case CMD_HANDSHAKE: return "握手";
        case CMD_PLAYER_ID: return "玩家ID";
        case CMD_MATCH_REQ: return "配對請求";
        case CMD_MATCH_ACK: return "配對確認";
        case CMD_MATCH_FAIL: return "配對失敗";
        case CMD_HEARTBEAT: return "心跳";
        case CMD_RESET: return "重置";
        default: return "未知";
    }
}

String irStateToString(IRCommState state) {
    switch (state) {
        case STATE_IDLE: return "空閒";
        case STATE_SCANNING: return "掃描中";
        case STATE_CONNECTING: return "連接中";
        case STATE_CONNECTED: return "已連接";
        case STATE_MATCHING: return "配對中";
        case STATE_ERROR: return "錯誤";
        default: return "未知";
    }
}
