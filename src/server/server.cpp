#include "server.h"

#include <netinet/in.h>
#include <sys/socket.h>  //API des sockets
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>

#include "message.h"

// Initialisation du singleton
Server* Server::instance = nullptr;

Server::Server() : db(nullptr) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        throw std::runtime_error("Error creating socket");
    }
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // permet la réutilisation du port et de l'adresse IP
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
               sizeof(opt));

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr),
             sizeof(serverAddr)) < 0) {
        throw std::runtime_error("Error binding socket");
    }

    // Start listening for incoming connections
    if (listen(serverSocket, MAX_CLIENTS) < 0) {
        throw std::runtime_error("Error listening");
    }

    // Initialize the set of active sockets
    FD_ZERO(&allActiveSockets);
    FD_SET(serverSocket, &allActiveSockets);
    maxSocket = serverSocket;

    // pour suivre le plus grand numéro de socket
    maxSocket = serverSocket;

    // Initialize the PostgreSQL database
    initializeDatabase();
}

void Server::initializeDatabase() {
    try {
        db = new pqxx::connection(
            "dbname=game_server user=tetris password='tetris' host=127.0.0.1 "
            "port=5432");

        // vérifie si la connexion à la BDD est bien établie
        if (!db || !db->is_open()) {
            throw std::runtime_error(
                "Database connection failed: Could not open database.");
        }

        pqxx::work transaction(*db);
        pqxx::result res =
            transaction.exec("SELECT username, password, player_id FROM users");

        for (auto row : res) {
            UserAccount account;
            account.username = row[0].as<std::string>();
            account.password = row[1].as<std::string>();
            account.playerID = row[2].as<int>();

            userDatabase[account.username] = account;
        }

    } catch (const std::exception& e) {
        std::cerr << "Database initialization failed: " << e.what()
                  << std::endl;
        exit(1);
    }
}

void Server::run() {
    std::cout << "Tetris Royale server running on port " << PORT << std::endl;
    // Add timeout to trigger a refresh in case a player is rejoining the server
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    while (true) {
        // Copy the set of active sockets for use with select()
        readySockets = allActiveSockets;

        // Wait for activity on any socket
        if (select(maxSocket + 1, &readySockets, nullptr, nullptr, &timeout) <
            0) {
            std::cerr << "Error with select()" << std::endl;
            return;
        }

        // Check if the server socket has a new connection
        if (FD_ISSET(serverSocket, &readySockets)) {
            handleNewConnection();
        }

        // Check all client sockets for incoming messages
        for (auto it = clients.begin(); it != clients.end(); ++it) {
            if (FD_ISSET(it->first, &readySockets)) {
                handleClientMessage(it->first);
            }
        }
    }
}

// Handle a new client connection
void Server::handleNewConnection() {
    sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    // accepte une nouvelle connexion
    int clientSocket = accept(
        serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
    if (clientSocket < 0) {
        std::cerr << "Error accepting connection" << std::endl;
        return;
    }

    // vérifie s'il n'y a pas plus de clients que prévu
    if (clients.size() > MAX_CLIENTS) {
        std::string msg = "Server is full. Connection canceled";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        close(clientSocket);
        return;
    }

    // ajoute le client à la liste des clients connectés
    auto player = std::make_shared<Player>();
    player->socket = clientSocket;
    player->username = "";
    player->isPlaying = false;
    player->gameRoom = nullptr;
    clients[clientSocket] = player;

    FD_SET(clientSocket,
           &allActiveSockets);  // ajoute le socket à l'ensemble actif

    // met à jour le plus grand numéro de socket actif
    if (clientSocket > maxSocket) {
        maxSocket = clientSocket;
    }

    std::cout << "New connection: " << clientSocket << std::endl;
}

// Handle incoming messages from a client
void Server::handleClientMessage(int clientSocket) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    if (readSafe(clientSocket, buffer, sizeof(Header)) != 0) {
        std::cerr << "[SERVER] Erreur: Déconnexion du client " << clientSocket
                  << std::endl;
        disconnectClient(clientSocket);
        return;
    }

    Header header;
    memcpy(&header, buffer, sizeof(Header));

    if (readSafe(clientSocket, buffer + sizeof(Header), header.sizeMessage) !=
        0) {
        std::cerr << "[SERVER] Erreur: Déconnexion du client " << clientSocket
                  << " (message incomplet)" << std::endl;
        disconnectClient(clientSocket);
        return;
    }

    std::cout << "[SERVER] Nouveau message reçu de " << clientSocket
              << " - Type: " << static_cast<int>(header.type) << std::endl;

    processMessage(clientSocket,
                   std::string(buffer, sizeof(Header) + header.sizeMessage));
}

// Disconnect a client and clean up resources
void Server::disconnectClient(int clientSocket) {
    removeActivePlayer(clientSocket);
    close(clientSocket);

    activePlayers.erase(clientSocket);
    std::cout << "Client " << clientSocket << " is disconnected" << std::endl;
}

// Process a message received from a client
void Server::processMessage(int clientSocket, const std::string& message) {
    if (message.size() < sizeof(Header)) {
        std::cerr << "Incomplete message" << std::endl;
        return;
    }

    // Parse the message header
    Header header;
    // récupere la partie header du message
    memcpy(&header, message.c_str(), sizeof(Header));

    // crée le gestionnaire de message selon le type
    std::shared_ptr<IMessage> msgHandler = IMessage::buildMessage(header);
    if (msgHandler) {
        // Handle the message content (after the header)
        msgHandler->handleMessage(message.substr(sizeof(Header)), clientSocket);
    } else {
        std::cerr << "Unknown message type" << std::endl;
    }
}

int Server::generatePlayerID() {
    // générateur de nb aléatoires
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dis(100000, 999999);

    return dis(gen);
}

int Server::readSafe(int clientSocket, char* buffer, size_t size) {
    size_t n = 0;
    while (n < size) {
        ssize_t result = recv(clientSocket, buffer + n, size - n, 0);

        if (result <= 0) {
            if (errno == EINTR) continue;
            std::cerr << "[SERVER] Erreur de réception, fermeture du socket "
                      << clientSocket << std::endl;
            return 1;
        }

        n += static_cast<size_t>(result);
    }

    return 0;
}

bool Server::sendMessage(int clientSocket, const void* data, size_t dataSize) {
    if (clientSocket == -1) {
        std::cerr << "Error: Invalid socket" << std::endl;
        return false;
    }

    // convertit les données en un buffer de type char* (pr envoyer octet par
    // octet)
    const char* buffer = static_cast<const char*>(data);
    size_t totalSent = 0;
    size_t bytesLeft = dataSize;  // nombre d'octets restants a envoyer

    while (totalSent < dataSize) {
        // envoie les octets restants a partir de la position actuelle du buffer
        ssize_t sent = send(clientSocket, buffer + totalSent, bytesLeft, 0);

        if (sent == -1) {
            perror("Error sending byte");
            return false;
        }

        totalSent += sent;
        bytesLeft -= sent;
    }
    return true;
}

void Server::addActivePlayer(std::shared_ptr<Player> player) {
    FD_SET(player->socket, &allActiveSockets);
    clients[player->socket] = player;
    if (player->socket > maxSocket) {
        maxSocket = player->socket;
    }
}

void Server::removeActivePlayer(int socketPlayer) {
    FD_CLR(socketPlayer, &allActiveSockets);
}

int Server::findRoomIndex(
    std::vector<std::pair<int, std::shared_ptr<GameRoom>>>& rooms, int roomId) {
    // dichotomique search
    int left = 0;
    int right = int(rooms.size() - 1);
    if (rooms.empty()) return -1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (rooms[mid].first == roomId) {
            return mid;
        } else if (rooms[mid].first < roomId) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    if (rooms[left].first == roomId) {
        return left;
    }
    return -1;
}

int Server::addGameRoom(std::shared_ptr<GameRoom> room, int clientSocket) {
    if (room == nullptr) {
        return -1;
    }
    removeActivePlayer(clientSocket);
    gameRooms_.push_back({room->getRoomId(), room});
    return room->getRoomId();
}

void Server::removeGameRoom(int roomId) {
    int index = findRoomIndex(gameRooms_, roomId);
    if (index != -1) {
        gameRooms_.erase(gameRooms_.begin() + index);
    }
}

std::string Server::getUserAccount(int playerID) {
    auto it = std::find_if(userDatabase.begin(), userDatabase.end(),
                           [playerID](const auto& pair) {
                               return pair.second.playerID == playerID;
                           });
    if (it != userDatabase.end()) {
        return it->second.username;
    }
    return "";
}
void Server::broadcastToLobby(int clientSocket, const void* data, size_t size) {
    auto lobby = getLobbyForClient(clientSocket);
    if (lobby) {
        for (const auto& [playerSocket, _] : lobby->getPlayers()) {
            if (playerSocket != clientSocket) {
                sendMessage(playerSocket, data, size);
            }
        }
    }
}

std::shared_ptr<Player> Server::getPlayer(int socket) {
    auto it = clients.find(socket);

    if (it != clients.end()) {
        if (!it->second->gameState) {  // Vérifie si gameState n'est
                                      // pas encore initialisé
            it->second->gameState = std::make_shared<GameState>();
        }
        return it->second;
    }

    return nullptr;
}

std::shared_ptr<GameRoom> Server::getLobbyForClient(int socket) {
    for (auto& [id, room] : gameRooms_) {
        if (room->getPlayers().find(socket) != room->getPlayers().end()) {
            return room;
        }
    }
    return nullptr;
}

bool Server::checkWinner(uint32_t score) {
    // Temporary implementation - compare with other players' scores
    static uint32_t highScore = 0;
    if (score > highScore) {
        highScore = score;
        return true;
    }
    return false;
}

void Server::cleanupLobby(int clientSocket) {
    auto lobby = getLobbyForClient(clientSocket);
    if (lobby) {
        auto& players = lobby->getPlayers();
        if (players.find(clientSocket) != players.end()) {
            lobby->removePlayer(
                players.at(clientSocket));  // Passer l'objet std::shared_ptr<Player>
        }

        if (lobby->getPlayers().empty()) {  // Vérification via le getter
            gameRooms_.erase(
                std::remove_if(gameRooms_.begin(), gameRooms_.end(),
                               [&](const auto& roomPair) {
                                   return roomPair.second == lobby;
                               }),
                gameRooms_.end());
        }
    }
}
