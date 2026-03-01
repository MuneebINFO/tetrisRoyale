#ifndef __VIEW_H
#define __VIEW_H

#include <pthread.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <memory>

#include "../common/CONSTANT.h"
#include "../common/header.h"

class Player;
class Lobby;
class Server;
class Game;

class IView {
   private:
    std::weak_ptr<Player> player_;
    std::weak_ptr<Lobby> lobby_;
    std::weak_ptr<Server> server_;
    std::string errorMessage_;

   public:
    IView() = default;
    virtual ~IView() = default;

    virtual void showBoards(Game* game) = 0;
    virtual void showGame(Game* game) = 0;
    virtual void showEndScreen(bool won) = 0;

    virtual ACCOUNT_STATE showAccountConnection() = 0;
    virtual void showCreatingMenu() = 0;
    virtual int showMenuInviteFriendToParty(
        std::vector<PlayerHeader> friends) = 0;
    virtual void showMenu(MENU_STATE menu) = 0;
    virtual void showMenuLobby() = 0;
    virtual void showLobbyModify() = 0;
    virtual void showLobbyWaitingRoom(bool) = 0;
    virtual void showMainMenu() = 0;
    virtual int showGameInvitationMenu(std::vector<LobbyInvitation>) = 0;
    virtual void showRankingMenu() = 0;
    virtual void showPlayMenu() = 0;
    virtual void showLogin() = 0;
    virtual void showSignUp() = 0;
    virtual int showMalusType() = 0;
    virtual int showBonusType() = 0;
    virtual int showMalusTarget(Game* game_) = 0;
    virtual void showMalus(Game* game) = 0;


    virtual void showMessage(std::string message, int y = 20, int x = 20) = 0;
    virtual void showErrorMessage(std::string message, int y, int x) = 0;

    virtual void clearScreen() = 0;
    // Setter and getter
    void setLobby(std::shared_ptr<Lobby> lobbyTest);
    std::shared_ptr<Lobby> getLobby();
    void setPlayer(std::shared_ptr<Player> player);
    std::shared_ptr<Player> getPlayer();
    void setServer(std::shared_ptr<Server> server) { server_ = server; }
    std::shared_ptr<Server> getServer();
    void setErrorMessage(std::string message) { errorMessage_ = message; }
    std::string getErrorMessage() { return errorMessage_; }
    std::vector<std::string> getVectorGameMode();
};

#endif
