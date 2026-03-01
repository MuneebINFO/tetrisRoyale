#pragma once

#include <pthread.h>

#include <atomic>
#include <memory>
#include <string>

#include "Controller.h"
#include "ViewCLI.h"

/*
 * ControllerCLI
 * This class is the controller for the CLI version of the game.
 */
struct Terminal {
    termios oldt;
    int oldf;
};

class ControllerCLI final : public IController {
   private:
    Terminal old_;
    pthread_t thread;
    pthread_t chatThread;
    pthread_mutex_t chatMutex;
    bool chatThreadActive = false;
    std::atomic<bool> threadActive;
    std::unique_ptr<ThreadData> data_;
    std::shared_ptr<ViewCLI> view_;

    static void* GameInputUser(void* c);
    bool validateInput(std::string input);
    void setRawMode(bool enable);
    void captureInputMainMenu(Tetris& tetris);
    void captureInputInvitationMenu(Tetris& tetris);
    void captureInputGameInvitationMenu(Tetris& tetris);
    void captureInputRankingMenu(Tetris& tetris);
    void captureInputProfileMenu(Tetris& tetris);
    void captureInputPlayMenu(Tetris& tetris);
    void captureInputCreatingMenu(Tetris& tetris);
    void captureInputChatRoom(Tetris& tetris);

    static void* receiveMessages(void* arg);

   public:
    ControllerCLI(std::shared_ptr<IView>);
    ~ControllerCLI();
    ControllerCLI(const ControllerCLI& other) = delete;
    ControllerCLI& operator=(const ControllerCLI& other) = delete;
    std::pair<std::string, std::string> getUserLoginInfo();
    void captureInput(Game* game);
    void captureInput(MENU_STATE menu, Tetris& tetris);
    void captureInputMenuLobby(Tetris& tetris);
    void waitingRoomInput(bool isLeader, bool& running);
    void waitingRoomAsLeader(bool&);
    void captureInputChoiceMenu(int key, int& idx1, int& idx2);
    void stop();
    void handleFriendRequestStatus(const HeaderResponse& responseHeader,
                                   const FriendHeader& playerNameStr);
};
