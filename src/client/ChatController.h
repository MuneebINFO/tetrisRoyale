#ifndef __CHATCONTROLLER_H
#define __CHATCONTROLLER_H

#include "ChatModel.h"
#include "Signal.h"
#include "Tetris.h"
#include "ChatView.h"

class Signal;
class ChatModel;
class ChatController;
class ChatView;
class Tetris;

struct ChatThreadArgs2 {
    ChatController* chatController;
    std::string username;
    std::shared_ptr<Player> player;
};

class ChatController {
   private:
    Signal& signal_;
    bool chatThreadActive = false;
    pthread_t chatThread = -1;
    pthread_mutex_t chatMutex;
    std::shared_ptr<ChatModel> chatModel_;
    std::shared_ptr<Server> server_;
    std::shared_ptr<ChatView> chatView_;
    std::shared_ptr<Player> player_;

   public:
    ChatController();
    ~ChatController();
    [[noreturn]] static void* receiveMessages(void* arg);
    void captureInputChatRoom(Tetris& tetris);
    void setPlayer(std::shared_ptr<Player> player) { player_ = player; }
};

#endif
