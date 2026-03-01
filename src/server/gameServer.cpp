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
                if (!room->isPlayerGamer(it->second->ID)) continue; 
                room->handlePlayerMessages(it->second);
            }
        }
    }
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
        if (int(gamers_.size()) < getNbrPlayers()) {
            gamers_.push_back(player->ID);
        }
    }

    players_[player->socket] = player;
    players_[player->socket]->gameRoom = getShared();
    FD_SET(player->socket, &allActiveSockets_);
    if (player->socket > maxFD_) {
        maxFD_ = player->socket;
    }
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
    size += sizeof(HeaderResponse) + sizeof(LobbyResponseHeader) +
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
    char buffer[BUFFER_SIZE];
    LobbyMessage lobbyMessage;
    if (player->ID != groupeLeader_) {
        size_t size = lobbyMessage.serializeErrorLobby(
            "Only the group leader can start the game", buffer);
        Server::getInstance().sendMessage(player->socket, buffer, size);
        return;
    }
    if (int(gamers_.size()) < getNbrPlayers()) {
        size_t size = lobbyMessage.serializeErrorLobby(
            "Not enough players to start the game", buffer);
        Server::getInstance().sendMessage(player->socket, buffer, size);
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
}
void GameRoom::playerLeaves(std::shared_ptr<Player> player) {
    Server& server = Server::getInstance();
    bool asGamer = isPlayerGamer(player->ID);
    removePlayer(player);
    if (recv(player->socket, nullptr, 1, MSG_PEEK | MSG_DONTWAIT) == 0) {
        server.disconnectClient(player->socket);
    } else {
        server.addActivePlayer(player);
    }
    sendParticipantLeaving(player, asGamer);
    if (player->ID == groupeLeader_ or players_.empty()) {
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

void GameRoom::sendParticipantLeaving(std::shared_ptr<Player> player, bool asGamer) {
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
    char buffer[BUFFER_SIZE];

    if (Server::readSafe(player->socket, buffer, sizeof(Header)) != 0) {
        playerLeaves(player);
        return;
    }
    Header header;
    memcpy(&header, buffer, sizeof(Header));
    if (Server::readSafe(player->socket, buffer + sizeof(Header),
                         header.sizeMessage) != 0) {
        playerLeaves(player);
        return;
    }
    std::string message(buffer, sizeof(Header) + header.sizeMessage);
    processMessage(player, message);
}

void GameRoom::processMessage(std::shared_ptr<Player> player, const std::string& message) {
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
// Setter and getter methods
void GameRoom::setNbrPlayers(int nbrPlayers) {
    if (nbrPlayers > MAX_PLAYERS_NUMBERS) {
        nbrPlayers_ = MAX_PLAYERS_NUMBERS;
    } else {
        nbrPlayers_ = nbrPlayers;
    }
}

int GameRoom::countAlivePlayers() {
    int count = 0;
    for (const auto& player : players_) {
        count++;
    }
    return count;
}

// Implémentation GameServer //

bool GameServer::handleSpawnTetramino(std::shared_ptr<Player>& player,
                                      SpawnTetraminoPayload& payload) {
    GameState& gs = *player->gameState;
    gs.init();
    gs.current_.x_ = payload.x;
    gs.current_.y_ = payload.y;
    gs.currentShape_.clear();

    for (int i = 0; i < payload.height; ++i) {
        std::vector<int> row;
        for (int j = 0; j < payload.width; ++j) {
            row.push_back(payload.shape[i][j]);
        }
        gs.currentShape_.push_back(row);
    }

    // si le tetramino ne peut pas etre placé
    if (!canMoveOrPlace(gs, 0, 0)) {
        return false;
    }

    // sinon on pose le tetramino
    for (int i = 0; i < payload.height; ++i) {
        for (int j = 0; j < payload.width; ++j) {
            if (payload.shape[i][j] == 1) {
                int gridX = payload.x + i;
                int gridY = payload.y + j;
                if (gridX >= 0 && gridX < HEIGHT && gridY >= 0 &&
                    gridY < WIDTH) {
                    gs.grid_[gridX][gridY] = 1;
                }
            }
        }
    }
    return true;
}

void GameServer::handleMove(std::shared_ptr<Player>& player,
                            GameUpdateHeader& update,
                            MovementPayload& payload) {
    GameState& gs = *player->gameState;
    auto& shape = gs.currentShape_;
    auto& grid = gs.grid_;
    auto& t = gs.current_;

    // on fait un clear temporaire de la grille
    clearOrReplaceForMove(shape, grid, t, false);

    bool locked = false;

    // si le tetramino ne peut plus tomber
    if (payload.dx == 1 && !canMoveOrPlace(gs, payload.dx, payload.dy)) {
        locked = true;

    } else if (canMoveOrPlace(gs, payload.dx, payload.dy)) {
        t.x_ += payload.dx;
        t.y_ += payload.dy;
    }

    // on replace dans la grille
    clearOrReplaceForMove(shape, grid, t, true);

    if (locked) {
        update.type = GAME_TYPE::LOCK;
        clearFullRow(gs, update);
        checkIfMalus(update, player);

    } else {
        update.type = GAME_TYPE::MOVE;
    }
    update.x = uint8_t(t.x_);
    update.y = uint8_t(t.y_);
    sendGridToOpponent(player, gs);
}

void GameServer::handleRotate(std::shared_ptr<Player>& player,
                              GameUpdateHeader& update,
                              RotationPayload& payload) {
    GameState& gs = *player->gameState;
    auto& t = gs.current_;
    auto& shape = gs.currentShape_;

    clearOrReplaceForRotate(gs, shape, t, false);

    // appliquer la rotation
    std::vector<std::vector<int>> rotatedShape;
    rotatedShape = rotate(shape, payload.clockwise);

    // vérifier la validité de la rotation
    bool valid = checkRotation(rotatedShape, gs, t);

    if (valid) {
        gs.currentShape_ = rotatedShape;
    }

    clearOrReplaceForRotate(gs, shape, t, true);

    update.type = GAME_TYPE::ROTATE;
    update.rotation = payload.clockwise;
}

void GameServer::handleConfirmMalus(std::shared_ptr<Player>& player, 
                                    MalusPayload& payload) {
    GameState& gs = *player->gameState;
    clearOrReplaceForMove(gs.currentShape_, gs.grid_, gs.current_, false);

    if (payload.malusType == 1) {
        for (int i = 0; i < payload.details; ++i) {
            gs.grid_.erase(gs.grid_.begin());
            gs.grid_.push_back(std::vector<int>(WIDTH, 9));
        }
    }
    
    clearOrReplaceForMove(gs.currentShape_, gs.grid_, gs.current_, true);
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
                rotated[j][cols - 1 - i] = shape[i][j];
            }
        }
    }
    return rotated;
}

bool GameServer::checkRotation(std::vector<std::vector<int>>& rotatedShape,
                               GameState& gs, Position& t) {
    for (int i = 0; i < int(rotatedShape.size()); ++i) {
        for (int j = 0; j < int(rotatedShape[0].size()); ++j) {
            if (rotatedShape[i][j] == 1) {
                int x = t.x_ + i;
                int y = t.y_ + j;
                if (x < 0 || x >= HEIGHT || y < 0 || y >= WIDTH ||
                    gs.grid_[x][y] != 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

void GameServer::clearOrReplaceForRotate(GameState& gs,
                                         std::vector<std::vector<int>>& shape,
                                         const Position& t, bool info) {
    for (int i = 0; i < int(shape.size()); ++i) {
        for (int j = 0; j < int(shape[0].size()); ++j) {
            if (shape[i][j] == 1) {
                int x = t.x_ + i;
                int y = t.y_ + j;
                if (x >= 0 && x < HEIGHT && y >= 0 && y < WIDTH)
                    gs.grid_[x][y] = info ? 1 : 0;
            }
        }
    }
}

void GameServer::clearOrReplaceForMove(std::vector<std::vector<int>>& shape,
                                       std::vector<std::vector<int>>& grid,
                                       const Position& t, bool info) {
    for (int i = 0; i < int(shape.size()); ++i) {
        for (int j = 0; j < int(shape[0].size()); ++j) {
            if (shape[i][j] == 1) {
                int gridX = t.x_ + i;
                int gridY = t.y_ + j;
                if (gridX >= 0 && gridX < HEIGHT && gridY >= 0 &&
                    gridY < WIDTH) {
                    grid[gridX][gridY] = info ? 1 : 0;
                }
            }
        }
    }
}

void GameServer::clearFullRow(GameState& gs, GameUpdateHeader& update) {
    int linesCleared = 0;
    auto& grid = gs.grid_;

    std::vector<int> clearedRows;  // on stocke les lignes effacées

    for (int i = HEIGHT - 1; i >= 0; --i) {
        bool full = true;
        for (int j = 0; j < WIDTH; ++j) {
            if (grid[i][j] == 0 || grid[i][j] == 9) {
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
        }
    }

    gs.points_ += linesCleared * 100;
    update.score = gs.points_;
    update.linesCleared = uint8_t(linesCleared);

    // on remplit les lignes supprimées
    memset(update.rows, -1, sizeof(update.rows));
    for (size_t i = 0; i < clearedRows.size() && i < 4; ++i) {
        update.rows[i] = uint8_t(clearedRows[i]);
    }
}

bool GameServer::canMoveOrPlace(const GameState& gameState, int dx, int dy) {
    const auto& t = gameState.current_;
    const auto& shape = gameState.currentShape_;
    const auto& grid = gameState.grid_;

    if (shape.empty() || shape[0].empty()) {
        std::cerr << "[SERVER] ERREUR : currentShape_ vide\n";
        return false;
    }

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

void GameServer::sendGridToOpponent(std::shared_ptr<Player>& player, GameState& gs) {
    GridUpdate gridUpdate;
    gridUpdate.playerID = player->ID;

    for (int i = 0; i < HEIGHT; ++i) {
        for (int j = 0; j < WIDTH; ++j) {
            gridUpdate.grid[i][j] = static_cast<uint8_t>(gs.grid_[i][j]);
        }
    }

    HeaderResponse header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(GridUpdate);

    GameTypeHeader gameHeader;
    gameHeader.type = GAME_TYPE::UPDATEGRID;

    char buffer[sizeof(HeaderResponse) + sizeof(GameTypeHeader) + sizeof(GridUpdate)];
    memcpy(buffer, &header, sizeof(HeaderResponse));
    memcpy(buffer + sizeof(HeaderResponse), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader), &gridUpdate, sizeof(GridUpdate));

    // envoie à tous les autres joueur du lobby
    player->gameRoom->broadcast(player->socket, buffer, sizeof(buffer));
}


void GameServer::sendMalus(std::string gameMode ,int targetSocket, int lines=0) {
    Server& server = Server::getInstance();
    HeaderResponse header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(MalusPayload);

    GameTypeHeader gameHeader;
    gameHeader.type = GAME_TYPE::MALUS;

    MalusPayload payload{};
    if (gameMode != "Royal") {
        payload.malusType = 1;
        payload.targetSocket = targetSocket;
        payload.details = lines;
    }

    char buffer[sizeof(HeaderResponse) + sizeof(GameTypeHeader) + sizeof(MalusPayload)];
    memcpy(buffer, &header, sizeof(HeaderResponse));
    memcpy(buffer + sizeof(HeaderResponse), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader), &payload, sizeof(MalusPayload));

    server.sendMessage(targetSocket, buffer, sizeof(buffer));
}

void GameServer::checkIfMalus(GameUpdateHeader &update, 
                              std::shared_ptr<Player> &player) {
    if (update.linesCleared >= 2) {
        int malusCount = 0;
        if (update.linesCleared == 2) malusCount = 1;
        else if (update.linesCleared == 3) malusCount = 2;
        else if (update.linesCleared == 4) malusCount = 4;
    
        auto gameMode = player->gameRoom->getGameMode();
    
        if (gameMode == "Classic") {
            int targetSocket = randomTarget(player);
            if (targetSocket != -1) {
                sendMalus(gameMode, targetSocket, malusCount);
            }

        } else if (gameMode == "Dual") {
            if (player->gameRoom->getPlayers().size() == 2) {
                int targetSocket = getOpponentSocket(player);
                sendMalus(gameMode, targetSocket, malusCount);
            }
            
        }
    }
}

int GameServer::randomTarget(std::shared_ptr<Player>& sender) {
    const auto& players = sender->gameRoom->getPlayers();
    std::vector<int> sockets;

    for (const auto& [sock, player] : players) {
        if (sock != sender->socket && player->gameRoom->isPlayerGamer(player->ID)) {
            sockets.push_back(sock);
        }
    }

    if (sockets.empty()) return -1;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(sockets.size()) - 1);

    return sockets[dis(gen)];
}

int GameServer::getOpponentSocket(std::shared_ptr<Player>& sender) {
    const auto& players = sender->gameRoom->getPlayers();
    for (const auto& [sock, player] : players) {
        if (sock != sender->socket && player->gameRoom->isPlayerGamer(player->ID)) {
            return sock;
        }
    }
    return -1;
}