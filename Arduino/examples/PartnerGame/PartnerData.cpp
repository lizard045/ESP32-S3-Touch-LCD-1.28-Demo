#include "PartnerData.h"

PartnerDataManager::PartnerDataManager() {
    playerCount = 0;
    resetGame();
}

bool PartnerDataManager::loadFromCSV(const String& csvData) {
    // 重置玩家計數
    playerCount = 0;
    
    // 分割CSV行
    int startPos = 0;
    int lineCount = 0;
    
    while (startPos < csvData.length()) {
        int endPos = csvData.indexOf('\n', startPos);
        if (endPos == -1) endPos = csvData.length();
        
        String line = csvData.substring(startPos, endPos);
        line.trim();
        
        // 跳過標題行
        if (lineCount > 0 && line.length() > 0 && playerCount < 10) {
            PartnerInfo tempPlayer;
            if (parseCSVLine(line, tempPlayer)) {
                players[playerCount] = tempPlayer;
                players[playerCount].playerId = playerCount;
                players[playerCount].isValid = true;
                playerCount++;
            }
        }
        
        lineCount++;
        startPos = endPos + 1;
    }
    
    return playerCount > 0;
}

bool PartnerDataManager::parseCSVLine(const String& line, PartnerInfo& player) {
    String fields[11]; // 包含時間戳記共11個欄位
    int fieldIndex = 0;
    int startPos = 0;
    
    // 解析CSV欄位
    while (startPos < line.length() && fieldIndex < 11) {
        int commaPos = line.indexOf(',', startPos);
        if (commaPos == -1) commaPos = line.length();
        
        String field = line.substring(startPos, commaPos);
        field.trim();
        fields[fieldIndex] = field;
        
        fieldIndex++;
        startPos = commaPos + 1;
    }
    
    // 確保有足夠的欄位
    if (fieldIndex < 11) return false;
    
    // 指派到結構體 (跳過時間戳記，從索引1開始)
    player.partner = fields[1];
    player.pet = fields[2];
    player.badHabit = fields[3];
    player.mbtiEI = fields[4];
    player.mbtiNS = fields[5];
    player.mbtiTF = fields[6];
    player.mbtiJP = fields[7];
    player.gender = fields[8];
    player.height = fields[9];
    player.accessories = fields[10];
    
    return true;
}

bool PartnerDataManager::addPlayer(const PartnerInfo& player) {
    if (playerCount >= 10) return false;
    
    players[playerCount] = player;
    players[playerCount].playerId = playerCount;
    players[playerCount].isValid = true;
    playerCount++;
    
    return true;
}

PartnerInfo PartnerDataManager::getPlayer(int playerId) {
    if (playerId >= 0 && playerId < playerCount) {
        return players[playerId];
    }
    
    PartnerInfo empty;
    empty.isValid = false;
    return empty;
}

int PartnerDataManager::getPlayerCount() {
    return playerCount;
}

void PartnerDataManager::startGame(int currentPlayerId, int targetPlayerId) {
    gameState.currentPlayer = currentPlayerId;
    gameState.targetPlayer = targetPlayerId;
    gameState.errorCount = 0;
    gameState.gameActive = true;
    gameState.showResult = false;
    gameState.isMatch = false;
    
    initializeHiddenTraits();
}

void PartnerDataManager::initializeHiddenTraits() {
    // 初始化前5個特徵為顯示狀態，後5個為隱藏狀態
    for (int i = 0; i < 10; i++) {
        if (i < 5) {
            gameState.hiddenTraits[i] = false; // 前5個特徵顯示 (Partner, Pet, Bad Habit, E/I, N/S)
        } else {
            gameState.hiddenTraits[i] = true;  // 後5個特徵隱藏 (T/F, J/P, Gender, Height, Accessories)
        }
    }
}

String PartnerDataManager::getVisibleTraits(int playerId) {
    if (playerId < 0 || playerId >= playerCount) {
        return "無效的玩家ID";
    }
    
    PartnerInfo player = players[playerId];
    
    // 在左上角顯示解鎖進度
    int unlockedCount = getUnlockedTraitCount();
    int totalCount = getTotalTraitCount();
    String result = "CR : (" + String(unlockedCount) + "/" + String(totalCount) + ")\n\n";
    result += "Partner Traits:\n\n";
    
    for (int i = 0; i < 10; i++) {
        String traitValue = getTraitValue(player, i);
        result += formatTraitForDisplay(i, traitValue, gameState.hiddenTraits[i]);
        result += "\n";
    }
    
    
    return result;
}

String PartnerDataManager::getTraitName(int traitIndex) {
    if (traitIndex >= 0 && traitIndex < 10) {
        return traitNames[traitIndex];
    }
    return "未知特徵";
}

String PartnerDataManager::getTraitValue(const PartnerInfo& player, int traitIndex) {
    switch (traitIndex) {
        case 0: return player.partner;
        case 1: return player.pet;
        case 2: return player.badHabit;
        case 3: return player.mbtiEI;
        case 4: return player.mbtiNS;
        case 5: return player.mbtiTF;
        case 6: return player.mbtiJP;
        case 7: return player.gender;
        case 8: return player.height;
        case 9: return player.accessories;
        default: return "未知";
    }
}

bool PartnerDataManager::checkMatch(int playerId1, int playerId2) {
    if (playerId1 < 0 || playerId1 >= playerCount || 
        playerId2 < 0 || playerId2 >= playerCount) {
        return false;
    }
    
    // 在這個遊戲中，每個人都有自己獨特的ID
    // 配對成功意味著找到了正確的目標玩家
    return playerId1 == playerId2;
}

void PartnerDataManager::processWrongMatch() {
    gameState.errorCount++;
    
    // 如果還有錯誤機會，揭露一個隱藏特徵
    if (gameState.errorCount < getMaxErrors()) {
        revealRandomTrait();
    }
    
    // 檢查遊戲是否結束
    if (gameState.errorCount >= getMaxErrors()) {
        gameState.gameActive = false;
        gameState.showResult = true;
        gameState.isMatch = false;
    }
}

void PartnerDataManager::revealRandomTrait() {
    // 找到所有隱藏的特徵
    int hiddenTraits[10];
    int hiddenCount = 0;
    
    for (int i = 0; i < 10; i++) {
        if (gameState.hiddenTraits[i]) {
            hiddenTraits[hiddenCount] = i;
            hiddenCount++;
        }
    }
    
    // 如果有隱藏的特徵，隨機揭露一個
    if (hiddenCount > 0) {
        int randomIndex = random(0, hiddenCount);
        int traitToReveal = hiddenTraits[randomIndex];
        gameState.hiddenTraits[traitToReveal] = false;
    }
}

String PartnerDataManager::getSingleTrait(int playerId, int traitIndex) {
    if (playerId < 0 || playerId >= playerCount || traitIndex < 0 || traitIndex >= 10) {
        return "Invalid";
    }
    
    PartnerInfo player = players[playerId];
    String traitValue = getTraitValue(player, traitIndex);
    
    return formatTraitForDisplay(traitIndex, traitValue, gameState.hiddenTraits[traitIndex]);
}

bool PartnerDataManager::isTraitUnlocked(int traitIndex) {
    if (traitIndex < 0 || traitIndex >= 10) return false;
    return !gameState.hiddenTraits[traitIndex];
}

int PartnerDataManager::getUnlockedTraitCount() {
    int unlockedCount = 0;
    for (int i = 0; i < 10; i++) {
        if (!gameState.hiddenTraits[i]) {
            unlockedCount++;
        }
    }
    return unlockedCount;
}

GameState& PartnerDataManager::getGameState() {
    return gameState;
}

void PartnerDataManager::resetGame() {
    gameState.currentPlayer = -1;
    gameState.targetPlayer = -1;
    gameState.errorCount = 0;
    gameState.gameActive = false;
    gameState.showResult = false;
    gameState.isMatch = false;
    
    for (int i = 0; i < 10; i++) {
        gameState.hiddenTraits[i] = false;
    }
}

bool PartnerDataManager::isGameOver() {
    return !gameState.gameActive && gameState.showResult;
}

String PartnerDataManager::formatTraitForDisplay(int traitIndex, const String& value, bool isHidden) {
    String traitName = getTraitName(traitIndex);
    
    if (isHidden) {
        return traitName + ": ???";
    } else {
        return traitName + ":\n" + value;
    }
}
