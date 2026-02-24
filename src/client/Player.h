#ifndef PLAYER_H_
#define PLAYER_H_
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
    FriendHeader friendSelected_;
    std::vector<FriendHeader> vectorFriends_;
    std::vector<FriendHeader> vectorInvitations_;

    std::shared_ptr<Server> server_;
    std::shared_ptr<IView> view_;
    std::shared_ptr<IController> controller_;
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
    std::string getName() { return pseudo_; }
    int getPlayerId() { return playerId_; }
    std::vector<FriendHeader> getVectorFriends() const {
        return vectorFriends_;
    }
    std::vector<FriendHeader> getVectorInvitations() const {
        return vectorInvitations_;
    }
    std::vector<LobbyInvitation> getLobbyInvitations();
    bool hasNoFriends() { return noFriends_; }
    void setNoFriends(bool noFriends) { noFriends_ = noFriends; }
    void setVectorFriends(std::vector<FriendHeader> vectorFriends) {
        vectorFriends_ = vectorFriends;
    }
    void setVectorInvitations(std::vector<FriendHeader> vectorInvitations) {
        vectorInvitations_ = vectorInvitations;
    }
    std::shared_ptr<Server> getServer() { return server_; }
    bool isConnected() { return playerId_ != -1; }
    void setName(std::string pseudo) { pseudo_ = pseudo; }
    void setPlayerId(int playerId) {
        if (playerId_ == -1) playerId_ = playerId;
    }
    FriendHeader getFriendSelected() { return friendSelected_; }
    void setFriendSelected(FriendHeader friendSelected) {
        this->friendSelected_ = friendSelected;
    }
    std::string getLastMessageSent() { return lastMessageSent_; }
    void setLastMessageSent(std::string lastMessageSent) {
        this->lastMessageSent_ = lastMessageSent;
    }
    void setLine(int line) { line_ = line; }
    int getLine() { return line_; }
};
#endif
