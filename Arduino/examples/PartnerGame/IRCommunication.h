#ifndef IRCOMMUNICATION_H
#define IRCOMMUNICATION_H

#include <Arduino.h>

// 前向宣告，避免在標頭檔中包含IRremote.h
class IRrecv;
class IRsend;
struct decode_results;

// 紅外線通訊相關常數
#define IR_SEND_PIN     15    // GPIO15 - 紅外線發射腳
#define IR_RECV_PIN     16    // GPIO16 - 紅外線接收腳 (VS1838B)
#define IR_LED_PIN      17    // GPIO17 - 狀態指示LED

// 通訊協定定義
#define IR_ADDRESS      0x1234  // 固定位址
#define IR_TIMEOUT      5000    // 5秒超時

// IRremote 3.9.0 相容性定義將在 .cpp 檔中處理

// 指令定義
enum IRCommand {
    CMD_HANDSHAKE   = 0x01,  // 握手信號
    CMD_PLAYER_ID   = 0x02,  // 玩家ID
    CMD_MATCH_REQ   = 0x03,  // 配對請求
    CMD_MATCH_ACK   = 0x04,  // 配對確認
    CMD_MATCH_FAIL  = 0x05,  // 配對失敗
    CMD_HEARTBEAT   = 0x06,  // 心跳信號
    CMD_RESET       = 0x07   // 重置
};

// 通訊狀態
enum IRCommState {
    STATE_IDLE,         // 空閒
    STATE_SCANNING,     // 掃描中
    STATE_CONNECTING,   // 連接中
    STATE_CONNECTED,    // 已連接
    STATE_MATCHING,     // 配對中
    STATE_ERROR         // 錯誤
};

// 訊息結構
struct IRMessage {
    uint8_t command;
    uint8_t playerId;
    uint16_t data;
    uint32_t timestamp;
    bool isValid;
};

class IRCommunication {
private:
    // 硬體相關
    int m_sendPin;
    int m_recvPin;
    int m_ledPin;
    
    // IRremote 物件
    IRsend* irSender;
    IRrecv* irReceiver;
    decode_results* results;
    
    // 通訊狀態
    IRCommState currentState;
    uint8_t myPlayerId;
    uint8_t targetPlayerId;
    uint32_t lastSendTime;
    uint32_t lastReceiveTime;
    
    // 訊息佇列
    IRMessage messageQueue[10];
    int queueHead;
    int queueTail;
    int queueCount;
    
    // 私有方法
    void initHardware();
    void sendRawCommand(uint8_t command, uint8_t playerId, uint16_t data = 0);
    bool receiveMessage(IRMessage& message);
    void processMessage(const IRMessage& message);
    void updateLED();
    bool isTimeout(uint32_t startTime, uint32_t timeout);
    uint32_t encodeMessage(uint8_t command, uint8_t playerId, uint16_t data);
    bool decodeMessage(uint32_t rawData, IRMessage& message);
    
    // 佇列操作
    void enqueueMessage(const IRMessage& message);
    bool dequeueMessage(IRMessage& message);
    void clearQueue();

public:
    IRCommunication(int send_pin = IR_SEND_PIN, int recv_pin = IR_RECV_PIN, int led_pin = IR_LED_PIN);
    ~IRCommunication();
    
    // 初始化
    bool begin(uint8_t playerId);
    void end();
    
    // 基本通訊
    bool sendHandshake();
    bool sendPlayerID(uint8_t playerId);
    bool sendMatchRequest(uint8_t targetId);
    bool sendMatchResponse(bool success);
    bool sendHeartbeat();
    
    // 高階功能
    bool startScanning();
    bool stopScanning();
    bool connectToPlayer(uint8_t playerId);
    bool performMatch(uint8_t targetId);
    
    // 狀態查詢
    IRCommState getState();
    bool isConnected();
    bool hasNewMessage();
    uint8_t getConnectedPlayerId();
    
    // 訊息處理
    bool getNextMessage(IRMessage& message);
    void update();  // 主循環調用
    
    // 工具方法
    void reset();
    void setPlayerId(uint8_t id);
    uint8_t getPlayerId();
    String getStateString();
    String getCommandString(uint8_t command);
    
    // 除錯功能
    void printMessage(const IRMessage& message);
    void printState();
    bool testConnection();
};

// 工具函數
String irCommandToString(IRCommand cmd);
String irStateToString(IRCommState state);

#endif
