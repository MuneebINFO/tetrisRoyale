#include "Server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "Message.h"
#include "Player.h"
Server::Server() : socket_(0), connected_(false) {}

Server::~Server() {
    if (connected_) {
        disconnect();
    }
}

void Server::setPlayer(std::shared_ptr<Player> player) { player_ = player; }
void Server::setName(std::string name) { player_->setName(name); }
void Server::setPlayerID(int id) {
    playerID_ = id;
    player_->setPlayerId(id);
}

int Server::connectToServer() {
    const std::string defaultIp = "127.0.0.1";
    const int defaultPort = 8080;
    if (connected_) {
        return 0;
    }
    // Récupération des variables d'environnement avec vérification
    char* env_ip = getenv("IP_SERVEUR");
    char* env_port = getenv("PORT_SERVEUR");

    std::string ip = (env_ip != nullptr) ? std::string(env_ip) : defaultIp;
    std::string port_str = (env_port != nullptr) ? std::string(env_port)
                                                 : std::to_string(defaultPort);

    uint16_t port;

    try {
        int port_val = std::stoi(port_str);
        if (port_val <= 0 || port_val > 65535) {
            port = defaultPort;
        } else {
            port = static_cast<uint16_t>(port_val);
        }
    } catch (std::exception& e) {
        port = defaultPort;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[CLIENT] Erreur : socket()");
        return -1;
    }

    sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, defaultIp.c_str(), &serv_addr.sin_addr) <= 0) {
        perror("[CLIENT] Erreur : inet_pton()");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, reinterpret_cast<sockaddr*>(&serv_addr),
                sizeof(serv_addr)) < 0) {
        perror("[CLIENT] Erreur : connect()");
        close(sockfd);
        return -1;
    }

    socket_ = sockfd;
    connected_ = true;
    return 0;
}

int Server::sendMessage(char* buffer, size_t size) {
    if (!connected_) {
        std::cerr << "[CLIENT] Erreur: Pas connecté au serveur." << std::endl;
        return 1;
    }

    size_t n = 0;
    while (n < size) {
        ssize_t result = send(socket_, buffer + n, size - n, 0);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EPIPE) {
                std::cerr << "[CLIENT] Erreur: Connexion fermée par le serveur."
                          << std::endl;
                return 1;
            } else {
                perror("[CLIENT] Erreur d'envoi");
                return 1;
            }
        }
        n += static_cast<size_t>(
            result);  // Mise à jour du nombre total d'octets envoyés
    }

    return 0;
}

int Server::receiveMessage(char* buffer) {
    if (!connected_) {
        return -1;
    }
    size_t totalRead = 0;
    size_t n = sizeof(HeaderResponse);
    if (readSafe(buffer, n) != 0) return -1;
    totalRead += n;

    HeaderResponse header;
    memcpy(&header, buffer, sizeof(HeaderResponse));
    n = header.sizeMessage;

    if (readSafe(buffer + sizeof(HeaderResponse), n) != 0) return -1;
    totalRead += n;

    return int(totalRead);
}

std::string Server::receive() {
    char buffer[BUFFER_SIZE * 2];
    memset(buffer, 0, sizeof(buffer));

    // lecture des données depuis le socket
    recv(socket_, buffer, sizeof(buffer), 0);
    buffer[BUFFER_SIZE] = '\0';  // ajout du caractère de fin
    return std::string(buffer,
                       sizeof(buffer));  // conversion en std::string
}

int Server::readSafe(char* buffer, size_t size) {
    size_t n = 0;
    while (n < size) {
        size_t result = recv(socket_, buffer + n, size - n, 0);
        if (result <= 0) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EPIPE) {
                return 1;
            }
        }
        n += result;
    }
    return 0;
}

void Server::disconnect() {
    if (connected_) {
        close(socket_);
        connected_ = false;
    }
}

void Server::handleSocialRequest(SOCIAL_TYPE type, FriendHeader friendHeader) {
    Header header;
    header.type = MESSAGE_TYPE::SOCIAL;
    header.sizeMessage = sizeof(SocialHeader) + sizeof(FriendHeader);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, &header, sizeof(Header));

    SocialHeader socialHeader;
    socialHeader.type = type;
    memcpy(buffer + sizeof(Header), &socialHeader, sizeof(SocialHeader));
    memcpy(buffer + sizeof(Header) + sizeof(SocialHeader), &friendHeader,
           sizeof(FriendHeader));

    sendMessage(buffer,
                sizeof(Header) + sizeof(SocialHeader) + sizeof(FriendHeader));
}

std::vector<FriendHeader> Server::receiveFriendListHeader() {
    std::vector<FriendHeader> friendList;

    char bufferNul[sizeof(int)];
    memset(bufferNul, 0, sizeof(bufferNul));
    recv(socket_, bufferNul, sizeof(int), 0);

    int count;
    memcpy(&count, bufferNul, sizeof(int));

    for (int i = 0; i < count; i++) {
        char buffer[sizeof(FriendHeader)];
        memset(buffer, 0, sizeof(buffer));
        recv(socket_, buffer, sizeof(FriendHeader), 0);
        FriendHeader header;
        memcpy(&header, buffer, sizeof(FriendHeader));
        friendList.push_back(header);
    }
    return friendList;
}

std::vector<ChatHeader> Server::receiveChatListHeader() {
    std::vector<ChatHeader> friendList;

    char buffer[sizeof(int)];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytesReceived = recv(socket_, buffer, sizeof(int), 0);
    int count;
    memcpy(&count, buffer, sizeof(int));
    for (int i = 0; i < count; i++) {
        char buffer[sizeof(ChatHeader)];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesReceived = recv(socket_, buffer, sizeof(ChatHeader), 0);
        ChatHeader header;
        memcpy(&header, buffer, sizeof(ChatHeader));
        friendList.push_back(header);
    }
    return friendList;
}
