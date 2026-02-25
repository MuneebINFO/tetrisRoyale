#pragma once

#include <pthread.h>

#include <atomic>
#include <memory>
#include <string>

#include "ChatModel.h"
#include "SocialView.h"
#include "ViewCLI.h"
#include "Controller.h"

/*
 * ControllerCLI
 * This class is the controller for the CLI version of the game.
 */
struct Terminal {
    termios oldt;
    int oldf;
};

class SocialView;
class ChatModel;

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
    std::shared_ptr<ChatModel> chatModel_;
    std::shared_ptr<SocialView> socialView_;

    static void* GameInputUser(void* c);
    static void* gameInputSpectator(void* c);
    bool validateInput(std::string input);
    void setRawMode(bool enable);
    void captureInputMainMenu(Tetris& tetris);
    void captureInputGameInvitationMenu(Tetris& tetris);
    void captureInputRankingMenu(Tetris& tetris);
    void captureInputPlayMenu(Tetris& tetris);
    void captureInputCreatingMenu(Tetris& tetris);
    void captureInputChatRoom(Tetris& tetris);

    static void* receiveMessages(void* arg);

   public:
    ControllerCLI(std::shared_ptr<IView>);
    ~ControllerCLI();
    ControllerCLI(const ControllerCLI& other) = delete;
    ControllerCLI& operator=(const ControllerCLI& other) = delete;

    std::pair<std::string, std::string> getUserLoginInfo() override;
    void captureInput(Game* game) override;
    void captureInput(MENU_STATE menu, Tetris& tetris) override;
    void captureInputMenuLobby(Tetris& tetris) override;
    void waitingRoomInput(bool isLeader, bool& running) override;
    void waitingRoomAsLeader(bool&) override;
    void stop() override;
    void handleFriendRequestStatus(const PlayerHeader& invitedPlayer,
                                   std::shared_ptr<IView> view_) override;
};
