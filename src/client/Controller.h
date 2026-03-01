#pragma once

#include <fcntl.h>
#include <pthread.h>
#include <termios.h>

#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

#include "../common/CONSTANT.h"
#include "../common/header.h"

class IView;
class Lobby;
class Server;
class Player;
class InvitationManager;
class Tetris;
class Game;

class IController {
   private:
    std::weak_ptr<IView> view_;
    std::weak_ptr<Lobby> lobby_;
    std::weak_ptr<Server> server_;
    std::weak_ptr<Player> player_;
    std::shared_ptr<InvitationManager> invitationManager_;
    bool multiplayerMode;

    static void* receiveMessages(void* arg);

   public:
    IController(std::shared_ptr<IView>);
    virtual ~IController() = default;
    IController(const IController& other) = delete;
    IController& operator=(const IController& other) = delete;
    virtual void handleMode(std::string mode, int nbPlayer = 1);
    virtual std::pair<std::string, std::string> getUserLoginInfo() = 0;
    bool validateInput(std::string input);
    virtual void captureInput(Game* game) = 0;
    virtual void captureInput(MENU_STATE menu, Tetris& tetris) = 0;
    virtual void captureInputMenuLobby(Tetris& tetris) = 0;
    virtual void waitingRoomInput(bool isLeader, bool& running) = 0;
    virtual void waitingRoomAsLeader(bool&) = 0;
    virtual void handleFriendRequestStatus(const PlayerHeader& invitedPlayer,
                                           std::shared_ptr<IView> view_) = 0;
    virtual void stop() = 0;
    virtual void handleKey(int ch, Game* game) = 0;

    // SETTER AND GETTER
    void setView(std::shared_ptr<IView> view);
    std::shared_ptr<IView> getView();
    void setLobby(std::shared_ptr<Lobby> lobby) { lobby_ = lobby; }
    std::shared_ptr<Lobby> getLobby();
    void setServer(std::shared_ptr<Server> server) { server_ = server; }
    std::shared_ptr<Server> getServer();
    void setPlayer(std::shared_ptr<Player> player) { player_ = player; }
    std::shared_ptr<Player> getPlayer();
    void setInvitationManager(
        std::shared_ptr<InvitationManager> invitationManager);
    std::shared_ptr<InvitationManager> getInvitationManager() {
        return invitationManager_;
    }
    void setMultiplayerMode(bool mode) { multiplayerMode = mode; }
    bool getMultiplayerMode() { return multiplayerMode; }
};

struct ThreadData {
    void* toControl;
    IController* controller;
};

struct ChatThreadArgs {
    IController* controller;
    std::string username;
    std::shared_ptr<Player> player;
};
