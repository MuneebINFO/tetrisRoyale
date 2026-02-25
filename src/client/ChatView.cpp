#include "ChatView.h"

void clearScreen() {
    clear();
    refresh();
}

void ChatView::showChatRoom() {
    clearScreen();
    int bottomLine =  getmaxy(stdscr) - 1;
    PlayerHeader friendSelected = player_->getFriendSelected();
    mvprintw(0, 0, "Chat Room with %s (ESC to exit, Enter to send)", friendSelected.username);
    showHistoryChat(friendSelected);

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    mvprintw(bottomLine, 1, "%s", PROMPT_TEXT);
    
    move(bottomLine, 1 + promptLength);
    refresh();
}

void ChatView::showHistoryChat(PlayerHeader friendSelected) {
    ChatModel message;
    server_->handleSocialRequest(SOCIAL_TYPE::GET_MESSAGE, friendSelected);
    std::vector<ChatHeader> chatList = message.getMessages(server_);
    
    for (int i = 0; i < int(chatList.size()); i++) {
        if(chatList[i].idPlayer == player_->getPlayerId()) { 
            mvprintw(1 + i, 1, "You: %s", chatList[i].message);
        }
        else {
            mvprintw(1 + i, 50, "%s says: %s", chatList[i].username, chatList[i].message);
        }  
    }
    player_->setLine(1 + int(chatList.size()));
    refresh();
}

void ChatView::refreshChatView(int line, std::string buffer) {
    int bottomLine = getmaxy(stdscr) - 1;

    move(bottomLine, 1);
    clrtoeol();
    mvprintw(bottomLine, 1, "%s", PROMPT_TEXT);

    move(line, 1);
    clrtoeol();
    mvprintw(line, 1, "You: %s", buffer.c_str());
    refresh();
}

void ChatView::showReceiveChatMessage(int line, PlayerHeader msg, std::shared_ptr<Player> player_) {
    int bottomLine = getmaxy(stdscr) - 1;

    if (strcmp(msg.message, "Message sent") == 0) {
        std::string lastMessage = player_->getLastMessageSent();
        mvprintw(line, static_cast<int>(lastMessage.size()) + 20, "%s", msg.message);
    } else {
        std::string displayText = std::string(msg.username) + " says: " + std::string(msg.message);
        mvprintw(line, 50, "%s", displayText.c_str());
    }
    
    line += 2;
    player_->setLine(line);
    move(bottomLine, 1 + strlen("> "));
    refresh();
}

void ChatView::renderChatPrompt(int pos, char ch) {
    int bottomLine = getmaxy(stdscr) - 1;
    move(bottomLine, 1 + promptLength + pos);
    mvaddch(bottomLine, 1 + promptLength + pos, ch);
}

void ChatView::handleBackSpace(int pos, char ch) {
    int bottomLine = getmaxy(stdscr) - 1;
    mvaddch(bottomLine, 1 + promptLength + pos, ch);
    move(bottomLine, 1 + promptLength + pos);
}