#ifndef PARTNERDATA_H
#define PARTNERDATA_H

#include <Arduino.h>

// 特徵資訊結構體
struct PartnerInfo {
    String partner;        // Single/Not Single
    String pet;           // Have/Don't have
    String badHabit;      // Smoker/Not
    String mbtiEI;        // Extraversion(E)/Introversion(I)
    String mbtiNS;        // Intuition(N)/Sensing(S)
    String mbtiTF;        // Thinking(T)/Feeling(F)
    String mbtiJP;        // Judging(J)/Perceiving(P)
    String gender;        // Male/Female
    String height;        // <=170cm/>170cm
    String accessories;   // Wear glasses/No glasses
    int playerId;         // 玩家ID
    bool isValid;         // 資料是否有效
};

// 遊戲狀態結構體
struct GameState {
    int currentPlayer;         // 當前玩家ID
    int targetPlayer;         // 目標玩家ID
    int errorCount;           // 錯誤次數
    bool hiddenTraits[10];    // 隱藏特徵標記 (true=隱藏, false=顯示)
    bool gameActive;          // 遊戲是否進行中
    bool showResult;          // 是否顯示結果
    bool isMatch;            // 是否配對成功
};

class PartnerDataManager {
private:
    PartnerInfo players[10];  // 最多10個玩家
    int playerCount;
    GameState gameState;
    
    // 特徵名稱對應 (暫時使用英文測試)
    const String traitNames[10] = {
        "Partner", "Pet", "Bad Habit", "E/I", 
        "N/S", "T/F", "J/P", 
        "Gender", "Height", "Accessories"
    };

public:
    PartnerDataManager();
    
    // 資料管理
    bool loadFromCSV(const String& csvData);
    bool parseCSVLine(const String& line, PartnerInfo& player);
    bool addPlayer(const PartnerInfo& player);
    PartnerInfo getPlayer(int playerId);
    int getPlayerCount();
    
    // 遊戲邏輯
    void startGame(int currentPlayerId, int targetPlayerId);
    void initializeHiddenTraits();
    String getVisibleTraits(int playerId);
    String getSingleTrait(int playerId, int traitIndex);  // 新增：獲取單個特徵
    String getTraitName(int traitIndex);
    String getTraitValue(const PartnerInfo& player, int traitIndex);
    bool checkMatch(int playerId1, int playerId2);
    void processWrongMatch();
    void revealRandomTrait();
    
    // 解鎖特徵相關
    int getUnlockedTraitCount();
    int getTotalTraitCount() { return 10; }
    bool isTraitUnlocked(int traitIndex);  // 新增：檢查特徵是否解鎖
    
    // 遊戲狀態
    GameState& getGameState();
    void resetGame();
    bool isGameOver();
    int getMaxErrors() { return 3; }
    
    // 顯示相關
    String formatTraitForDisplay(int traitIndex, const String& value, bool isHidden = false);
};

#endif
