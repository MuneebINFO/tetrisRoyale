#ifndef __LOBBY_H
#define __LOBBY_H

#include <cstdint>
#include <memory>
#include <string>

#include "../common/CONSTANT.h"
#include "../common/header.h"
#include "Signal.h"

class IView;
class IController;
class Player;
class Server;
class Tetris;

class Lobby {
   private:
    Signal& signal_;
    Tetris& tetris_;
    std::shared_ptr<IController> controller_;
    std::shared_ptr<Player> player_;
    std::shared_ptr<IView> view_;
    std::shared_ptr<Server> server_;
    int groupeLeader_;
    int numberOfPlayer_;
    std::vector<std::pair<int, std::string>> players_;
    int numberOfSpectator_;
    std::vector<std::pair<int, std::string>> spectators_;
    std::string gameMode_;
    int gameRoomId_ = -1;
    bool isGamer_;
    bool isChoosing_;
    bool isChoosingMode_;
    bool isChoosingNumber_;
    bool hasChoosedNumber_;
    bool hasChoosedMode_;
    bool isSetup_;

   public:
    Lobby(Tetris& tetris, std::shared_ptr<IController> controller,
          std::shared_ptr<Player> player, std::shared_ptr<IView> view,
          std::shared_ptr<Server> server);
    ~Lobby();
    void setupGame();
    void startGame();
    void sendNotification(LobbyInviteFriend& lobbyInviteFriend);
    void sendStartNotification();
    void sendUpdateLobby();
    void modifyLobby();
    void joinLobby(LobbyInvitation);
    void leaveLobby();
    void waitGameStart(bool);
    void waitingRoomAsLeader();
    void addPlayer(int playerId, std::string playerName, bool isGamer);
    void removePlayer(int playerId, bool asGamer);
    void reset();

    // getters and setters
    uint8_t getNumberOfPlayer() { return uint8_t(numberOfPlayer_); }
    const std::vector<std::pair<int, std::string>>& getPlayers() {
        return players_;
    }
    void setNumberOfPlayer(int nbrPlayer) { numberOfPlayer_ = nbrPlayer; }
    int getNumberOfSpectator() { return numberOfSpectator_; }
    const std::vector<std::pair<int, std::string>>& getSpectators() {
        return spectators_;
    }
    void setNumberOfSpectator(int nbrSpec) { numberOfSpectator_ = nbrSpec; }
    std::string getGameMode() { return gameMode_; }

    void setGroupeLeader(int groupeLeader) { groupeLeader_ = groupeLeader; }
    int getGroupeLeader() { return groupeLeader_; }

    std::shared_ptr<Player> getPlayer() { return player_; }
    void setRoomId(int roomId) { gameRoomId_ = roomId; }
    int getRoomId() { return gameRoomId_; }
    void setIsGamer(bool gamer) { isGamer_ = gamer; }
    bool getIsGamer() { return isGamer_; }
    void setGameMode(std::string gameMode) { gameMode_ = gameMode; }
    bool hasChoosedNumber() { return numberOfPlayer_ > 0; }
    bool hasChoosedMode() { return gameMode_ != ""; }
    bool isChoosing() const { return isChoosing_; }
    void setChoosing(bool choosing) { isChoosing_ = choosing; }
    bool isChoosingMode() const { return isChoosingMode_; }
    void setChoosingMode(bool choosingMode) { isChoosingMode_ = choosingMode; }
    bool isChoosingNumber() const { return isChoosingNumber_; }
    void setChoosingNumber(bool choosingNumber) {
        isChoosingNumber_ = choosingNumber;
    }
    void setIsSetup(bool setup) { isSetup_ = setup; }
    bool getIsSetup() { return isSetup_; }
    std::shared_ptr<IView> getView() { return view_; }
};

#endif
