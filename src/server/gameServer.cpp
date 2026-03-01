#include "gameServer.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "../common/header.h"
#include "message.h"
#include "server.h"

// GameRoom method definitions

void* GameRoom::loop(void* c) {
    GameRoom* room = static_cast<GameRoom*>(c);
    Server& server = Server::getInstance();
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;
    room->setRunning(true);
    while (room->isRunning()) {
        room->readySockets_ = room->allActiveSockets_;

        // Wait for activity on any socket
        if (select(room->maxFD_ + 1, &room->readySockets_, nullptr, nullptr,
                   &timeout) < 0) {
            std::cerr << "Error with select() in GameRoom" << std::endl;
            return nullptr;
        }
        // Check all client sockets for incoming messages
        for (auto it = room->players_.begin(); it != room->players_.end();
             ++it) {
            if (FD_ISSET(it->first, &room->readySockets_)) {
                // if (!room->isPlayerGamer(it->second->ID)) continue;
                room->handlePlayerMessages(it->second);
            }
        }
    }
    room->deleteAllInvitations(room->roomId_);
    std::cout << "Game room " << room->roomId_ << " is closed" << std::endl;
    server.removeGameRoom(room->roomId_);
    return nullptr;
}

void GameRoom::broadcast(int excludePlayer, char* buffer, ssize_t size) {
    Server& server = Server::getInstance();

    for (auto it : players_) {
        if (it.first != excludePlayer) {
            server.sendMessage(it.first, buffer, size);
        }
    }
}

void GameRoom::addPlayer(std::shared_ptr<Player> player, bool asGamer) {
    if (asGamer) {
        // si le nombre de joueurs atteint la limite, on refuse d'ajouter
        if (int(gamers_.size()) == getNbrPlayers()) {
            return;
        }
        gamers_.push_back(player->ID);
    }

    players_[player->socket] = player;
    players_[player->socket]->gameRoom = getShared();
    
    // ajout du socket à l'ensemble (pour select)
    FD_SET(player->socket, &allActiveSockets_);

    if (player->socket > maxFD_) {
        // mise à jour de maxFD (pour select)
        maxFD_ = player->socket;
    }

    // construction du message de mise à jour du lobby
    char buffer[BUFFER_SIZE];
    size_t size = 0;
    HeaderResponse response = {MESSAGE_TYPE::LOBBY,
                               sizeof(LobbyResponseHeader)};
    size += sizeof(HeaderResponse);

    LobbyResponseHeader lobbyResponse = {LOBBY_RESPONSE::UPDATE_PLAYER,
                                         roomId_};
    memcpy(buffer + size, &lobbyResponse, sizeof(LobbyResponseHeader));
    size += sizeof(LobbyResponseHeader);

    LobbyUpdatePlayer lobbyUpdatePlayer;
    lobbyUpdatePlayer.nbPlayers = 1;
    lobbyUpdatePlayer.completed = true;
    memcpy(buffer + size, &lobbyUpdatePlayer, sizeof(LobbyUpdatePlayer));
    size += sizeof(LobbyUpdatePlayer);

    LobbyUpdatePlayerList lUPL;
    lUPL.added = true;
    lUPL.idPlayer = player->ID;
    strcpy(lUPL.name, player->username.c_str());
    lUPL.asGamer = asGamer;
    memcpy(buffer + size, &lUPL, sizeof(LobbyUpdatePlayerList));
    size += sizeof(LobbyUpdatePlayerList);

    response.sizeMessage = int(size - sizeof(HeaderResponse));
    memcpy(buffer, &response, sizeof(HeaderResponse));
    broadcast(player->socket, buffer, size);
}

void GameRoom::sendParticipantList(std::shared_ptr<Player> player) {
    Server& server = Server::getInstance();

    char buffer[BUFFER_SIZE];
    size_t size = 0;

    HeaderResponse response = {MESSAGE_TYPE::LOBBY,
                               sizeof(LobbyResponseHeader)};
    LobbyResponseHeader lobbyResponse = {LOBBY_RESPONSE::UPDATE_PLAYER,
                                         roomId_};
    size  += sizeof(HeaderResponse) + sizeof(LobbyResponseHeader) +
            sizeof(LobbyUpdatePlayer);

    LobbyUpdatePlayer lobbyUpdatePlayer;
    LobbyUpdatePlayerList lUPL;

    for (auto it : players_) {
        if (it.first == player->socket) {
            continue;
        }

        lobbyUpdatePlayer.nbPlayers++;
        lUPL.added = true;
        lUPL.idPlayer = it.second->ID;
        strcpy(lUPL.name, it.second->username.c_str());
        lUPL.asGamer = isPlayerGamer(it.second->ID);

        // si le buffer est trop plein pour contenir un joueur en plus, 
        // on envoie une partie
        if (size + sizeof(LobbyUpdatePlayerList) > BUFFER_SIZE) {
            response.sizeMessage = int(size - sizeof(HeaderResponse));
            memcpy(buffer, &response, sizeof(HeaderResponse));
            lobbyUpdatePlayer.completed = false;
            memcpy(buffer + sizeof(HeaderResponse), &lobbyResponse,
                   sizeof(LobbyResponseHeader));
            server.sendMessage(player->socket, buffer, size);
            lobbyUpdatePlayer.nbPlayers = 0;
            size = sizeof(HeaderResponse) + sizeof(LobbyResponseHeader) +
                   sizeof(LobbyUpdatePlayer);
        }

        memcpy(buffer + size, &lUPL, sizeof(LobbyUpdatePlayerList));
        size += sizeof(LobbyUpdatePlayerList);
    }

    response.sizeMessage = int(size - sizeof(HeaderResponse));
    memcpy(buffer, &response, sizeof(HeaderResponse));
    lobbyUpdatePlayer.completed = true;
    memcpy(buffer + sizeof(HeaderResponse), &lobbyResponse,
           sizeof(LobbyResponseHeader));
    server.sendMessage(player->socket, buffer, size);
}

void GameRoom::handleStartRequest(std::shared_ptr<Player> player) {
    Server& server = Server::getInstance();

    char buffer[BUFFER_SIZE];
    LobbyMessage lobbyMessage;

    if (player->ID != groupeLeader_) {
        size_t size = lobbyMessage.serializeErrorLobby(
            "Only the group leader can start the game", buffer);
        server.sendMessage(player->socket, buffer, size);
        return;
    }

    if (int(gamers_.size()) < getNbrPlayers()) {
        size_t size = lobbyMessage.serializeErrorLobby(
            "Not enough players to start the game", buffer);
        server.sendMessage(player->socket, buffer, size);
        return;

    }

    setPlaying(true);

    size_t size = 0;
    HeaderResponse response = {MESSAGE_TYPE::LOBBY,
                               sizeof(LobbyResponseHeader)};
    size += sizeof(HeaderResponse);

    LobbyResponseHeader lobbyResponse = {LOBBY_RESPONSE::STARTED, roomId_};
    memcpy(buffer + size, &lobbyResponse, sizeof(LobbyResponseHeader));
    size += sizeof(LobbyResponseHeader);

    response.sizeMessage = int(size - sizeof(HeaderResponse));
    memcpy(buffer, &response, sizeof(HeaderResponse));
    broadcast(-1, buffer, size);

    for (auto it : players_) {
        it.second->isPlaying = true;
    }
}

void GameRoom::playerLeaves(std::shared_ptr<Player> player) {
    Server& server = Server::getInstance();

    bool asGamer = isPlayerGamer(player->ID);
    removePlayer(player);

    // vérifie si la socket est toujours connectée
    if (recv(player->socket, nullptr, 1, MSG_PEEK | MSG_DONTWAIT) == 0) {
        server.disconnectClient(player->socket);
    } else {
        server.addActivePlayer(player);
    }

    // informer les autres que ce joueur est parti
    sendParticipantLeaving(player, asGamer);

    if ((player->ID == groupeLeader_ or gamers_.empty()) and !isPlaying()) {
        sendEndLobby();

        for (auto it : players_) {
            removePlayer(it.second);
            server.addActivePlayer(it.second);
        }
        setRunning(false);
    }
}

void GameRoom::removePlayer(std::shared_ptr<Player> player) {
    for (auto it = gamers_.begin(); it != gamers_.end(); ++it) {
        if (*it == player->ID) {
            gamers_.erase(it);
            break;
        }
    }

    players_.erase(player->socket);
    FD_CLR(player->socket, &allActiveSockets_);
}

void GameRoom::sendEndLobby() {
    size_t size = 0;
    char buffer[BUFFER_SIZE];

    HeaderResponse response = {MESSAGE_TYPE::LOBBY,
                               sizeof(LobbyResponseHeader)};
    memcpy(buffer, &response, sizeof(HeaderResponse));
    size += sizeof(HeaderResponse);

    LobbyResponseHeader lobbyResponse = {LOBBY_RESPONSE::END, roomId_};
    memcpy(buffer + size, &lobbyResponse, sizeof(LobbyResponseHeader));
    size += sizeof(LobbyResponseHeader);

    broadcast(-1, buffer, size);
}

void GameRoom::deleteAllInvitations(int roomId) {
    Server& server = Server::getInstance();

    pqxx::connection* db = server.getDB();
    pqxx::work transaction(*db);
    std::string query = "DELETE FROM game_invitation WHERE room_id = " +
                        std::to_string(roomId) + ";";

    try {
        transaction.exec(query);

    } catch (const std::exception& e) {
        std::cerr << "Error deleting invitations: " << e.what() << std::endl;
    }

    transaction.commit();
}

void GameRoom::sendLobbyUpdate() {
    char buffer[BUFFER_SIZE];
    size_t size = 0;

    HeaderResponse response = {MESSAGE_TYPE::LOBBY, sizeof(HeaderResponse)};
    memcpy(buffer, &response, sizeof(HeaderResponse));
    size += sizeof(HeaderResponse);

    LobbyResponseHeader lobbyResponse = {LOBBY_RESPONSE::UPDATE, roomId_};
    memcpy(buffer + size, &lobbyResponse, sizeof(LobbyResponseHeader));
    size += sizeof(LobbyResponseHeader);

    LobbyUpdate lobbyUpdate;
    lobbyUpdate.nbGamerMax = getNbrGamerMax();
    strcpy(lobbyUpdate.gameMode, gameMode_.c_str());
    memcpy(buffer + size, &lobbyUpdate, sizeof(LobbyUpdate));
    size += sizeof(LobbyUpdate);

    response.sizeMessage = int(size - sizeof(HeaderResponse));
    memcpy(buffer, &response, sizeof(HeaderResponse));

    broadcast(-1, buffer, size);
}

void GameRoom::sendParticipantLeaving(std::shared_ptr<Player> player,
                                      bool asGamer) {
    size_t size = sizeof(HeaderResponse) + sizeof(LobbyResponseHeader) +
                  sizeof(LobbyUpdatePlayer) + sizeof(LobbyUpdatePlayerList);

    char buffer[BUFFER_SIZE];
    HeaderResponse response = {MESSAGE_TYPE::LOBBY, sizeof(HeaderResponse)};
    LobbyResponseHeader lobbyResponse = {LOBBY_RESPONSE::UPDATE_PLAYER,
                                         roomId_};

    LobbyUpdatePlayer lobbyUpdatePlayer;
    lobbyUpdatePlayer.nbPlayers = 1;
    lobbyUpdatePlayer.completed = true;

    LobbyUpdatePlayerList lUPL;
    lUPL.added = false;
    lUPL.idPlayer = player->ID;
    strcpy(lUPL.name, player->username.c_str());
    lUPL.asGamer = asGamer;

    response.sizeMessage = int(size - sizeof(HeaderResponse));
    memcpy(buffer, &response, sizeof(HeaderResponse));
    memcpy(buffer + sizeof(HeaderResponse), &lobbyResponse,
           sizeof(LobbyResponseHeader));
    memcpy(buffer + sizeof(HeaderResponse) + sizeof(LobbyResponseHeader),
           &lobbyUpdatePlayer, sizeof(LobbyUpdatePlayer));
    memcpy(buffer + sizeof(HeaderResponse) + sizeof(LobbyResponseHeader) +
               sizeof(LobbyUpdatePlayer),
           &lUPL, sizeof(LobbyUpdatePlayerList));

    broadcast(player->socket, buffer, size);
}

bool GameRoom::isPlayerGamer(int playerID) {
    for (int gamer : gamers_) {
        if (gamer == playerID) {
            return true;
        }
    }
    return false;
}

void GameRoom::handlePlayerMessages(std::shared_ptr<Player> player) {
    Server& server = Server::getInstance();
    char buffer[BUFFER_SIZE];

    if (server.readSafe(player->socket, buffer, sizeof(Header)) != 0) {
        playerLeaves(player);
        return;
    }

    Header header;
    memcpy(&header, buffer, sizeof(Header));

    if (server.readSafe(player->socket, buffer + sizeof(Header),
                        header.sizeMessage) != 0) {
        playerLeaves(player);
        return;
    }

    std::string message(buffer, sizeof(Header) + header.sizeMessage);
    processMessage(player, message);
}

void GameRoom::processMessage(std::shared_ptr<Player> player,
                              const std::string& message) {
    Header header;
    memcpy(&header, message.c_str(), sizeof(Header));

    if (header.type != MESSAGE_TYPE::LOBBY and
        header.type != MESSAGE_TYPE::GAME) {
        std::cout << "Invalid message type for GameRoom" << std::endl;
        return;
    }

    std::shared_ptr<IMessage> msgHandler = IMessage::buildMessage(header);
    msgHandler->handleMessage(message.substr(sizeof(Header)), player->socket);
}

void GameRoom::setNbrPlayers(int nbrPlayers) {
    if (nbrPlayers > MAX_PLAYERS_NUMBERS) {
        nbrPlayers_ = MAX_PLAYERS_NUMBERS;
    } else {
        nbrPlayers_ = nbrPlayers;
    }
}

int GameRoom::countAlivePlayers() { return static_cast<int>(gamers_.size()); }


// Implémentation GameServer //

Tetramino GameServer::generateRandomTetramino() {
    auto& gs = getPlayerGameState();
    
    std::vector<std::vector<int>> shape;
    if (isMiniTetraActive(gs)) {
        // on génere un bloc 1x1
        shape = {{1}};
        reduceMiniTetraCount(gs);
    } else {
        int shapeIndex = int(rand() % SHAPES.size());
        shape = SHAPES[shapeIndex];
    }

    // Si toutes les couleurs ont été utilisées, on réinitialise
    if (gs->previousColors_.size() == NUMBER_OF_COLORS) {
        gs->previousColors_.clear();
    }

    // Pour choisir une couleur différente des précédentes
    std::uint8_t colorIndex;
    do {
        colorIndex = std::uint8_t(rand() % NUMBER_OF_COLORS + 1);
    } while (std::find(gs->previousColors_.begin(), gs->previousColors_.end(),
                       colorIndex) != gs->previousColors_.end());

    gs->previousColors_.push_back(colorIndex);

    return Tetramino(0, int(WIDTH / 2 - shape[0].size() / 2), shape,
                     colorIndex);
}

bool GameServer::handleTetramino(SpawnTetraminoPayload& payload) {
    auto& gs = getPlayerGameState();
    gs->init();
    gs->currentTetramino = generateRandomTetramino();

    // vérifie si le tetramino peut etre placé dans la grille 
    if (!canMoveOrPlace(0, 0)) {
        return false;
    }

    payload.x = static_cast<std::uint8_t>(gs->currentTetramino.x_);
    payload.y = static_cast<std::uint8_t>(gs->currentTetramino.y_);
    payload.color = gs->currentTetramino.colorIndex_;
    payload.height = static_cast<uint8_t>(gs->currentTetramino.shape_.size());
    payload.width = static_cast<uint8_t>(gs->currentTetramino.shape_[0].size());

    for (size_t i = 0; i < gs->currentTetramino.shape_.size(); ++i) {
        for (size_t j = 0; j < gs->currentTetramino.shape_[i].size(); ++j) {
            payload.shape[i][j] = uint8_t(gs->currentTetramino.shape_[i][j]);
        }
    }

    return true;
}

void GameServer::handleMove(GameUpdateHeader& update, MovementPayload& payload) {
    auto& player = getPlayer();
    auto& gs = getPlayerGameState();
    auto& t = gs->currentTetramino;
    auto gameMode = player->gameRoom->getGameMode();

    bool locked = false;

    // si le tetramino ne peut pas descendre
    if (payload.dx == 1 && !canMoveOrPlace(payload.dx, payload.dy)) {
        locked = true;
    }
    else if (canMoveOrPlace(payload.dx, payload.dy)) {
        t.x_ += payload.dx;
        t.y_ += payload.dy;
    }

    // si le tetramino est posé
    if (locked) {
        for (size_t i = 0; i < t.shape_.size(); ++i) {
            for (size_t j = 0; j < t.shape_[i].size(); ++j) {
                if (t.shape_[i][j] == 1) {
                    int x = static_cast<int>(t.x_ + i);
                    int y = static_cast<int>(t.y_ + j);

                    if (x >= 0 && x < HEIGHT && y >= 0 && y < WIDTH) {
                        gs->grid_[x][y] = t.colorIndex_;
                    }
                }
            }
        }

        update.type = GAME_TYPE::LOCK;
        clearFullRow(update);

        if (gameMode == "Classic" || gameMode == "Dual") {
            checkIfMalus(update);
        }
    }
    else {
        update.type = GAME_TYPE::MOVE;
    }

    update.x = uint8_t(t.x_);
    update.y = uint8_t(t.y_);
    sendGridToOpponent();
}

bool GameServer::handleRotate(GameUpdateHeader& update, RotationPayload& payload) {
    auto& gs = getPlayerGameState();
    auto& t = gs->currentTetramino;

    std::vector<std::vector<int>> rotatedShape = rotate(t.shape_, payload.clockwise);

    if (!checkRotation(rotatedShape, t)) {
        return false;
    }

    t.shape_ = rotatedShape;

    update.type = GAME_TYPE::ROTATE;
    update.rotation = payload.clockwise;
    update.x = static_cast<uint8_t>(t.x_);
    update.y = static_cast<uint8_t>(t.y_);
    return true;
}


void GameServer::handleMalus(MalusPayload& payload) {
    int malusType = payload.malusType;

    switch (malusType) {
        case 1:
            break;

        default:
            break;
    }
}

void GameServer::handleBonus(BonusPayload& payload) {
    auto& gs = getPlayerGameState();
    int bonusType = payload.type;

    switch (bonusType) {
        case 1:
            addPoints(gs, payload.details);
            break;
        case 3:
            setMiniTetra(gs);

        default:
            break;
    }
}

std::vector<std::vector<int>> GameServer::rotate(
    const std::vector<std::vector<int>>& shape, bool clockwise) {
    int rows = int(shape.size());
    int cols = int(shape[0].size());
    std::vector<std::vector<int>> rotated(cols, std::vector<int>(rows, 0));

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (clockwise) {
                rotated[j][rows - 1 - i] = shape[i][j];
            } else {
                rotated[cols - 1 - j][i] = shape[i][j];
            }
        }
    }
    return rotated;
}

bool GameServer::checkRotation(std::vector<std::vector<int>>& rotatedShape,
                               Tetramino& t) {
    auto& gs = getPlayerGameState();

    for (int i = 0; i < int(rotatedShape.size()); ++i) {
        for (int j = 0; j < int(rotatedShape[0].size()); ++j) {
            if (rotatedShape[i][j] == 1) {
                int x = t.x_ + i;
                int y = t.y_ + j;
                if (x < 0 || x >= HEIGHT || y < 0 || y >= WIDTH ||
                    gs->grid_[x][y] != 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

void GameServer::clearFullRow(GameUpdateHeader& update) {
    auto& gs = getPlayerGameState();
    auto& player = getPlayer();
    int linesCleared = 0;
    auto& grid = gs->grid_;

    std::vector<int> clearedRows;  // on stocke les lignes effacées

    for (int i = HEIGHT - 1; i >= 0; --i) {
        bool full = true;
        for (int j = 0; j < WIDTH; ++j) {
            // si la ligne n'est pas complète
            if (grid[i][j] == 0) {
                full = false;
                break;
            }
        }

        if (full) {
            linesCleared++;
            clearedRows.push_back(i);
            grid.erase(grid.begin() + i);
            grid.emplace(grid.begin(), WIDTH, 0);
            i++;
            player->energy++;
        }
    }

    gs->points_ += linesCleared * 100;
    update.score = gs->points_;
    update.linesCleared = uint8_t(linesCleared);

    // on remplit les lignes supprimées
    memset(update.rows, -1, sizeof(update.rows));
    for (size_t i = 0; i < clearedRows.size() && i < 4; ++i) {
        update.rows[i] = uint8_t(clearedRows[i]);
    }
}

bool GameServer::canMoveOrPlace(int dx, int dy) {
    auto& gs = getPlayerGameState();
    const auto& t = gs->currentTetramino;
    const auto& shape = t.shape_;
    const auto& grid = gs->grid_;

    for (int i = 0; i < int(shape.size()); ++i) {
        for (int j = 0; j < int(shape[0].size()); ++j) {
            if (shape[i][j] == 1) {
                int newX = t.x_ + i + dx;
                int newY = t.y_ + j + dy;

                if (newX < 0 || newX >= HEIGHT || newY < 0 || newY >= WIDTH)
                    return false;

                if (grid[newX][newY] != 0) return false;
            }
        }
    }
    return true;
}

void GameServer::sendGridToOpponent() {
    auto& gs = getPlayerGameState();
    auto& player = getPlayer();

    // copie de la grille du joueur
    std::vector<std::vector<int>> tmpGrid = gs->grid_;

    // insère le tétraminos actif dans la grille copiée
    auto& t = gs->currentTetramino;
    for (size_t i = 0; i < t.shape_.size(); ++i) {
        for (size_t j = 0; j < t.shape_[i].size(); ++j) {
            if (t.shape_[i][j] == 1) {
                int x = static_cast<int>(t.x_ + i);
                int y = static_cast<int>(t.y_ + j);

                if (x >= 0 && x < HEIGHT && y >= 0 && y < WIDTH) {
                    tmpGrid[x][y] = t.colorIndex_;
                }
            }
        }
    }

    GridUpdate gridUpdate;
    strncpy(gridUpdate.username, player->username.c_str(), 
            sizeof(gridUpdate.username));
    gridUpdate.username[sizeof(gridUpdate.username) - 1] = '\0';
    gridUpdate.playerID = player->ID;

    for (int i = 0; i < HEIGHT; ++i) {
        for (int j = 0; j < WIDTH; ++j) {
            gridUpdate.grid[i][j] = static_cast<uint8_t>(tmpGrid[i][j]);
        }
    }

    // envoyer cette grille aux autres joueurs de la gameroom
    HeaderResponse header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(GridUpdate);

    GameTypeHeader gameHeader;
    gameHeader.type = GAME_TYPE::UPDATEGRID;

    char buffer[sizeof(HeaderResponse) + sizeof(GameTypeHeader) + sizeof(GridUpdate)];
    memcpy(buffer, &header, sizeof(HeaderResponse));
    memcpy(buffer + sizeof(HeaderResponse), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader), &gridUpdate, sizeof(GridUpdate));

    player->gameRoom->broadcast(player->socket, buffer, sizeof(buffer));
}

void GameServer::sendRowMalus(int targetSocket, int lines, int emptyIdx) {
    Server& server = Server::getInstance();
    HeaderResponse header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(MalusPayload);

    GameTypeHeader gameHeader;
    gameHeader.type = GAME_TYPE::MALUS;

    MalusPayload payload{};
    payload.malusType = 1;
    payload.target = targetSocket;
    payload.details = lines;
    payload.emptyIdx = emptyIdx;

    char buffer[sizeof(HeaderResponse) + sizeof(GameTypeHeader) +
                sizeof(MalusPayload)];
    memcpy(buffer, &header, sizeof(HeaderResponse));
    memcpy(buffer + sizeof(HeaderResponse), &gameHeader,
           sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader), &payload,
           sizeof(MalusPayload));

    server.sendMessage(targetSocket, buffer, sizeof(buffer));
}

void GameServer::checkIfMalus(GameUpdateHeader& update) {
    auto& player = getPlayer();
    if (update.linesCleared >= 2) {
        int malusCount = 0;
        if (update.linesCleared == 2)
            malusCount = 1;
        else if (update.linesCleared == 3)
            malusCount = 2;
        else if (update.linesCleared == 4)
            malusCount = 4;

        auto gameMode = player->gameRoom->getGameMode();
        int targetSocket = -1;

        if (gameMode == "Classic") {
            targetSocket = randomTarget();

        } else if (gameMode == "Dual") {
            targetSocket = getOpponentSocket();
        }

        if (targetSocket != -1) {
            auto& targetGrid = getGridFromSocket(targetSocket);
            int emptyIdx = addMalusRow(targetGrid, malusCount);
            sendRowMalus(targetSocket, malusCount, emptyIdx);
        }
    }
}

int GameServer::randomTarget() {
    auto& sender = getPlayer();
    const auto& players = sender->gameRoom->getPlayers();
    std::vector<int> sockets;

    for (const auto& [sock, player] : players) {
        if (sock != sender->socket &&
            player->gameRoom->isPlayerGamer(player->ID)) {
            sockets.push_back(sock);
        }
    }

    if (sockets.empty()) return -1;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0,
                static_cast<int>(sockets.size()) - 1);

    return sockets[dis(gen)];
}

int GameServer::getOpponentSocket() {
    auto& sender = getPlayer();
    const auto& players = sender->gameRoom->getPlayers();
    for (const auto& [sock, player] : players) {
        if (sock != sender->socket &&
            player->gameRoom->isPlayerGamer(player->ID)) {
            return sock;
        }
    }
    return -1;
}

int GameServer::IdToSocket(int playerID) {
    const auto& players = getPlayer()->gameRoom->getPlayers();
    for (const auto& [sock, player] : players) {
        if (player->ID == playerID) {
            return sock;
        }
    }
    return -1;
}

std::vector<std::vector<int>>& GameServer::getGridFromSocket(int socket) {
    auto players = currentPlayer->gameRoom->getPlayers();
    for (const auto& [id, player] : players) {
        if (player && player->socket == socket) {
            if (player->gameState) {
                return player->gameState->grid_;
            }
        }
    }
    throw std::runtime_error("Aucun joueur trouvé avec ce socket");
}

int GameServer::addMalusRow(std::vector<std::vector<int>>& grid, int malusCount) {
    // choisir l'endroit du trou
    int emptyIdx = rand() % WIDTH;

    for (int l = 0; l < malusCount; ++l) {
        // supprimer la ligne du haut
        grid.erase(grid.begin());

        // créer une ligne de malus gris avec un seul trou
        std::vector<int> malusRow(WIDTH, 9);
        malusRow[emptyIdx] = 0;

        grid.push_back(malusRow);
    }
    return emptyIdx;
}
