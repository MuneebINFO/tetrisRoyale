#ifndef MESSAGE__H
#define MESSAGE__H

#include <cstring>
#include <memory>

#include "../common/CONSTANT.h"
#include "../common/header.h"
#include "Lobby.h"

class Player;
class Server;
class IView;

class Message {
   public:
    Message() = default;
    ~Message() = default;
    void serializeConnection(ACCOUNT_TYPE type, std::string username,
                             std::string password, char* buffer);
    AccountResponseHeader fromBuffer(char* buffer);
    void serializeLobby(LOBBY_TYPE type, Lobby& lobby, char* buffer);
    void serializeSocialGetLobbyInvite(SOCIAL_TYPE type,
                                       std::shared_ptr<Player> player,
                                       char* buffer);
};

class LobbyMessage {
   private:
    std::shared_ptr<Server> server_;

    void handleUPDATE_PLAYER(Lobby& lobby, char* buffer);
    void handleUPDATE(Lobby& lobby, char* buffer);
    void handleError(Lobby& lobby, char* buffer);

   public:
    LobbyMessage(std::shared_ptr<Server>);
    ~LobbyMessage() = default;
    void handleWaitingRoom(Lobby& lobby, bool&);
};
#endif
