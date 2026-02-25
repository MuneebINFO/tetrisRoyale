#ifndef PLAYER_H_
#define PLAYER_H_
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../common/CONSTANT.h"
#include "../common/header.h"
#include "Signal.h"

class Server;
class IView;
class IController;
class Tetris;

class Player : public std::enable_shared_from_this<Player> {
   private:
    Signal& signal_;
    std::string pseudo_;
    int playerId_;
    int line_ = 1;
    bool noFriends_;
    bool requestAccepted_;
    std::string lastMessageSent_;
    PlayerHeader friendSelected_;
    PlayerHeader player_;
    std::vector<PlayerHeader> vectorFriends_;
    std::vector<PlayerHeader> vectorInvitations_;

    std::weak_ptr<Server> server_;
    std::weak_ptr<IView> view_;
    std::weak_ptr<IController> controller_;
    std::pair<std::string, std::string> getUserLoginInfo(ACCOUNT_STATE menu);

    [[nodiscard]] __attribute__((const)) static ACCOUNT_TYPE
    translateAccountState(ACCOUNT_STATE menu);

   public:
    Player(std::shared_ptr<IView> view, std::shared_ptr<IController> controller,
           std::shared_ptr<Server> server);
    ~Player() = default;
    bool requestAccepted() { return requestAccepted_; }
    void setRequestAccepted(bool requestAccepted) {
        requestAccepted_ = requestAccepted;
    }
    bool login(Tetris* tetris);
    std::pair<std::string, std::string> getUserInfo(ACCOUNT_STATE);

    // Getter and setter

    std::shared_ptr<IController> getController();
    std::shared_ptr<IView> getView();
    std::string getName() { return pseudo_; }
    int getPlayerId() { return playerId_; }
    std::vector<PlayerHeader> getVectorFriends() const {
        return vectorFriends_;
    }
    std::vector<PlayerHeader> getVectorInvitations() const {
        return vectorInvitations_;
    }
    std::vector<LobbyInvitation> getLobbyInvitations();
    bool hasNoFriends() { return noFriends_; }
    void setNoFriends(bool noFriends) { noFriends_ = noFriends; }
    void setVectorFriends(std::vector<PlayerHeader> vectorFriends) {
        vectorFriends_ = vectorFriends;
    }
    void setVectorInvitations(std::vector<PlayerHeader> vectorInvitations) {
        vectorInvitations_ = vectorInvitations;
    }
    std::shared_ptr<Server> getServer();
    bool isConnected() { return playerId_ != -1; }
    void setName(std::string pseudo) { pseudo_ = pseudo; }
    void setPlayerId(int playerId) {
        if (playerId_ == -1) playerId_ = playerId;
    }
    PlayerHeader getFriendSelected() { return friendSelected_; }
    void setFriendSelected(PlayerHeader friendSelected) {
        this->friendSelected_ = friendSelected;
    }
    PlayerHeader getPlayer() {
        player_.idPlayer = playerId_;
        strcpy(player_.username, pseudo_.c_str());
        return player_;
    }
    std::string getLastMessageSent() { return lastMessageSent_; }
    void setLastMessageSent(std::string lastMessageSent) {
        this->lastMessageSent_ = lastMessageSent;
    }
    void setLine(int line) { line_ = line; }
    int getLine() { return line_; }

    void setHightScore(int score);
    int getHightScore() { return player_.highScore; }
};
#endif
