#pragma once
#include <pthread.h>
#include <termios.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../common/CONSTANT.h"
#include "../common/header.h"
#include "Signal.h"

class IView;
class IController;
class Player;
class Server;

struct Tetramino {
    int x_, y_;  // position de départ
    std::vector<std::vector<int>> shape_;
    std::uint8_t colorIndex_;

    Tetramino(int x, int y, std::vector<std::vector<int>> shape,
              std::uint8_t colorIndex)
        : x_(x), y_(y), shape_(shape), colorIndex_(colorIndex) {}
};

class Board {
   private:
    bool hasGrid_;
    std::vector<std::vector<std::uint8_t>> grid_;
    std::map<std::uint16_t, std::vector<std::vector<std::uint8_t>>> boards_;

   public:
    Board() = default;
    Board(std::vector<std::uint16_t> idPlayers, bool hasGrid = false);
    std::vector<std::vector<std::uint8_t>>& getGrid() { return grid_; }
    std::map<std::uint16_t, std::vector<std::vector<std::uint8_t>>>&
    getBoards() {
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
    Signal& signal_;
    std::shared_ptr<Player> player_;
    std::shared_ptr<IView> view_;
    std::shared_ptr<IController> controller_;
    std::shared_ptr<Server> server_;
    std::vector<std::uint8_t> previousColors_;
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
    std::map<int, std::string> playerNames_;
    std::vector<std::string> activeMalus;

   public:
    Game(std::shared_ptr<Player> player, std::shared_ptr<IView> view,
         std::shared_ptr<IController> controller,
         std::shared_ptr<Server> server);
    ~Game();
    Game(const Game& other) = delete;
    Game& operator=(const Game& other) = delete;

    void start();
    [[nodiscard]] Tetramino generateRandomTetramino();
    void clearFullRows(GameUpdateHeader& update);
    void tetraminoFall();
    void checkSignal();
    void updatePlayerPosition(int x, int y);

    void setupBoards(std::vector<std::uint16_t> idPlayers,
                     bool hasGrid = false);
    [[nodiscard]] bool getRunning() const { return running_; }
    [[nodiscard]] bool getPaused() const { return paused_; }
    [[nodiscard]] int getPoints() const { return points_; }
    [[nodiscard]] Tetramino& getCurrent() { return current_; }
    [[nodiscard]] std::vector<std::vector<std::uint8_t>> getGrid() {
        return board_.getGrid();
    }
    [[nodiscard]] std::map<std::uint16_t,
                           std::vector<std::vector<std::uint8_t>>>
    getBoards() {
        return board_.getBoards();
    }
    std::shared_ptr<Player> getPlayer() {
        return player_;
    }
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
    std::string getGameMode() { return gameMode_; }
    void setGameMode(const std::string& mode) { gameMode_ = mode; }
    void setRunning(bool running);
    void setPaused(bool paused);
    void setPoints(int points) { points_ = points; }
    void sendSignalMutex();
    void sendSpawnTetramino(const std::vector<std::vector<int>>& shape, int x,
                            int y);
    void sendMovementMessage(int dx, int dy);
    void sendRotationMessage(bool clockwise);
    void sendMalusMessage(int malusType, int targetSocket);
    void sendBonusMessage(int bonusType);
    int selectMalus();
    int selectBonus();
    int getRandomOpponent();
    void receiveMalus();
    void applyMalus(MalusPayload& payload);
    void applyBonus(int type);
    void addMalusRow(int lines);
    bool isInputBlocked() const { return blockedInput_; }
    void unblockInput() { blockedInput_ = false; }
    void addLuckyPoints();

    void processServerResponse();
    void* receiveLoop();
    static void* receiveLoopHelper(void* arg);
    std::vector<std::vector<int>> rotate(
        const std::vector<std::vector<int>>& shape, bool clockwise);
    void newBoard();
    std::vector<std::vector<std::uint8_t>> convertGrid(uint8_t raw[HEIGHT][WIDTH]);
    void sendConfirmMalus(MalusPayload& malus);
};
