#ifndef SSERVER_H
#define SSERVER_H

#include <sys/select.h>  // multiplexage des sockets avec select()

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
    std::map<int, std::shared_ptr<Player>> clients; // socket = joueur
    // liste des salles (ID, room)
    std::vector<std::pair<int, std::shared_ptr<GameRoom>>> gameRooms_;
    std::set<int> activePlayers; // sockets des joueurs en attente
    int maxSocket;
    pqxx::connection* db;
    // données des comptes utilisateurs
    std::map<std::string, UserAccount> userDatabase;  

    static Server* instance;

    Server();
    ~Server();

    // gère l'arrivée d'une nouvelle connexion client
    void handleNewConnection();

    // traite les messages envoyés par les clients
    void handleClientMessage(int clientSocket);

    // analyse et exécute les commandes envoyées par un client
    void processMessage(int clientSocket, const std::string& message);

   public:
    // Initialize the PostgreSQL database
    void initializeDatabase();

    // démarre la boucle principale du serveur, gère les connexions et messages
    // clients
    void run();

    // getters
    std::string getUserAccount(int playerID);
    std::map<int, std::shared_ptr<Player>>& getClients() { return clients; }

    pqxx::connection* getDB() { return db; }
    std::map<std::string, UserAccount>& getUserDatabase() {
        return userDatabase;
    }
    std::vector<std::pair<int, std::shared_ptr<GameRoom>>>& getGameRooms() {
        return gameRooms_;
    }

    // déconnecte proprement un client et libère les ressources
    void disconnectClient(int clientSocket);

    int generatePlayerID();
    
    static Server& getInstance() {
        if (!instance) {
            instance = new Server();
        }
        return *instance;
    }

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
    // nettoie le lobby d'un joueur lorsqu'il quitte
    void cleanupLobby(int clientSocket);
    int receiveMessage(char* buffer);
};

#endif
