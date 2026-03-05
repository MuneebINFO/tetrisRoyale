#include "ChatController.h"

namespace {
    constexpr int THREAD_JOIN_TIMEOUT_SEC = 1;
}

ChatController::ChatController()
    : signal_{Signal::getInstance()},
      chatThreadActive{false},
      chatModel_{std::make_shared<ChatModel>()},
      server_{std::make_shared<Server>()},
      chatView_{std::make_shared<ChatView>()} {
    initializeMutex();
}

ChatController::~ChatController() { 
    if (threadArgs != nullptr) {
        delete threadArgs;
        threadArgs = nullptr;
    }
    pthread_mutex_destroy(&chatMutex); 
}

void ChatController::initializeMutex() {
    if (pthread_mutex_init(&chatMutex, nullptr) != 0) {
        throw std::runtime_error("Error: chat mutex initialization failed");
    }
}

void* ChatController::receiveMessages(void* arg) {
    ChatThreadArgs* args = static_cast<ChatThreadArgs*>(arg);
    ChatController* chatController = args->chatController;
    std::shared_ptr<Player> player = args->player;
  
    char buffer[sizeof(HeaderResponse) + sizeof(PlayerHeader)];
    
    while (chatController->chatThreadActive) {
        int bytesRead = chatController->server_->receiveMessage(buffer);
        
        if (bytesRead > 0) {
            HeaderResponse header;
            memcpy(&header, buffer, sizeof(HeaderResponse));

            int line = player->getLine();
            pthread_mutex_lock(&chatController->chatMutex);
            
            if (header.type == MESSAGE_TYPE::CHAT) {
                PlayerHeader msg;
                memcpy(&msg, buffer + sizeof(HeaderResponse), sizeof(PlayerHeader));
                chatController->chatView_->showReceiveChatMessage(line, msg, player);
            }
            
            pthread_mutex_unlock(&chatController->chatMutex);
        }
    }

    refresh();
    return nullptr;
}

void ChatController::captureInputChatRoom(Tetris& tetris) {
    initChatSession();
    
    std::string buffer;
    bool done = false;
    
    while (!done && signal_.getSigIntFlag() == 0) {
        int line = getCurrentLine();
        int pos = static_cast<int>(buffer.length());
        int ch = getch();

        switch (ch) {
            case ESCAPE:
                done = handleEscapeKey(tetris);
                break;
                
            case '\n':
                handleEnterKey(buffer, line);
                break;

            case KEY_BACKSPACE:
            case 127:
            case '\b':
                handleBackspaceKey(buffer, pos);
                break;

            default:
                handleCharacterInput(buffer, pos, ch);
                break;
        }
    }
}

void ChatController::initChatSession() {
    chatThreadActive = true;
    PlayerHeader friendSelected = player_->getFriendSelected();
    
    threadArgs = new ChatThreadArgs{
        this, 
        friendSelected.username, 
        player_
    };
    
    pthread_create(&chatThread, nullptr, receiveMessages, threadArgs);
    chatModel_->setCursor();
}

bool ChatController::handleEscapeKey(Tetris& tetris) {
    PlayerHeader you = player_->getPlayer();
    strcpy(you.receiver, "");
    server_->handleSocialRequest(SOCIAL_TYPE::LEAVE_CHAT, you);
    
    cleanupChatThread();
    
    tetris.setMenuState(MENU_STATE::PROFILE);
    chatThreadActive = false;
    return true;
}

void ChatController::handleEnterKey(std::string& buffer, int line) {
    pthread_mutex_lock(&chatMutex);
    
    chatView_->refreshChatView(line, buffer);
    
    if (!buffer.empty()) {
        PlayerHeader friendSelected = player_->getFriendSelected();
        player_->setLastMessageSent(buffer);
        chatModel_->sendMessage(server_, friendSelected, buffer);
    }
    
    pthread_mutex_unlock(&chatMutex);
    buffer.clear();
}

void ChatController::handleBackspaceKey(std::string& buffer, int pos) {
    if (pos > 0) {
        buffer.pop_back();
        
        pthread_mutex_lock(&chatMutex);
        chatView_->handleBackSpace(pos - 1, ' ');
        pthread_mutex_unlock(&chatMutex);
    }
}

void ChatController::handleCharacterInput(std::string& buffer, int pos, int ch) {
    if (isprint(ch) && buffer.length() < ::MAX_LENGTH_MESSAGES) {
        buffer.push_back(static_cast<char>(ch));
        
        pthread_mutex_lock(&chatMutex);
        chatView_->renderChatPrompt(pos, ch);
        pthread_mutex_unlock(&chatMutex);
    }
}

int ChatController::getCurrentLine() {
    pthread_mutex_lock(&chatMutex);
    int line = player_->getLine();
    pthread_mutex_unlock(&chatMutex);
    return line;
}

void ChatController::cleanupChatThread() {
    timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += THREAD_JOIN_TIMEOUT_SEC;

    int join_result = pthread_timedjoin_np(chatThread, nullptr, &timeout);
    
    if (join_result == ETIMEDOUT) {
        pthread_cancel(chatThread);
        pthread_detach(chatThread);
        
    }

    delete threadArgs;
    threadArgs = nullptr;
    
}