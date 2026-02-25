#include "ChatModel.h"

std::vector<ChatHeader> ChatModel::getMessages(std::shared_ptr<Server> server_) {
    std::vector<ChatHeader> chatList;
    char countBuffer[sizeof(int) + sizeof(HeaderResponse)];
    int count;
    memset(countBuffer, 0, sizeof(countBuffer));
    server_->receiveMessage(countBuffer);
    memcpy(&count, countBuffer + sizeof(HeaderResponse), sizeof(int));

    for(int i = 0; i < count; i++) {
        char buffer[sizeof(ChatHeader) + sizeof(HeaderResponse)];
        memset(buffer, 0, sizeof(buffer));
        server_->receiveMessage(buffer);
        ChatHeader header;
        memcpy(&header, buffer + sizeof(HeaderResponse), sizeof(ChatHeader));
        chatList.push_back(header);
    }
    
    return chatList;
}

void ChatModel::sendMessage(std::shared_ptr<Server> server_, PlayerHeader friendSelected, std::string buffer) {
    strcpy(friendSelected.message, buffer.c_str());
    server_->handleSocialRequest(SOCIAL_TYPE::SEND_MESSAGE, friendSelected);
}

void ChatModel::setCursor() {
    int bottomLine = getmaxy(stdscr) - 1;
    move(bottomLine, 1 + promptLength);
}
