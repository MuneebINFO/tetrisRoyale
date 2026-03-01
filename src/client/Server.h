#ifndef __CSERVER_H
#define __CSERVER_H
#include <memory>

#include "../common/CONSTANT.h"
#include "../common/header.h"

class Player;
struct Tetramino;

class Server {
   private:
    std::shared_ptr<Player> player_;
    int socket_;
    bool connected_;
    int playerID_;
    int readSafe(char* buffer, size_t size);

   public:
    Server();
    ~Server();
    void setPlayer(std::shared_ptr<Player> player);
    int connectToServer();
    void disconnect();
    int sendMessage(char* buffer, size_t size);
    int receiveMessage(char* buffer);
    void setName(std::string name);
    void setPlayerID(int id);
    int getSocket() const { return socket_; }
    int getPlayerID() { return playerID_; }
    bool isConnected() { return connected_; }
    void handleSocialRequest(SOCIAL_TYPE type, FriendHeader friendHeader);
    std::vector<ChatHeader> receiveChatListHeader();
    std::vector<FriendHeader> receiveFriendListHeader();
    std::string receive();
    static void* listenToServer(void* c);
};

#endif
