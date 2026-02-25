#include "ChatController.h"

ChatController::ChatController()
    : signal_{Signal::getInstance()},
      chatModel_{std::make_shared<ChatModel>()},
      server_{std::make_shared<Server>()},
      chatView_{std::make_shared<ChatView>()} {
    if (pthread_mutex_init(&chatMutex, nullptr) != 0) {
        throw std::runtime_error("Error: mutex init failed");
    }
}

ChatController::~ChatController() { pthread_mutex_destroy(&chatMutex); }

[[noreturn]] void* ChatController::receiveMessages(void* arg) {
    ChatThreadArgs2* args = static_cast<ChatThreadArgs2*>(arg);
    ChatController* chatController = args->chatController;
    std::shared_ptr<Player> player = args->player;

    char buffer[sizeof(HeaderResponse) + sizeof(PlayerHeader)];

    while (true) {
        int bytesRead = chatController->server_->receiveMessage(buffer);

        if (bytesRead > 0) {
            HeaderResponse header;
            memcpy(&header, buffer, sizeof(HeaderResponse));

            int line = player->getLine();
            pthread_mutex_lock(&chatController->chatMutex);
            if (header.type == MESSAGE_TYPE::CHAT) {
                PlayerHeader msg;
                memcpy(&msg, buffer + sizeof(HeaderResponse),
                       sizeof(PlayerHeader));
                chatController->chatView_->showReceiveChatMessage(line, msg,
                                                                  player);
            }
            pthread_mutex_unlock(&chatController->chatMutex);
        }
    }

    refresh();
    delete args;
}

void ChatController::captureInputChatRoom(Tetris& tetris) {
    bool done = false;
    chatThreadActive = true;
    PlayerHeader you = player_->getPlayer();
    PlayerHeader friendSelected = player_->getFriendSelected();
    ChatThreadArgs2* args =
        new ChatThreadArgs2{this, friendSelected.username, player_};
    pthread_create(&chatThread, nullptr, receiveMessages, args);

    std::string buffer;
    chatModel_->setCursor();
    while (!done && signal_.getSigIntFlag() == 0) {
        pthread_mutex_lock(&chatMutex);
        int line = args->player->getLine();
        int pos = static_cast<int>(buffer.length());
        pthread_mutex_unlock(&chatMutex);
        int ch = getch();

        switch (ch) {
            case 27: {
                strcpy(you.receiver, "");
                server_->handleSocialRequest(SOCIAL_TYPE::LEAVE_CHAT, you);

                timespec timeout;
                clock_gettime(CLOCK_REALTIME, &timeout);
                timeout.tv_sec += 0;
                timeout.tv_nsec += 100000000;

                int join_result =
                    pthread_timedjoin_np(chatThread, nullptr, &timeout);
                if (join_result == ETIMEDOUT) {
                    pthread_cancel(chatThread);
                    pthread_detach(chatThread);
                }

                tetris.setMenuState(MENU_STATE::PROFILE);
                done = true;
            } break;
            case '\n': {
                pthread_mutex_lock(&chatMutex);
                chatView_->refreshChatView(line, buffer);
                if (!buffer.empty()) {
                    args->player->setLastMessageSent(buffer);
                    chatModel_->sendMessage(server_, friendSelected, buffer);
                    pthread_mutex_unlock(&chatMutex);
                } else {
                    pthread_mutex_unlock(&chatMutex);
                }
                buffer.clear();
            } break;

            case KEY_BACKSPACE:
            case 127:
            case '\b': {
                if (pos > 0) {
                    buffer.pop_back();
                    pthread_mutex_lock(&chatMutex);
                    chatView_->handleBackSpace(pos - 1, ' ');
                    pthread_mutex_unlock(&chatMutex);
                }
            } break;

            default: {
                if (isprint(ch) && buffer.length() < MAX_LENGTH_MESSAGES) {
                    buffer.push_back(static_cast<char>(ch));
                    pthread_mutex_lock(&chatMutex);
                    chatView_->renderChatPrompt(pos, ch);
                    pthread_mutex_unlock(&chatMutex);
                }
            } break;
        }
    }
}
