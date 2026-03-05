#ifndef __CHATCONTROLLER_H
#define __CHATCONTROLLER_H

#include "../model/ChatModel.h"
#include "../model/Signal.h"
#include "../model/Tetris.h"
#include "../view/ChatView.h"

class Signal;
class ChatModel;
class ChatController;
class ChatView;
class Tetris;

struct ChatThreadArgs {
    ChatController* chatController;
    std::string username;
    std::shared_ptr<Player> player;
};

/*
    ChatController is responsible for managing the chat functionality
    between players. It handles the input and output of chat messages,
    as well as the interaction with the server and the chat model.
    It also manages the chat thread for receiving messages.
*/

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
    ChatThreadArgs* threadArgs = nullptr;

    void initializeMutex();
    void initChatSession();
    bool handleEscapeKey(Tetris& tetris);
    void handleEnterKey(std::string& buffer, int line);
    void handleBackspaceKey(std::string& buffer, int pos);
    void handleCharacterInput(std::string& buffer, int pos, int ch);
    int getCurrentLine();
    void cleanupChatThread();
   public:
    ChatController();
    ~ChatController();
    static void* receiveMessages(void* arg);
    void captureInputChatRoom(Tetris& tetris);
    void setPlayer(std::shared_ptr<Player> player) { player_ = player; }
    void setServer(std::shared_ptr<Server> server) { server_ = server; }
    void setChatView(std::shared_ptr<ChatView> chatView) {
        chatView_ = chatView;
    }
   
};

#endif
