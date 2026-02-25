#ifndef __CHATVIEW_H
#define __CHATVIEW_H

#include "ChatModel.h"
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
    void showHistoryChat(PlayerHeader friendSelected);
    void refreshChatView(int line, std::string buffer);
    void showReceiveChatMessage(int line, PlayerHeader msg,
                                std::shared_ptr<Player> player_);
    void renderChatPrompt(int pos, char ch);
    void handleBackSpace(int pos, char ch);
    void setPlayer(std::shared_ptr<Player> player) { player_ = player; }
    void setServer(std::shared_ptr<Server> server) { server_ = server; }
    void setViewCLI(std::shared_ptr<ViewCLI> view) { view_ = view; }
};

#endif
