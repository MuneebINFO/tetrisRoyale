#pragma once
#include <pthread.h>
#include <termios.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>

#include "../../common/CONSTANT.h"
#include "../../common/header.h"
#include "Signal.h"

class IView;
class IController;
struct Player;
class Server;

struct Tetramino {
    int x_, y_;  // position de départ
    std::vector<std::vector<int>> shape_;
    std::uint8_t colorIndex_;

    Tetramino() : x_(0), y_(0), shape_(4, std::vector<int>(4, 0)), colorIndex_(0) {}

    Tetramino(int x, int y, std::vector<std::vector<int>> shape,
              std::uint8_t colorIndex)
        : x_(x), y_(y), shape_(shape), colorIndex_(colorIndex) {}
};

class Board {
   private:
    bool hasGrid_;
    std::vector<std::vector<std::uint8_t>> grid_;
    std::map<int, std::vector<std::vector<std::uint8_t>>> boards_;

   public:
    Board() = default;
    Board(std::vector<int> idPlayers, bool hasGrid = false);
    std::vector<std::vector<std::uint8_t>>& getGrid() { return grid_; }
    std::map<int, std::vector<std::vector<std::uint8_t>>>& getBoards() {
        return boards_;
    }
    void showBoard();
    void showBoards();
};

class Game {
   private:
    bool running_;
    bool paused_;
    int points_;
    int energy = 0;
    Signal& signal_;
    std::shared_ptr<Player> player_;
    std::shared_ptr<IView> view_;
    std::shared_ptr<IController> controller_;
    std::shared_ptr<Server> server_;
    Tetramino current_;
    pthread_mutex_t mutexTetramino_;
    pthread_mutex_t mutexGrid_;
    pthread_mutex_t mutexStateM;
    pthread_cond_t mutexStateC;
    Board board_;
    std::string gameMode_;
    bool blockedInput_ = false;
    pthread_t listenerThread;
    bool lockPending_ = false;
    bool leaving_ = false;
    bool isSpectator_ = false;
    int invCMDCount_ = 0;
    bool speedUp_ = false;
    std::chrono::time_point<std::chrono::steady_clock> blackScreenStart;
    int blackScreenSec = 0;
    std::chrono::time_point<std::chrono::steady_clock> speedDownStart;
    int speedDownSec = 0;
    std::map<int, std::string> playerNames_;
    bool controllerWait = false;
    std::vector<std::string> activeMalus;

   public:
    Game(std::shared_ptr<Player> player, std::shared_ptr<IView> view,
         std::shared_ptr<IController> controller,
         std::shared_ptr<Server> server);
    ~Game();
    Game(const Game& other) = delete;
    Game& operator=(const Game& other) = delete;

    int start();
    void askTetraminoToServer();
    void startSpectator();
    void reset();
    void clearFullRows(GameUpdateHeader& update);
    void tetraminoFall();
    void checkSignal();
    void updatePlayerPosition(int x, int y);

    void setupBoards(std::vector<int> idPlayers, bool hasGrid = false);
    [[nodiscard]] bool getRunning();
    [[nodiscard]] bool getPaused() const { return paused_; }
    [[nodiscard]] int getPoints() const { return points_; }
    [[nodiscard]] int getEnergy() const { return energy; }
    [[nodiscard]] bool getIsLeaving() const { return leaving_; }
    [[nodiscard]] bool getIsSpectator() const { return isSpectator_; }
    [[nodiscard]] Tetramino& getCurrent() { return current_; }
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> getGrid() {
        return board_.getGrid();
    }
    [[nodiscard]] std::map<int, std::vector<std::vector<std::uint8_t>>>
    getBoards() {
        return board_.getBoards();
    }
    std::shared_ptr<Player> getPlayer() { return player_; }
    void setPlayerName(int id, const std::string& name) {
        playerNames_[id] = name;
    }
    std::string getPlayerName(int id) {
        auto it = playerNames_.find(id);
        if (it != playerNames_.end()) return it->second;
        return "Unknown";
    }

    std::vector<std::string> getActiveMalus() { return activeMalus; }

    void addActiveMalus(const std::string& malus) {
        if (std::find(activeMalus.begin(), activeMalus.end(), malus) == activeMalus.end()) {
            activeMalus.push_back(malus);
        }
    }
    
    void removeActiveMalus(const std::string& malus) {
        auto it = std::find(activeMalus.begin(), activeMalus.end(), malus);
        if (it != activeMalus.end()) {
            activeMalus.erase(it);
        }
    }
    
    bool isControllerWaiting() { return controllerWait; }
    void setControllerWait(bool wait) { controllerWait = wait; }
    std::string getGameMode() { return gameMode_; }
    void setGameMode(const std::string& mode) { gameMode_ = mode; }
    void setRunning(bool running);
    void setPaused(bool paused);
    void setPoints(int points) { points_ = points; }
    void setIsLeaving(bool leaving) { leaving_ = leaving; }
    void setIsSpectator(bool isSpectator) { isSpectator_ = isSpectator; }

    void sendSignalMutex();
    void sendSpawnTetramino(const std::vector<std::vector<int>>& shape, int x,
                            int y);
    void sendMovementMessage(int dx, int dy);
    void sendRotationMessage(bool clockwise);
    void sendMalusMessage(MalusPayload& payload);
    void sendBonusMessage(BonusPayload& payload);
    void applyMalus(MalusPayload& payload);
    void applyBonus(BonusPayload& payload);
    void sendQuitParty();
    void sendStartMessage();

    void addMalusRow(MalusPayload& payload);
    bool isInputBlocked() const { return blockedInput_; }
    void unblockInput() { blockedInput_ = false; }

    void addLuckyPoints(BonusPayload& payload);

    void processServerResponse();
    void handleTetramino(char* recvBuffer);
    void handleMove(char* recvBuffer);
    void handleRotate(char* recvBuffer);
    void handleLock(char* recvBuffer);
    void handleUpdateGrid(char* recvBuffer);
    void handleMalus(char* recvBuffer);
    void handleEnd(GameTypeHeader& gameHeader);
    void handleMalusAuthorisation();
    void handleBonusAuthorisation();
    void showGame();
    void* receiveLoop();
    static void* receiveLoopHelper(void* arg);
    std::vector<std::vector<int>> rotate(
        const std::vector<std::vector<int>>& shape, bool clockwise);
    void newBoard();
    std::vector<std::vector<std::uint8_t>> convertGrid(
        uint8_t raw[HEIGHT][WIDTH]);

    void askRoyalPermission(std::string type);
    bool isInvCMD() const { return invCMDCount_ > 0; }
    bool isSpeedUp() { return speedUp_; }
    void setSpeedUp(bool speed) { speedUp_ = speed; }
    bool isBlackScreen();
    bool isSpeedDown();
    void setSpeedDown(int speed) { speedDownSec = speed; }
};
