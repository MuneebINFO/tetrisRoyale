#ifndef __CHATVIEW_H
#define __CHATVIEW_H

#include "../model/ChatModel.h"
#include "View.h"

class ViewCLI;
class ChatView {
   private:
    std::shared_ptr<Player> player_;
    std::shared_ptr<Server> server_;
    std::shared_ptr<ViewCLI> view_;

   public:
    ChatView() = default;
    ~ChatView() = default;
    void showChatRoom();
    void showHistoryChat(const PlayerHeader& friendSelected);
    void refreshChatView(int line, const std::string& buffer);
    void showReceiveChatMessage(int line, const PlayerHeader& msg, 
                        std::shared_ptr<Player> player_);
    void setupChatRoomLayout();
    void displayUserMessage(int line, const char* message);
    void displayFriendMessage(int line, const char* username, const char* message);
    void clearInputArea();
    void displayMessageSentConfirmation(int line, const std::string& lastMessage);
    void updateCursorAfterNewMessage(int line);
    void renderChatPrompt(int pos, char ch);
    void handleBackSpace(int pos, char ch);
    void setPlayer(std::shared_ptr<Player> player) { player_ = player; }
    void setServer(std::shared_ptr<Server> server) { server_ = server; }
    void setViewCLI(std::shared_ptr<ViewCLI> view) { view_ = view; }
};

#endif
