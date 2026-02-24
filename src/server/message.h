#ifndef __SMESSAGE_H_
#define __SMESSAGE_H

#include <memory>
#include <string>

#include "../common/header.h"
#include "server.h"


class IMessage {
   public:
    IMessage() = default;
    virtual ~IMessage() = default;
    virtual void handleMessage(const std::string& message,
                               int clientSocket = -1) = 0;
    static std::shared_ptr<IMessage> buildMessage(const Header& header);
};

// Derived class for account-related messages (e.g., login, register, logout)
class AccountMessage final : public IMessage {
    AccountHeader header_;  // Header specific to account messages

    void handleLogin(const std::string& username, const std::string& password,
                     int clientSocket);
    void handleRegister(const std::string& username,
                        const std::string& password, int clientSocket);
    void handleLogout(const std::string& username, int clientSocket);

   public:
    void handleMessage(const std::string& message,
                       int clientSocket = -1) override;
};

class GameMessage final : public IMessage {
    GameTypeHeader header_;

    // Helper methods to handle specific game actions
    void handleSpawnTetraminoMsg(const char* details, int clientSocket);
    void handleMoveMsg(const char* details, int clientSocket);
    void handleRotateMsg(const char* details, int clientSocket);
    void handleMalusMsg(const char* target, int clientSocket);
    void handleBonusMsg(const char* target, int clientSocket);
    void handleStartMsg(int clientSocket);
    void handleEndMsg(int clientSocket);
    void handleConfirmMalusMsg(const char* target, int clientSocket);

   public:
    void handleMessage(const std::string& message,
                       int clientSocket = -1) override;
};

class LobbyMessage final : public IMessage {
    LobbyHeader header_;

    void handleCreate(const std::string& message, int clientSocket);
    void handleModify(const std::string& message);
    void handleJoin(const std::string& message, int clientSocket);
    void handleLeave(const std::string& message, int clientSocket);
    void handleInviteFriend(const std::string& message, int clientSocket);
    void handleStart(const std::string& message, int clientSocket);

   public:
    void handleMessage(const std::string& message,
                       int clientSocket = -1) override;
    int serializeErrorLobby(const std::string& msg, char* buffer);
};

class ChatMessage final : public IMessage {
   public:
    void handleMessage(const std::string& message,
                       int clientSocket = -1) override;
};

class SystemMessage final : public IMessage {
   public:
    void handleMessage(const std::string& message,
                       int clientSocket = -1) override;
};

class SocialMessage final : public IMessage {
    private:
     void handleGetLobbyInvite(const std::string& message, int clientSocket);
     void inviteFriend(const std::string& message, int clientSocket = -1);
     void getFriendRequest(const std::string& message, int clientSocket = -1);
     void getFriendList(const std::string& message, int clientSocket = -1);
     void acceptFriendinvite(const std::string& message, int clientSocket = -1);
     void sendMessages(const std::string& message, int clientSocket = -1);
     void getMessages(const std::string& message, int clientSocket = -1);
     void saveMessageToDatabase(int senderId, int receiverId, const std::string& message);
     std::vector<std::pair<int, std::string>> loadMessageHistory(int player1Id, int player2Id);
     void inChatRoom(const std::string& message, int clientSocket = -1);
     void leaveChat(const std::string& message, int clientSocket = -1);
     // New helper methods
     void getFriendData(const std::string& message, int clientSocket, 
                      const std::string& queryTemplate, const std::string& countQueryTemplate);
     FRIEND_REQUEST_STATUS processInviteRequest(int invitingPlayerId, int invitedPlayerID, pqxx::connection* db);
    
    public:
     void handleMessage(const std::string& message,
                        int clientSocket = -1) override;
 };

#endif
