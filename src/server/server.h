#ifndef SSERVER_H
#define SSERVER_H

#include <sqlite3.h>     // For database integration
#include <sys/select.h>  //multiplexage des sockets avec select()

#include <cstdint>
#include <map>
#include <memory>
#include <pqxx/pqxx>  // For database integration with PostgreSQL
#include <set>
#include <string>
#include <vector>
#include "../common/header.h"
#include "../common/CONSTANT.h"

#include "../common/CONSTANT.h"
#include "../common/header.h"
#include "gameServer.h"

struct Player;
class GameRoom;
struct UserAccount;

// Main server class
class Server {
    int serverSocket;
    fd_set allActiveSockets, readySockets;
    std::map<int, std::shared_ptr<Player>> clients;
    std::vector<std::pair<int, std::shared_ptr<GameRoom>>> gameRooms_;
    std::set<int> activePlayers;
    int maxSocket;
    pqxx::connection* db;
    std::map<std::string, UserAccount> userDatabase;

    static Server* instance;  // singleton

    // constructeur privé pour singleton
    Server();
    ~Server();

    // gere l'arrivée d'une nouvelle connexion client
    void handleNewConnection();

    // traite les messages envoyés par les clients
    void handleClientMessage(int clientSocket);

    // analyse et exécute les commandes envoyées par un client
    void processMessage(int clientSocket, const std::string& message);
    // on peut aussi faire une alternative ou le message sera en forme de
    // tableau d'entiers mais à voir

   public:
    // Initialize the PostgreSQL database
    void initializeDatabase();

    // démarre la boucle principale du serveur, gere les connexions et messages
    // clients
    void run();
    // getters
    std::string getUserAccount(int playerID);
    std::map<int, std::shared_ptr<Player>>& getClients() { return clients; }

    pqxx::connection* getDB() { return db; }
    std::map<std::string, UserAccount>& getUserDatabase() {
        return userDatabase;
    }

    // déconnecte proprement un client et libere les ressources
    void disconnectClient(int clientSocket);

    int generatePlayerID();

    static Server& getInstance() {
        if (!instance) {
            instance = new Server();
        }
        return *instance;
    }

    // pr etre sur que le message est correctement envoyé
    bool sendMessage(int clientSocket, const void* data, size_t dataSize);
    void removeActivePlayer(int socketPlayer);
    int findRoomIndex(
        std::vector<std::pair<int, std::shared_ptr<GameRoom>>>& rooms,
        int roomId);
    int addGameRoom(std::shared_ptr<GameRoom> room, int clientSocket);
    void removeGameRoom(int roomId);
    void addActivePlayer(std::shared_ptr<Player> player);
    static int readSafe(int clientSocket, char* buffer, size_t size);
    void broadcastToLobby(int clientSocket, const void* data, size_t size);
    std::shared_ptr<Player> getPlayer(int socket);
    std::shared_ptr<GameRoom> getLobbyForClient(int socket);
    bool checkWinner(uint32_t score);
    void cleanupLobby(int clientSocket);
    std::vector<std::pair<int, std::shared_ptr<GameRoom>>>& getGameRooms() {
        return gameRooms_;
    }
    int receiveMessage(char* buffer);
    // void updateRanking(const std::string& username, int score);
};

#endif
