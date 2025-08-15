#ifndef IRCOMMUNICATION_H
#define IRCOMMUNICATION_H

#include <Arduino.h>
#include "Config.h"
// 使用 IRremoteESP8266 做為 IR 收發實作
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

// 紅外線通訊相關常數（與 Config.h 同步）
#ifndef IR_SEND_PIN
#define IR_SEND_PIN     15
#endif
#ifndef IR_RECV_PIN
#define IR_RECV_PIN     7
#endif
#ifndef IR_LED_PIN
#define IR_LED_PIN      17
#endif

// 通訊協定定義
#define IR_ADDRESS      0x1234  // 固定位址
#define IR_TIMEOUT      5000    // 5秒超時


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
    
    // IRremoteESP8266 物件
    IRsend* irsend;
    IRrecv* irrecv;
    
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

    // 配對請求的暫存（用於 4.x 多封包資料合併）
    bool pendingMatchReq;
    uint8_t pendingSenderId;
    uint32_t pendingReqTime;
    
    // 錯誤訊號連擊計數：連續兩次錯誤才累加一次 CR
    uint8_t wrongStreak;
    
    // 私有方法
    void initHardware();
    void sendRawCommand(uint8_t command, uint8_t playerId, uint16_t data = 0);
    void sendDataByte(uint8_t playerId, uint8_t dataByte);
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
    
    // 錯誤/正確信號處理
    void recordWrongSignal();
    void resetWrongStreak();
    // 當達到兩次錯誤時的事件旗標（由主程式讀取後清除）
    volatile bool wrongUnlockEvent;

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
    // 讀取並清除兩次錯誤觸發的解鎖事件（回傳 true 表示本次消耗到事件）
    bool consumeWrongUnlockEvent();
    
    // 除錯功能
    void printMessage(const IRMessage& message);
    void printState();
    bool testConnection();
};

// 工具函數
String irCommandToString(IRCommand cmd);
String irStateToString(IRCommState state);

#endif
