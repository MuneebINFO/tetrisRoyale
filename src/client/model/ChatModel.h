#ifndef __CHATMODEL_H
#define __CHATMODEL_H

#include <sys/socket.h>

#include <cstring>
#include <memory>

#include "../../common/CONSTANT.h"
#include "../../common/header.h"
#include "../view/ViewCLI.h"
#include "Player.h"
#include "Server.h"

class Server;

/*
    ChatModel is responsible for managing the chat functionality
    between players. It handles the input and output of chat messages,
    as well as the interaction with the server and the chat model.
*/
class ChatModel {
   private:
    PlayerHeader recipient;

   public:
    ChatModel() = default;
    void setCursor();
    std::vector<ChatHeader> getMessages(std::shared_ptr<Server> server_);
    void sendMessage(std::shared_ptr<Server> server_,
                     PlayerHeader friendSelected, std::string buffer);
    PlayerHeader getRecipient() const { return recipient; }
    void setRecipient(const PlayerHeader& recipient) {
        this->recipient = recipient;
    }
};

#endif
