#include "message.h"

#include <pthread.h>
#include <sys/socket.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include "gameServer.h"


// Factory method to create the appropriate message handler based on the header
std::shared_ptr<IMessage> IMessage::buildMessage(const Header& header) {
    switch (header.type) {
        case MESSAGE_TYPE::ACCOUNT:
            return std::make_shared<AccountMessage>();
        case MESSAGE_TYPE::GAME:
            return std::make_shared<GameMessage>();  // Create a GameMessage
        case MESSAGE_TYPE::LOBBY:
            return std::make_shared<LobbyMessage>();  // Create a LobbyMessage
        case MESSAGE_TYPE::CHAT:
            return std::make_shared<ChatMessage>();
        case MESSAGE_TYPE::SYSTEM:
            return std::make_shared<SystemMessage>();  // Create a SystemMessage
                                                       // handler
        case MESSAGE_TYPE::SOCIAL:
            return std::make_shared<SocialMessage>();
        case MESSAGE_TYPE::FRIEND:
        case MESSAGE_TYPE::ACCEPTFRIEND:
        case MESSAGE_TYPE::FRIENDLIST:
        case MESSAGE_TYPE::REQUESTFRIENDLIST:
        default:
            std::cerr << "[SERVER] ERREUR: Type de message inconnu"
                      << std::endl;
            return nullptr;
    }
}

void AccountMessage::handleMessage(const std::string& message,
                                   int clientSocket) {
    AccountHeader header;
    memcpy(&header, message.c_str(), sizeof(AccountHeader));

    std::string username(header.username);
    std::string password(header.password);

    switch (header.type) {
        case ACCOUNT_TYPE::LOGIN:
            handleLogin(username, password, clientSocket);
            break;
        case ACCOUNT_TYPE::REGISTER:
            handleRegister(username, password, clientSocket);
            break;
        case ACCOUNT_TYPE::LOGOUT:
            handleLogout(username, clientSocket);
            break;
        default:
            break;
    }
}

void AccountMessage::handleLogin(const std::string& username,
    const std::string& password,
    int clientSocket) {
    Server& server = Server::getInstance();
    auto& clients = server.getClients();

    char buffer[BUFFER_SIZE];
    HeaderResponse header = {MESSAGE_TYPE::ACCOUNT,
    sizeof(AccountResponseHeader)};
    AccountResponseHeader responseHeader;
    memcpy(buffer, &header, sizeof(HeaderResponse));
    size_t size = sizeof(HeaderResponse) + sizeof(AccountResponseHeader);

    // ouvre une transaction pour exécuter la requête SQL
    pqxx::work transaction(*server.getDB());

    // requête SQL pour récupérer le mot de passe et l'ID du joueur
    pqxx::result res = transaction.exec_params(
    "SELECT password, player_id FROM users WHERE username = $1", username);

    // vérifie si l'utilisateur existe dans la bdd
    if (res.empty() || res[0][0].is_null() || res[0][1].is_null()) {
        std::cout << "User not found in DataBase" << std::endl;
        responseHeader.responseType = ACCOUNT_RESPONSE::ERROR;
        responseHeader.idPlayer = -1;
        strcpy(responseHeader.message, "Username or password is incorrect");

        memcpy(buffer + sizeof(HeaderResponse), &responseHeader,
        sizeof(AccountResponseHeader));
        server.sendMessage(clientSocket, buffer, size);
        return;
    }

    // récupère les informations de l'utilisateur depuis la bdd
    std::string passwordInDB = res[0][0].as<std::string>();
    int playerID = res[0][1].as<int>();

    // vérifie si le mdp saisi correspond à celui stocké
    if (passwordInDB == password) {
    std::cout << "Login successful for user: " << username << std::endl;
    responseHeader.responseType = ACCOUNT_RESPONSE::VALID;
    responseHeader.idPlayer = playerID;
    strcpy(responseHeader.message, "LOGIN_SUCCESS");
    
    // assigner l'ID du joueur à la connexion
    auto player = std::make_shared<Player>();
    player->ID = playerID;
    player->username = username;
    player->socket = clientSocket;
    player->gameRoom = nullptr;
    clients[clientSocket] = player;

    } else {
    std::cout << "Incorrect password for user: " << username << std::endl;
    responseHeader.responseType = ACCOUNT_RESPONSE::ERROR;
    responseHeader.idPlayer = -1;
    strcpy(responseHeader.message, "Username or password is incorrect");
    }

    memcpy(buffer + sizeof(HeaderResponse), &responseHeader,
    sizeof(AccountResponseHeader));
    server.sendMessage(clientSocket, buffer, size);
}


void AccountMessage::handleRegister(const std::string& username,
                                    const std::string& password,
                                    int clientSocket) {
    Server& server = Server::getInstance();
    char buffer[BUFFER_SIZE];
    HeaderResponse header = {MESSAGE_TYPE::ACCOUNT,
                             sizeof(AccountResponseHeader)};
    AccountResponseHeader responseHeader;
    memcpy(buffer, &header, sizeof(HeaderResponse));
    size_t size = sizeof(HeaderResponse) + sizeof(AccountResponseHeader);

    // ouvre une transaction pour exécuter la requête SQL
    pqxx::work transaction(*server.getDB());

    // vérifie si l'utilisateur existe déjà en BDD
    pqxx::result res = transaction.exec_params(
        "SELECT COUNT(*) FROM users WHERE username = $1", username);

    if (res[0][0].as<int>() > 0) {  // Si l'utilisateur existe deja
        std::cout << "User already exists: " << username << std::endl;
        responseHeader.responseType = ACCOUNT_RESPONSE::EXIST;
        responseHeader.idPlayer = -1;
        strcpy(responseHeader.message, "User already exists");
    } else {
        int playerID = server.generatePlayerID();

        std::cout << "Registration successful: " << username << " with ID "
                  << playerID << std::endl;

        // insere les nouvelles informations dans la BDD
        transaction.exec_params(
            "INSERT INTO users (username, password, player_id) VALUES ($1, $2, "
            "$3)",
            username, password, playerID);

        transaction.commit();

        responseHeader.responseType = ACCOUNT_RESPONSE::VALID;
        responseHeader.idPlayer = playerID;
    }

    memcpy(buffer + sizeof(HeaderResponse), &responseHeader,
           sizeof(AccountResponseHeader));
    server.sendMessage(clientSocket, buffer, size);
}

void AccountMessage::handleLogout(const std::string& username,
                                  int clientSocket) {
    Server& server = Server::getInstance();

    if (clientSocket != -1) {
        server.disconnectClient(clientSocket);
        std::cout << "LOGOUT DONE: " << username << std::endl;
    } else {
        std::cout << "LOGOUT FAILED: " << std::endl;
    }
}

void GameMessage::handleMessage(const std::string& message, int clientSocket) {
    if (message.size() < sizeof(GameTypeHeader)) {
        std::cerr << "[SERVER] Erreur: Message de jeu invalide reçu (taille "
                     "trop petite)."
                  << std::endl;
        return;
    }

    GameTypeHeader gameHeader;
    memcpy(&gameHeader, message.c_str(), sizeof(GameTypeHeader));
    const char* msg = message.data() + sizeof(GameTypeHeader);

    switch (gameHeader.type) {
        case GAME_TYPE::SPAWN_TETRAMINO:
            handleSpawnTetraminoMsg(msg, clientSocket);
            break;
        case GAME_TYPE::ROTATE:
            handleRotateMsg(msg, clientSocket);
            break;
        case GAME_TYPE::MOVE:
            handleMoveMsg(msg, clientSocket);
            break;
        case GAME_TYPE::MALUS:
            handleMalusMsg(msg, clientSocket);
            break;
        case GAME_TYPE::START:
            handleStartMsg(clientSocket);
            break;
        case GAME_TYPE::END:
            handleEndMsg(clientSocket);
            break;
        case GAME_TYPE::MALUS_CONFIRM:
            handleConfirmMalusMsg(msg, clientSocket);
            break;
        case GAME_TYPE::BONUS:
        case GAME_TYPE::LOCK:
        default:
            std::cerr << "[SERVER] Erreur: Type de message de jeu inconnu."
                      << std::endl;
            break;
    }
}

void GameMessage::handleSpawnTetraminoMsg(const char* details,
                                          int clientSocket) {
    GameServer game;
    Server& server = Server::getInstance();
    auto player = server.getPlayer(clientSocket);
    if (!player) return;

    SpawnTetraminoPayload payload;
    memcpy(&payload, details, sizeof(SpawnTetraminoPayload));

    if (!game.handleSpawnTetramino(player, payload)) {
        HeaderResponse header;
        header.type = MESSAGE_TYPE::GAME;
        header.sizeMessage = sizeof(GameTypeHeader);

        GameTypeHeader gameHeader;
        gameHeader.type = GAME_TYPE::LOST;

        char LosingMsg[sizeof(HeaderResponse) + sizeof(GameTypeHeader)];
        memcpy(LosingMsg, &header, sizeof(HeaderResponse));
        memcpy(LosingMsg + sizeof(HeaderResponse), &gameHeader, sizeof(GameTypeHeader));

        if (player->gameRoom->countAlivePlayers() == 2) {
            GameTypeHeader gameHeader2;
            gameHeader2.type = GAME_TYPE::WIN;

            char WiningMsg[sizeof(HeaderResponse) + sizeof(GameTypeHeader)];
            memcpy(WiningMsg, &header, sizeof(HeaderResponse));
            memcpy(WiningMsg + sizeof(HeaderResponse), &gameHeader2, sizeof(GameTypeHeader));
            server.sendMessage(game.getOpponentSocket(player), WiningMsg, sizeof(WiningMsg));
        }

        player->gameRoom->removePlayer(player);
        server.sendMessage(clientSocket, LosingMsg, sizeof(LosingMsg));
    }
}

void GameMessage::handleMoveMsg(const char* details, int clientSocket) {
    GameServer game;
    Server& server = Server::getInstance();
    auto player = server.getPlayer(clientSocket);
    if (!player) return;

    MovementPayload payload;
    memcpy(&payload, details, sizeof(MovementPayload));

    GameUpdateHeader update;
    game.handleMove(player, update, payload);

    HeaderResponse header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(GameUpdateHeader);

    GameTypeHeader gameHeader;
    if (update.type == GAME_TYPE::MOVE) {
        gameHeader.type = GAME_TYPE::MOVE;
    } else {
        gameHeader.type = GAME_TYPE::LOCK;
    }

    char response[sizeof(HeaderResponse) + sizeof(GameTypeHeader) + sizeof(GameUpdateHeader)];
    memcpy(response, &header, sizeof(HeaderResponse));
    memcpy(response + sizeof(HeaderResponse), &gameHeader, sizeof(GameTypeHeader));
    memcpy(response + sizeof(HeaderResponse) + sizeof(GameTypeHeader), &update, sizeof(GameUpdateHeader));

    server.sendMessage(clientSocket, response, sizeof(response));
}

void GameMessage::handleRotateMsg(const char* details, int clientSocket) {
    GameServer game;
    Server& server = Server::getInstance();
    auto player = server.getPlayer(clientSocket);
    if (!player) return;

    RotationPayload payload;
    memcpy(&payload, details, sizeof(RotationPayload));

    GameUpdateHeader update;
    game.handleRotate(player, update, payload);

    HeaderResponse header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(GameUpdateHeader);

    GameTypeHeader gameHeader;
    gameHeader.type = GAME_TYPE::ROTATE;

    char response[sizeof(HeaderResponse) + sizeof(GameTypeHeader) + sizeof(GameUpdateHeader)];
    memcpy(response, &header, sizeof(HeaderResponse));
    memcpy(response + sizeof(HeaderResponse), &gameHeader, sizeof(GameTypeHeader));
    memcpy(response + sizeof(HeaderResponse) + sizeof(GameTypeHeader), &update, sizeof(GameUpdateHeader));

    server.sendMessage(clientSocket, response, sizeof(response));
}

void GameMessage::handleConfirmMalusMsg(const char* details, int clientSocket) {
    GameServer game;
    Server& server = Server::getInstance();

    auto player = server.getPlayer(clientSocket);
    if (!player) return;

    MalusPayload payload;
    memcpy(&payload, details, sizeof(MalusPayload));

    game.handleConfirmMalus(player, payload);
}

void GameMessage::handleMalusMsg(const char* details, int clientSocket) {
    Server& server = Server::getInstance();
    auto sender = server.getPlayer(clientSocket);

    if (!sender) {
        std::cerr << "[SERVER] Erreur : joueur introuvable pour le socket "
                  << clientSocket << std::endl;
        return;
    }

    MalusPayload payload;
    memcpy(&payload, details, sizeof(MalusPayload));

    std::cout << "[SERVER] Type de Malus reçu " << payload.malusType
              << " envoyé par joueur " << clientSocket << " au joueur "
              << payload.targetSocket << std::endl;

    // Envoyer le malus au joueur ciblé
    server.sendMessage(payload.targetSocket, &payload, sizeof(payload));
}

void GameMessage::handleBonusMsg(const char* details, int clientSocket) {
    Server& server = Server::getInstance();
    auto sender = server.getPlayer(clientSocket);

    if (!sender) {
        std::cerr << "[SERVER] Erreur : joueur introuvable pour le socket "
                  << clientSocket << std::endl;
        return;
    }

    BonusPayload payload;
    memcpy(&payload, details, sizeof(BonusPayload));

    std::cout << "[SERVER] Type de Bonus reçu " << payload.type
              << " envoyé par joueur " << clientSocket << std::endl;

    server.broadcastToLobby(clientSocket, &payload, sizeof(BonusPayload));
}

void GameMessage::handleStartMsg(int clientSocket) {
    Server& server = Server::getInstance();
    auto lobby = server.getLobbyForClient(clientSocket);

    if (lobby) {
        GameUpdateHeader update;
        update.type = GAME_TYPE::START;
        server.broadcastToLobby(clientSocket, &update, sizeof(update));
    }
}

void GameMessage::handleEndMsg(int clientSocket) {
    Server& server = Server::getInstance();
    auto player = server.getPlayer(clientSocket);

    if (player) {
        GameEndHeader end{
            .type = GAME_TYPE::END,
            .score = uint32_t(player->gameState->points_),
            .isWinner = server.checkWinner(player->gameState->points_)};
        server.broadcastToLobby(clientSocket, &end, sizeof(end));
        server.cleanupLobby(clientSocket);
    }
}

void ChatMessage::handleMessage(const std::string& message, int clientSocket) {
    std::cout << "Chat message: " << message << clientSocket << std::endl;
    // Add logic to process chat messages
}
/*
 * Handle lobby messages
 */
void LobbyMessage::handleMessage(const std::string& message, int clientSocket) {
    LobbyHeader header;
    memcpy(&header, message.c_str(), sizeof(LobbyHeader));

    switch (header.type) {
        case LOBBY_TYPE::CREATE:
            handleCreate(message, clientSocket);
            break;
        case LOBBY_TYPE::MODIFY:
            handleModify(message);
            break;
        case LOBBY_TYPE::JOIN:
            handleJoin(message, clientSocket);
            break;
        case LOBBY_TYPE::LEAVE:
            handleLeave(message, clientSocket);
            break;
        case LOBBY_TYPE::START:
            handleStart(message, clientSocket);
            break;
        case LOBBY_TYPE::INVITE:
            handleInviteFriend(message, clientSocket);
            break;
        default:
            break;
    }
}

void LobbyMessage::handleCreate(const std::string& message, int clientSocket) {
    Server& server = Server::getInstance();
    LobbyHeader header;
    std::map<int, std::shared_ptr<Player>>& clients = server.getClients();

    memcpy(&header, message.c_str(), sizeof(LobbyHeader));
    std::shared_ptr<GameRoom> gameRoom = std::make_shared<GameRoom>();
    gameRoom->setGroupeLeader(clients[clientSocket]->ID);
    gameRoom->setGameMode(header.gameMode);
    gameRoom->setNbrPlayers(header.nbPlayers);
    std::vector<std::pair<int, std::shared_ptr<GameRoom>>>& gameRooms =
        server.getGameRooms();
    if (gameRooms.size() == 0) {
        gameRoom->setRoomId(1);
    } else {
        gameRoom->setRoomId(gameRooms.back().first + 1);
    }
    int roomId = server.addGameRoom(gameRoom, clientSocket);
    if (roomId == -1) {
        std::cerr << "Failed to create game room" << std::endl;
        return;
    }
    gameRoom->addPlayer(clients[clientSocket], true);
    LobbyResponseHeader response = {LOBBY_RESPONSE::CREATED, roomId};
    server.sendMessage(clientSocket, &response, sizeof(response));

    std::cout << "Game room created with ID " << roomId << std::endl;
    pthread_t thread;
    pthread_create(&thread, nullptr, GameRoom::loop, gameRoom.get());
}

void LobbyMessage::handleModify(const std::string& message) {
    Server& server = Server::getInstance();
    LobbyHeader header;
    memcpy(&header, message.c_str(), sizeof(LobbyHeader));
    std::vector<std::pair<int, std::shared_ptr<GameRoom>>>& gameRooms =
        server.getGameRooms();
    int index = server.findRoomIndex(gameRooms, header.idRoom);

    if (index != -1) {
        std::shared_ptr<GameRoom> gameRoom = gameRooms[index].second;
        gameRoom->setGameMode(header.gameMode);
        gameRoom->setNbrPlayers(header.nbPlayers);
        gameRoom->sendLobbyUpdate();
    }
}

void LobbyMessage::handleJoin(const std::string& message, int clientSocket) {
    Server& server = Server::getInstance();
    pqxx::connection* db = server.getDB();
    pqxx::work transaction(*db);

    LobbyHeader header;
    memcpy(&header, message.c_str(), sizeof(LobbyHeader));
    std::vector<std::pair<int, std::shared_ptr<GameRoom>>>& gameRooms =
        server.getGameRooms();
    std::map<int, std::shared_ptr<Player>>& clients = server.getClients();
    int index = server.findRoomIndex(gameRooms, header.idRoom);
    if (index != -1) {
        pqxx::result res = transaction.exec_params(
            "SELECT * FROM game_invitation WHERE id_player_invited = $1 AND "
            "room_id = $2",
            clients[clientSocket]->ID, header.idRoom);

        if (res.empty()) {
            std::cerr << "std::shared_ptr<Player> not invited to the game room" << std::endl;
            return;
        }

        bool asGamer = res[0]["as_gamer"].as<bool>();

        server.removeActivePlayer(clientSocket);
        gameRooms[index].second->addPlayer(clients[clientSocket], asGamer);
        gameRooms[index].second->sendParticipantList(clients[clientSocket]);
    }
}

void LobbyMessage::handleLeave(const std::string& message, int clientSocket) {
    Server& server = Server::getInstance();
    LobbyHeader header;
    memcpy(&header, message.c_str(), sizeof(LobbyHeader));
    std::vector<std::pair<int, std::shared_ptr<GameRoom>>>& gameRooms =
        server.getGameRooms();
    int index = server.findRoomIndex(gameRooms, header.idRoom);
    std::map<int, std::shared_ptr<Player>>& clients = server.getClients();
    if (index != -1) {
        gameRooms[index].second->playerLeaves(clients[clientSocket]);
    } else {
        std::cerr << "Game room not found" << std::endl;
    }
}

void LobbyMessage::handleStart(const std::string& msg, int clientSocket) {
    Server& server = Server::getInstance();
    std::vector<std::pair<int, std::shared_ptr<GameRoom>>>& gameRooms =
        server.getGameRooms();
    std::map<int, std::shared_ptr<Player>>& clients = server.getClients();
    LobbyHeader header;
    memcpy(&header, msg.c_str(), sizeof(LobbyHeader));
    int index = server.findRoomIndex(gameRooms, header.idRoom);
    if (index != -1) {
        gameRooms[index].second->handleStartRequest(clients[clientSocket]);
    }
}

void LobbyMessage::handleInviteFriend(const std::string& message,
                                      int clientSocket) {
    LobbyInviteFriend invite;
    memcpy(&invite, message.substr(sizeof(LobbyHeader)).c_str(),
           sizeof(LobbyInviteFriend));
    // Add logic to invite a friend to the lobby
    std::string userName(invite.nameInviting);
    int roomId = invite.idRoom;
    std::string gameMode(invite.gameMode);
    bool asGamer = invite.asGamer;

    Server& server = Server::getInstance();
    pqxx::connection* db = server.getDB();

    auto& userDatabase = server.getUserDatabase();
    auto& clientsConnected = server.getClients();
    char buffer[BUFFER_SIZE];
    ACCOUNT_RESPONSE result;

    if (userDatabase.find(userName) == userDatabase.end()) {
        result = ACCOUNT_RESPONSE::ERROR;
        memcpy(buffer, &result, sizeof(ACCOUNT_RESPONSE::ERROR));
        server.sendMessage(clientSocket, buffer, sizeof(result));
        std::cout << "User not found" << std::endl;
        return;
    }
    UserAccount userAccount = userDatabase[userName];
    int invitedPlayerID = userAccount.playerID;
    std::shared_ptr<Player> invitingPlayer = clientsConnected[clientSocket];
    int invitingPlayerId = invitingPlayer->ID;

    std::string query = R"(
            INSERT INTO game_invitation (id_player_invited, id_player_inviting, room_id, as_gamer, game_mode)
            SELECT $1, $2, $3, $4, $5
        )";
    pqxx::work transaction(*db);
    pqxx::result res = transaction.exec_params(
        query, invitedPlayerID, invitingPlayerId, roomId, asGamer, gameMode);
    transaction.commit();
}

int LobbyMessage::serializeErrorLobby(const std::string& msg, char* buffer) {
    HeaderResponse response = {MESSAGE_TYPE::LOBBY,
                               sizeof(LobbyResponseHeader)};
    LobbyResponseHeader lobbyResponse = {LOBBY_RESPONSE::ERROR, -1};
    LobbyErrorResponse lobbyErrorResponse = {uint32_t(msg.size())};
    response.sizeMessage = sizeof(LobbyResponseHeader) +
                           sizeof(LobbyErrorResponse) + uint32_t(msg.size());
    memcpy(buffer, &response, sizeof(HeaderResponse));
    memcpy(buffer + sizeof(HeaderResponse), &lobbyResponse,
           sizeof(LobbyResponseHeader));
    memcpy(buffer + sizeof(HeaderResponse) + sizeof(LobbyResponseHeader),
           &lobbyErrorResponse, sizeof(LobbyErrorResponse));
    memcpy(buffer + sizeof(HeaderResponse) + sizeof(LobbyResponseHeader) +
               sizeof(LobbyErrorResponse),
           msg.c_str(), uint32_t(msg.size()));
    return int(sizeof(HeaderResponse) + sizeof(LobbyResponseHeader) +
               sizeof(LobbyErrorResponse) + uint32_t(msg.size()));
}

void SystemMessage::handleMessage(const std::string& message,

                                  int clientSocket) {
    std::cout << "System message: " << message << clientSocket << std::endl;
}

void SocialMessage::handleMessage(const std::string& message,
                                  int clientSocket) {
    SocialHeader header;
    memcpy(&header, message.c_str(), sizeof(SocialHeader));
    switch (header.type) {
        case SOCIAL_TYPE::GET_FRIEND_REQUEST:
            getFriendRequest(message, clientSocket);
            break;
        case SOCIAL_TYPE::GET_FRIEND_LIST:
            getFriendList(message, clientSocket);
            break;
        case SOCIAL_TYPE::INVITE:
            inviteFriend(message, clientSocket);
            break;
        case SOCIAL_TYPE::ACCEPT:
            acceptFriendinvite(message, clientSocket);
            break;
        case SOCIAL_TYPE::GET_LOBBY_INVITE:
            handleGetLobbyInvite(message, clientSocket);
            break;
        case SOCIAL_TYPE::SEND_MESSAGE:
            sendMessages(message, clientSocket);
            break;
        case SOCIAL_TYPE::GET_MESSAGE:
            getMessages(message, clientSocket);
            break;
        case SOCIAL_TYPE::INCHATROOM:
            inChatRoom(message, clientSocket);
            break;
        case SOCIAL_TYPE::LEAVE_CHAT:
            leaveChat(message, clientSocket);
            break;
    }
}

void SocialMessage::handleGetLobbyInvite(const std::string& message,
                                         int clientSocket) {
    std::cout << message << std::endl;  // just to avoid warning
    Server& server = Server::getInstance();
    pqxx::connection* db = server.getDB();
    auto& clientsConnected = server.getClients();
    int playerID = clientsConnected[clientSocket]->ID;

    char buffer[BUFFER_SIZE];
    HeaderResponse headerResponse = {MESSAGE_TYPE::SOCIAL,
                                     sizeof(HeaderResponse)};
    LobbyInvitation lobbyInvite;
    SocialResponseLobbyInvite response;
    int invitation = 0;
    size_t baseSize =
        sizeof(HeaderResponse) + sizeof(SocialResponseLobbyInvite);
    size_t size = sizeof(HeaderResponse) + sizeof(response);
    response.numberOfInvitations = 0;

    std::string query = R"(
        SELECT * FROM game_invitation
        WHERE id_player_invited = $1
    )";
    pqxx::work transaction(*db);
    pqxx::result res = transaction.exec_params(query, playerID);

    // Loop over all the invitations the player has
    for (const pqxx::row& row : res) {
        memset(&lobbyInvite, 0, sizeof(LobbyInvitation));

        lobbyInvite.roomId = row["room_id"].as<int>();
        int idPlayerInviting = row["id_player_inviting"].as<int>();
        lobbyInvite.idPlayerInviting = idPlayerInviting;
        std::string invitingPlayer = server.getUserAccount(idPlayerInviting);
        memcpy(lobbyInvite.invitingPlayer, invitingPlayer.c_str(),
               invitingPlayer.size());
        std::string gameMode = row["game_mode"].as<std::string>();
        memcpy(lobbyInvite.gameMode, gameMode.c_str(), gameMode.size());
        lobbyInvite.asGamer = row["as_gamer"].as<bool>();

        if (BUFFER_SIZE < size + sizeof(LobbyInvitation)) {
            // In case the buffer is too small for all the invitation,
            // presend.
            headerResponse.sizeMessage =
                uint32_t(size - sizeof(HeaderResponse));
            memcpy(buffer, &headerResponse, sizeof(HeaderResponse));
            response.completed = false;
            memcpy(buffer + sizeof(HeaderResponse), &response,
                   sizeof(response));
            server.sendMessage(clientSocket, buffer, size);
            invitation = 0;
            response.numberOfInvitations = 0;
            size = baseSize;
        }
        response.numberOfInvitations = ++invitation;
        memcpy(buffer + size, &lobbyInvite, sizeof(LobbyInvitation));
        size = baseSize + (sizeof(LobbyInvitation) * invitation);
    }
    headerResponse.sizeMessage = uint32_t(size - sizeof(HeaderResponse));
    memcpy(buffer, &headerResponse, sizeof(HeaderResponse));
    response.completed = true;
    memcpy(buffer + sizeof(HeaderResponse), &response, sizeof(response));
    server.sendMessage(clientSocket, buffer, size);
}

void SocialMessage::inviteFriend(const std::string& message, int clientSocket) {
    Server& server = Server::getInstance();
    auto& userDatabase = server.getUserDatabase();
    auto& clientsConnected = server.getClients();
    
    FriendHeader friendHeader;
    memcpy(&friendHeader, message.c_str() + sizeof(SocialHeader),
           sizeof(FriendHeader));
    std::string userNameStr(friendHeader.username);
    
    HeaderResponse response;
    response.type = MESSAGE_TYPE::INVITEFRIEND;
    std::shared_ptr<Player> invitingPlayer = clientsConnected[clientSocket];
    
    if (userDatabase.find(userNameStr) == userDatabase.end()) {
        response.status = FRIEND_REQUEST_STATUS::PLAYER_NOT_FOUND;
    } else {
        UserAccount userAccount = userDatabase[userNameStr];
        int invitedPlayerID = userAccount.playerID;
        int invitingPlayerId = invitingPlayer->ID;
        
        if (invitingPlayerId == invitedPlayerID) {
            std::cout << "std::shared_ptr<Player> " << invitingPlayerId << " tried to add themselves as a friend" << std::endl;
            response.status = FRIEND_REQUEST_STATUS::SELF_ADD_FORBIDDEN;
        } else {
            response.status = processInviteRequest(invitingPlayerId, invitedPlayerID, server.getDB());
        }
    }
    
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, &response, sizeof(HeaderResponse));
    if(server.sendMessage(clientSocket, buffer, sizeof(HeaderResponse)) == 0) {
        std::cout << "Failed to send friend request status to client " << clientSocket << std::endl;
    } else {
        std::cout << "Friend request status sent to client " << clientSocket << std::endl;
    }
}


FRIEND_REQUEST_STATUS SocialMessage::processInviteRequest(int invitingPlayerId, int invitedPlayerID, pqxx::connection* db) {
    pqxx::work txn(*db);
    
    // Check if already in friend list
    pqxx::result result = txn.exec_params(
        "SELECT friend_id FROM friend_list WHERE player_id = $1 AND friend_id = $2;",
        invitingPlayerId, invitedPlayerID
    );
    
    if (!result.empty()) {
        return FRIEND_REQUEST_STATUS::ALREADY_IN_LIST;
    }
    
    // Check if request already sent
    pqxx::result res = txn.exec_params(
        "SELECT 1 FROM friend_request WHERE id_player_inviting = $1 AND id_player_invited = $2;",
        invitingPlayerId, invitedPlayerID
    );
    
    if (!res.empty()) {
        return FRIEND_REQUEST_STATUS::FRIEND_REQUEST_ALREADY_SENT;
    }
    
    // Success case - insert the request
    txn.exec_params(
        "INSERT INTO friend_request (id_player_inviting, id_player_invited) VALUES ($1, $2);",
        invitingPlayerId, invitedPlayerID
    );
    txn.commit();
    
    std::cout << "Friend request sent from " << invitingPlayerId << " to " << invitedPlayerID << std::endl;
    return FRIEND_REQUEST_STATUS::FRIEND_REQUEST_SENT;
}

void SocialMessage::getFriendData(const std::string& message, int clientSocket, 
    const std::string& queryTemplate, const std::string& countQueryTemplate) {
    FriendHeader header;
    Server& server = Server::getInstance();
    auto& userDatabase = server.getUserDatabase();
    pqxx::connection* db = server.getDB();
    pqxx::work txn(*db);

    FriendHeader friendHeader;
    memcpy(&friendHeader, message.c_str() + sizeof(SocialHeader),
           sizeof(FriendHeader));
    std::string userNameStr(friendHeader.username);
    std::string playerIDstr(std::to_string(friendHeader.idPlayer));

    pqxx::result result = txn.exec_params(queryTemplate, playerIDstr);
    pqxx::result countResult = txn.exec_params(countQueryTemplate, playerIDstr);

    int count = countResult[0][0].as<int>();    
    char countbuffer[sizeof(int)];
    memset(countbuffer, 0, sizeof(countbuffer));
    memcpy(countbuffer, &count, sizeof(int));
    server.sendMessage(clientSocket, countbuffer, sizeof(int));

 
    for (const auto& row : result) {
        char buffer[sizeof(FriendHeader)];
        header.idPlayer = row[0].as<int>();
        for (const auto& f : userDatabase) {
            if (f.second.playerID == header.idPlayer) {
                strcpy(header.username, f.second.username.c_str());
            }
        }

        memset(buffer, 0, sizeof(buffer));
        memcpy(buffer, &header, sizeof(FriendHeader));
        if(!server.sendMessage(clientSocket, buffer, sizeof(FriendHeader))) {
            std::cout << "Failed to send friend data to client " << clientSocket << std::endl;
        }
        else {
            std::cout << "Friend data sent to client " << clientSocket << std::endl;
        }  
}

txn.commit();
}
void SocialMessage::getFriendRequest(const std::string& message, int clientSocket) {
    std::cout << "Friend request list requested" << std::endl;
    
    const std::string query = 
        "SELECT id_player_inviting FROM friend_request WHERE id_player_invited = $1;";
    const std::string countQuery = 
        "SELECT COUNT(*) FROM friend_request WHERE id_player_invited = $1;";
    
    getFriendData(message, clientSocket, query, countQuery);
}

void SocialMessage::getFriendList(const std::string& message, int clientSocket) {
    std::cout << "Friend list requested" << std::endl;
    
    const std::string query = 
        "SELECT friend_id FROM friend_list WHERE player_id = $1;";
    const std::string countQuery = 
        "SELECT COUNT(*) FROM friend_list WHERE player_id = $1;";
    
    getFriendData(message, clientSocket, query, countQuery);
}



void SocialMessage::acceptFriendinvite(const std::string& message,
                                       int clientSocket) {
    Server& server = Server::getInstance();
    auto& clientsConnected = server.getClients();
    FriendHeader friendHeader;
    memcpy(&friendHeader, message.c_str() + sizeof(SocialHeader),
           sizeof(FriendHeader));
    std::string userNameStr(friendHeader.username);
    std::string playerIDstr(std::to_string(friendHeader.idPlayer));
 
    std::shared_ptr<Player> invitedPlayer = clientsConnected[clientSocket];
    std::cout << invitedPlayer->username << " accepted friend request from: " << userNameStr << std::endl;
    int invitedPlayerID = invitedPlayer->ID;
    pqxx::connection* db = server.getDB();
    pqxx::work txn(*db);
    txn.exec_params(
        "DELETE FROM friend_request WHERE (id_player_inviting = $1 AND "
        "id_player_invited = $2) OR (id_player_inviting = $2 AND "
        "id_player_invited = $1);",
        playerIDstr, invitedPlayerID);
    txn.exec_params(
        "INSERT INTO friend_list (player_id, friend_id) VALUES ($1, $2), ($2, $1);", playerIDstr, invitedPlayerID
    );
    txn.commit();
    std::cout << "Message saved to database!" << std::endl;
}

void SocialMessage::saveMessageToDatabase(int senderId, int receiverId, const std::string& message) {
    Server& server = Server::getInstance();   
    pqxx::connection* db = server.getDB();
    pqxx::work txn(*db);

    pqxx::result res = txn.exec_params(
        "INSERT INTO chat_messages (sender_id, receiver_id, message) VALUES ($1, $2, $3)",
        senderId, receiverId, message
    );

    txn.commit();
    std::cout << "Message saved to database!" << std::endl;
}

void SocialMessage::sendMessages(const std::string& message, int clientSocket) {
        Server& server = Server::getInstance();
        auto& clientsConnected = server.getClients();
        std::shared_ptr<Player>& sender = clientsConnected[clientSocket];
        FriendHeader friendHeader;
        memcpy(&friendHeader, message.c_str() + sizeof(SocialHeader), sizeof(FriendHeader));
        std::string userNameStr(friendHeader.username);
        std::cout << sender->username << " sent a message to " << userNameStr << " saying: " << friendHeader.message << std::endl;
        for (const auto& [sock, player] : clientsConnected) {
            std::cout << sender->receiver << " AND " << player->receiver << std::endl;
            if ((player->username == friendHeader.username) && 
            (player->receiver == sender->username) && (sender->receiver == player->username)) {
              
                if(player->isInChatRoom && sender->isInChatRoom) {
                    std::string message = sender->username + " says: " + friendHeader.message;
                    server.sendMessage(sock, message.c_str(), message.size());
                } 
            }
        }
        server.sendMessage(clientSocket, "Message sent", sizeof("Message sent"));
        saveMessageToDatabase(sender->ID, friendHeader.idPlayer, friendHeader.message);
    }

    std::vector<std::pair<int, std::string>> SocialMessage::loadMessageHistory(int player1Id, int player2Id) {
        std::vector<std::pair<int, std::string>> messages;
        Server& server = Server::getInstance();  
        pqxx::connection* db = server.getDB();
        pqxx::work txn(*db);
            
        pqxx::result res = txn.exec_params("SELECT * FROM chat_messages ORDER BY timestamp;");

        for (const auto& row : res) {
            int senderId = row[1].as<int>();
            int receiverId = row[2].as<int>();
            if((senderId == player1Id && receiverId == player2Id) || (senderId == player2Id && receiverId == player1Id)) {
                std::string message = row[3].as<std::string>();
                messages.push_back(std::make_pair(senderId, message));
            }
        }
        txn.commit();
        std::cout << "Message history loaded!" << std::endl;
        return messages;
    }
    
void SocialMessage::getMessages(const std::string& message, int clientSocket) {
    Server& server = Server::getInstance();
    auto& clientsConnected = server.getClients();
    std::shared_ptr<Player> requester = clientsConnected[clientSocket];
    ChatHeader chatHeader;
    FriendHeader friendHeader;
    memcpy(&friendHeader, message.c_str() + sizeof(SocialHeader), sizeof(FriendHeader));
    std::vector<std::pair<int, std::string>> messageHistory = loadMessageHistory(requester->ID, friendHeader.idPlayer);

    char countbuffer[sizeof(int)];
    memset(countbuffer, 0, sizeof(countbuffer));
    int count = messageHistory.size();
    memcpy(countbuffer, &count, sizeof(int));
    server.sendMessage(clientSocket, countbuffer, sizeof(int));

    for (int i = 0; i < messageHistory.size(); ++i) {
        char buffer[sizeof(ChatHeader)];
        memset(buffer, 0, sizeof(buffer));
        chatHeader.idPlayer = messageHistory[i].first;
        strcpy(chatHeader.username, friendHeader.username);
        strcpy(chatHeader.message, messageHistory[i].second.c_str());
        memcpy(buffer, &chatHeader, sizeof(ChatHeader));
        server.sendMessage(clientSocket, buffer, sizeof(ChatHeader));
    }
}

void SocialMessage::inChatRoom(const std::string& message, int clientSocket) {
    Server& server = Server::getInstance();
    auto& clientsConnected = server.getClients();
    std::shared_ptr<Player>& player = clientsConnected[clientSocket];
    FriendHeader friendHeader;
    memcpy(&friendHeader, message.c_str() + sizeof(SocialHeader), sizeof(FriendHeader));
    player->isInChatRoom = true;
    strcpy(player->receiver, friendHeader.receiver);
    std::cout << player->username << " is in a chat room with " << player->receiver << std::endl; 
}
void SocialMessage::leaveChat(const std::string& message, int clientSocket) {
    Server& server = Server::getInstance();
    auto& clientsConnected = server.getClients();
    std::shared_ptr<Player>& player = clientsConnected[clientSocket];
    player->isInChatRoom = false;
    FriendHeader friendHeader;
    memcpy(&friendHeader, message.c_str() + sizeof(SocialHeader), sizeof(FriendHeader));
    strcpy(player->receiver, friendHeader.receiver);
    std::cout << player->username << " left the chat room with " << friendHeader.username << std::endl; 
}
