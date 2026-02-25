#ifndef __CHATMODEL_H
#define __CHATMODEL_H

#include <sys/socket.h>

#include <cstring>
#include <memory>

#include "../common/CONSTANT.h"
#include "../common/header.h"
#include "ViewCLI.h"
#include "Player.h"
#include "Server.h"

class Server;

class ChatModel {
   private:
    PlayerHeader recipient;

   public:
    ChatModel() = default;
    void setCursor();
    std::vector<ChatHeader> getMessages(std::shared_ptr<Server> server_);
    void sendMessage(std::shared_ptr<Server> server_,
                     PlayerHeader friendSelected, std::string buffer);
    void joinChatRoom(int clientSocket);
    void leaveChatRoom(int clientSocket);
    PlayerHeader getRecipient() const { return recipient; }
    void setRecipient(const PlayerHeader& recipient) {
        this->recipient = recipient;
    }
};

#endif
