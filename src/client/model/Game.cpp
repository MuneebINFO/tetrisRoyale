#include "Game.h"

#include <fcntl.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <ncurses.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>  // Pour std::rand()
#include <iostream>
#include <vector>

#include "../controller/Controller.h"
#include "../view/View.h"
#include "Game.h"
#include "InvitationManager.h"
#include "Player.h"
#include "Server.h"
#include "Tetris.h"

Board::Board(std::vector<int> idPlayers, bool hasGrid) : hasGrid_{hasGrid} {
    grid_ = std::vector<std::vector<std::uint8_t>>(
        HEIGHT, std::vector<std::uint8_t>(WIDTH, 0));
    for (int id : idPlayers) {
        boards_[id] = std::vector<std::vector<std::uint8_t>>(
            HEIGHT, std::vector<std::uint8_t>(WIDTH, 0));
    }
}

Game::Game(std::shared_ptr<Player> player, std::shared_ptr<IView> view,
           std::shared_ptr<IController> controller,
           std::shared_ptr<Server> server) try
    : running_{true},
      paused_{false},
      points_{0},
      signal_{Signal::getInstance()},
      player_{player},
      view_{view},
      controller_{controller},
      server_{server},
      current_{} {
    if (pthread_mutex_init(&mutexTetramino_, nullptr) != 0 or
        pthread_mutex_init(&mutexGrid_, nullptr) != 0 or
        pthread_mutex_init(&mutexStateM, nullptr) != 0 or
        pthread_cond_init(&mutexStateC, nullptr) != 0) {
        throw std::runtime_error("Error: mutex init failed");
    }
} catch (const std::exception& e) {
    throw;
}

Game::~Game() {
    pthread_mutex_destroy(&mutexStateM);
    pthread_cond_destroy(&mutexStateC);
    pthread_mutex_destroy(&mutexTetramino_);
    pthread_mutex_destroy(&mutexGrid_);
}

void Game::clearFullRows(GameUpdateHeader& update) {
    auto& grid_ = board_.getGrid();
    for (int i = 0; i < update.linesCleared; ++i) {
        int row = update.rows[i];
        if (row >= 0 && row < HEIGHT) {
            grid_.erase(grid_.begin() + row);
            grid_.emplace(grid_.begin(), WIDTH, 0);
        }
    }

    points_ = update.score;
    energy += update.linesCleared;
}

void Game::checkSignal() {
    if (signal_.getSigIntFlag()) {
        setRunning(false);
    }
    if (signal_.checkSigTstp(*this) and paused_) {
        // std::cout << "Game is paused" << std::endl;
    }
}

void Game::startSpectator() {
    view_->clearScreen();
    showGame();
    controller_->captureInput(this);
}

int Game::start() {
    view_->clearScreen();
    reset();

    srand(static_cast<unsigned>(time(nullptr) + getpid() +
                                player_->getPlayerId()) *
          273821);

    sendStartMessage();
    askTetraminoToServer();

    // ncurses non bloquant
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    int sock = server_->getSocket();

    auto lastFall = std::chrono::steady_clock::now();

    while (getRunning()) {
        int ch = getch();
        if (ch != ERR) {
            controller_->handleKey(ch, this);
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 2000; // 2ms

        int activity = select(sock + 1, &readfds, nullptr, nullptr, &tv);
        if (activity > 0 && FD_ISSET(sock, &readfds)) {
            processServerResponse();

            while (true) {
                fd_set tmp;
                FD_ZERO(&tmp);
                FD_SET(sock, &tmp);
                timeval tv2;
                tv2.tv_sec = 0;
                tv2.tv_usec = 0;

                int a2 = select(sock + 1, &tmp, nullptr, nullptr, &tv2);
                if (a2 > 0 && FD_ISSET(sock, &tmp)) {
                    processServerResponse();
                } else {
                    break;
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsedUs =
            std::chrono::duration_cast<std::chrono::microseconds>(now - lastFall)
                .count();

        long delayUs = static_cast<long>(SLEEP_TIME) - static_cast<long>(points_) * 10L;

        if (isSpeedUp()) {
            delayUs = delayUs / 3;
        } else if (isSpeedDown()) {
            delayUs = delayUs * 3;
        }

        if (delayUs < 50000) delayUs = 50000; // 50ms minimum

        if (elapsedUs >= delayUs) {
            sendMovementMessage(1, 0);
            lastFall = now;
        }

        showGame();
        usleep(2000);
    }

    return getIsLeaving();
}

void Game::reset() {
    setRunning(true);
    setPaused(false);
    points_ = 0;
    energy = 0;
    blockedInput_ = false;
    lockPending_ = false;
    leaving_ = false;
    invCMDCount_ = 0;
    speedUp_ = false;
    blackScreenSec = 0;
    speedDownSec = 0;
    controllerWait = false;
    activeMalus.clear();
    playerNames_.clear();
    current_ = Tetramino();
}

bool Game::getRunning() {
    pthread_mutex_lock(&mutexStateM);
    bool running = running_;
    pthread_mutex_unlock(&mutexStateM);
    return running;
}

void Game::setRunning(bool running) {
    pthread_mutex_lock(&mutexStateM);
    running_ = running;
    pthread_mutex_unlock(&mutexStateM);
}

void Game::setPaused(bool paused) {
    pthread_mutex_lock(&mutexStateM);
    paused_ = paused;
    pthread_cond_broadcast(&mutexStateC);
    pthread_mutex_unlock(&mutexStateM);
}

void Game::setupBoards(std::vector<int> idPlayers, bool hasGrid) {
    board_ = Board(idPlayers, hasGrid);
}

void Game::sendSignalMutex() {
    pthread_mutex_lock(&mutexStateM);
    pthread_cond_signal(&mutexStateC);
    pthread_mutex_unlock(&mutexStateM);
}

void Game::showGame() {
    pthread_mutex_lock(&mutexGrid_);
    pthread_mutex_lock(&mutexTetramino_);
    view_->showGame(this);
    pthread_mutex_unlock(&mutexTetramino_);
    pthread_mutex_unlock(&mutexGrid_);
}

void Game::askTetraminoToServer() {
    Header header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader);

    GameTypeHeader gameHeader;
    gameHeader.type = GAME_TYPE::TETRAMINO_REQUEST;

    char buffer[sizeof(Header) + sizeof(GameTypeHeader)];
    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));

    server_->sendMessage(buffer, sizeof(buffer));
}

void Game::sendMovementMessage(int dx, int dy) {
    if (lockPending_) return;

    Header header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(MovementPayload);

    GameTypeHeader gameHeader;
    gameHeader.type = GAME_TYPE::MOVE;

    MovementPayload payload{dx, dy};

    char buffer[sizeof(Header) + sizeof(GameTypeHeader) +
                sizeof(MovementPayload)];
    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(Header) + sizeof(GameTypeHeader), &payload,
           sizeof(MovementPayload));

    server_->sendMessage(buffer, sizeof(buffer));
}

void Game::sendRotationMessage(bool clockwise) {
    Header header;
    memset(&header, 0, sizeof(Header));
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(bool);

    GameTypeHeader gameHeader;
    memset(&gameHeader, 0, sizeof(GameTypeHeader));
    gameHeader.type = GAME_TYPE::ROTATE;

    RotationPayload payload{clockwise};

    char buffer[sizeof(Header) + sizeof(GameTypeHeader) +
                sizeof(RotationPayload)];
    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(Header) + sizeof(GameTypeHeader), &payload,
           sizeof(RotationPayload));

    server_->sendMessage(buffer, sizeof(buffer));
}

void Game::sendMalusMessage(MalusPayload& payload) {
    Header header;
    memset(&header, 0, sizeof(Header));
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(MalusPayload);

    GameTypeHeader gameHeader;
    memset(&gameHeader, 0, sizeof(GameTypeHeader));
    gameHeader.type = GAME_TYPE::MALUS;

    size_t totalSize =
        sizeof(Header) + sizeof(GameTypeHeader) + sizeof(MalusPayload);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(Header) + sizeof(GameTypeHeader), &payload,
           sizeof(MalusPayload));

    server_->sendMessage(buffer, totalSize);
}

void Game::sendBonusMessage(BonusPayload& payload) {
    Header header;
    memset(&header, 0, sizeof(Header));
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(BonusPayload);

    GameTypeHeader gameHeader;
    memset(&gameHeader, 0, sizeof(GameTypeHeader));
    gameHeader.type = GAME_TYPE::BONUS;

    size_t totalSize =
        sizeof(Header) + sizeof(GameTypeHeader) + sizeof(BonusPayload);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(Header) + sizeof(GameTypeHeader), &payload,
           sizeof(BonusPayload));

    server_->sendMessage(buffer, totalSize);
}

// Receives a message from the server and processes it
void Game::processServerResponse() {
    char recvBuffer[BUFFER_SIZE];
    server_->receiveMessage(recvBuffer);

    HeaderResponse header;
    memcpy(&header, recvBuffer, sizeof(HeaderResponse));

    if (header.type != MESSAGE_TYPE::GAME) return;

    GameTypeHeader gameHeader;
    memcpy(&gameHeader,
           recvBuffer + sizeof(HeaderResponse),
           sizeof(GameTypeHeader));

    if (gameHeader.type == GAME_TYPE::TETRAMINO_REQUEST) {
        handleTetramino(recvBuffer);

    } else if (gameHeader.type == GAME_TYPE::MOVE) {
        handleMove(recvBuffer);

    } else if (gameHeader.type == GAME_TYPE::ROTATE) {
        handleRotate(recvBuffer);

    } else if (gameHeader.type == GAME_TYPE::LOCK) {
        handleLock(recvBuffer);
        return;

    } else if (gameHeader.type == GAME_TYPE::UPDATEGRID) {
        handleUpdateGrid(recvBuffer);

    } else if (gameHeader.type == GAME_TYPE::MALUS) {
        handleMalus(recvBuffer);

    } else if (gameHeader.type == GAME_TYPE::LOST ||
               gameHeader.type == GAME_TYPE::WIN ||
               gameHeader.type == GAME_TYPE::END) {
        handleEnd(gameHeader);
        return;

    } else if (gameHeader.type == GAME_TYPE::MALUS_AUTHORISATION) {
        handleMalusAuthorisation();

    } else if (gameHeader.type == GAME_TYPE::BONUS_AUTHORISATION) {
        handleBonusAuthorisation();
    }
}

void Game::handleTetramino(char* recvBuffer) {
    pthread_mutex_lock(&mutexTetramino_);

    SpawnTetraminoPayload payload;
    memcpy(&payload,
           recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader),
           sizeof(SpawnTetraminoPayload));

    current_.x_ = payload.x;
    current_.y_ = payload.y;
    current_.colorIndex_ = payload.color;

    current_.shape_.clear();
    for (int i = 0; i < payload.height; ++i) {
        std::vector<int> row;
        for (int j = 0; j < payload.width; ++j) {
            row.push_back(payload.shape[i][j]);
        }
        current_.shape_.push_back(row);
    }

    pthread_mutex_unlock(&mutexTetramino_);
}

void Game::handleMove(char* recvBuffer) {
    GameUpdateHeader update;
    memcpy(&update,
           recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader),
           sizeof(GameUpdateHeader));

    pthread_mutex_lock(&mutexTetramino_);
    current_.x_ = update.x;
    current_.y_ = update.y;
    pthread_mutex_unlock(&mutexTetramino_);
}

void Game::handleRotate(char* recvBuffer) {
    GameUpdateHeader update;
    memcpy(&update,
           recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader),
           sizeof(GameUpdateHeader));

    pthread_mutex_lock(&mutexTetramino_);
    current_.shape_ = rotate(current_.shape_, update.rotation);
    current_.x_ = update.x;
    current_.y_ = update.y;
    pthread_mutex_unlock(&mutexTetramino_);
}

void Game::handleLock(char* recvBuffer) {
    if (isInputBlocked()) {
        std::string malus = "Commandes bloquées!";
        unblockInput();
        removeActiveMalus(malus);
    }

    if (isInvCMD()) {
        std::string malus = "Commandes inversées!";
        invCMDCount_--;
        if (invCMDCount_ == 0) removeActiveMalus(malus);
    }

    GameUpdateHeader update;
    memcpy(&update,
           recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader),
           sizeof(GameUpdateHeader));

    lockPending_ = true;
    pthread_mutex_lock(&mutexGrid_);

    newBoard();
    clearFullRows(update);

    pthread_mutex_unlock(&mutexGrid_);

    // on passe au suivant
    pthread_mutex_lock(&mutexTetramino_);
    askTetraminoToServer();
    lockPending_ = false;
    pthread_mutex_unlock(&mutexTetramino_);
}

void Game::handleUpdateGrid(char* recvBuffer) {
    GridUpdate updateGrid;
    memcpy(&updateGrid,
           recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader),
           sizeof(GridUpdate));

    // Handle the grid update for the player who is not the current player
    if (updateGrid.playerID != player_->getPlayerId()) {
        pthread_mutex_lock(&mutexGrid_);

        std::string cleanUsername(updateGrid.username,
                                  strnlen(updateGrid.username, 32));
        setPlayerName(updateGrid.playerID, cleanUsername);

        board_.getBoards()[updateGrid.playerID] = convertGrid(updateGrid.grid);

        pthread_mutex_unlock(&mutexGrid_);
    }
}

void Game::handleMalus(char* recvBuffer) {
    MalusPayload payload;
    memcpy(&payload,
           recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader),
           sizeof(MalusPayload));

    applyMalus(payload);
}

void Game::handleEnd(GameTypeHeader& gameHeader) {
    setRunning(false);
    bool won = (gameHeader.type == GAME_TYPE::WIN);
    view_->showEndScreen(won);
    sendQuitParty();
    drainPendingServerMessages();
}

void Game::drainPendingServerMessages(int timeoutMs) {
    if (!server_) {
        return;
    }

    const int sock = server_->getSocket();
    if (sock < 0) {
        return;
    }

    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 20000;

        int activity = select(sock + 1, &readfds, nullptr, nullptr, &tv);
        if (activity <= 0 || !FD_ISSET(sock, &readfds)) {
            break;
        }

        char buffer[BUFFER_SIZE];
        if (server_->receiveMessage(buffer) <= 0) {
            break;
        }
    }
}

void Game::handleMalusAuthorisation() {
    setPaused(true);
    int malusTypeToSent = view_->showMalusType();
    int targetID = view_->showMalusTarget(this);
    setPaused(false);

    MalusPayload payload;
    payload.malusType = malusTypeToSent;
    payload.target = targetID;

    sendMalusMessage(payload);
    setControllerWait(false);
}

void Game::handleBonusAuthorisation() {
    setPaused(true);
    int bonusTypeToApply = view_->showBonusType();
    setPaused(false);

    BonusPayload payload;
    payload.type = bonusTypeToApply;

    applyBonus(payload);
    sendBonusMessage(payload);
    setControllerWait(false);
}

std::vector<std::vector<int>> Game::rotate(
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

void Game::newBoard() {
    auto& grid_ = board_.getGrid();
    for (int i = 0; i < int(current_.shape_.size()); ++i) {
        for (int j = 0; j < int(current_.shape_[0].size()); ++j) {
            if (current_.shape_[i][j] == 1) {
                int gridX = current_.x_ + i;
                int gridY = current_.y_ + j;
                if (gridX >= 0 && gridX < HEIGHT && gridY >= 0 &&
                    gridY < WIDTH) {
                    grid_[gridX][gridY] = current_.colorIndex_;
                }
            }
        }
    }
}

std::vector<std::vector<std::uint8_t>> Game::convertGrid(
    uint8_t raw[HEIGHT][WIDTH]) {
    std::vector<std::vector<std::uint8_t>> result(
        HEIGHT, std::vector<std::uint8_t>(WIDTH));
    for (int i = 0; i < HEIGHT; ++i)
        for (int j = 0; j < WIDTH; ++j) result[i][j] = raw[i][j];
    return result;
}

void Game::applyMalus(MalusPayload& payload) {
    int malusType = payload.malusType;
    std::string malus = "";

    switch (malusType) {
        case 1:  // Ajout d'une ligne grise
            addMalusRow(payload);
            break;

        case 2:  // Bloquer les commandes pour le prochain bloc
            blockedInput_ = true;
            malus = "Commandes bloquées!";
            break;

        case 3:  // inverser les commandes
            invCMDCount_ = 3;
            malus = "Commandes inversées!";
            break;

        case 4:               // accélération du tetramino
            setSpeedDown(0);  // au cas ou il avait ce bonus
            setSpeedUp(true);
            malus = "Accélération!!!";
            break;

        case 5:  // écran noir
            blackScreenStart = std::chrono::steady_clock::now();
            blackScreenSec = 5;
            malus = "Extinction des feux!";
            break;

        default:
            std::cerr << "Malus inconnu reçu." << std::endl;
            break;
    }

    if (malus != "") addActiveMalus(malus);
}

// Ajout d'une ligne Malus
void Game::addMalusRow(MalusPayload& payload) {
    int lines = payload.details;
    auto& grid = board_.getGrid();

    // choisir l'endroit du trou
    int emptyIdx = payload.emptyIdx;

    for (int l = 0; l < lines; ++l) {
        // supprimer la ligne du haut
        grid.erase(grid.begin());

        // créer une ligne de malus gris avec un seul trou
        std::vector<std::uint8_t> malusRow(WIDTH, 9);
        malusRow[emptyIdx] = 0;

        grid.push_back(malusRow);
    }
}

bool Game::isBlackScreen() {
    if (blackScreenSec == 0) return false;

    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - blackScreenStart)
            .count();

    if (elapsed >= blackScreenSec) {
        std::string malus = "Extinction des feux!";
        removeActiveMalus(malus);
        blackScreenSec = 0;
        return false;
    }

    return true;
}

void Game::applyBonus(BonusPayload& payload) {
    int type = payload.type;

    switch (type) {
        case 1:  // Lotterie
            addLuckyPoints(payload);
            break;

        case 2: {               // ralentir
            setSpeedUp(false);  // au cas ou le joueur avait ce malus
            std::string malus = "Accélération!!!";
            removeActiveMalus(malus);
            speedDownStart = std::chrono::steady_clock::now();
            setSpeedDown(5);
            break;
        }

        case 3:  // petit tetramino
            break;

        default:
            std::cerr << "Bonus inconnu reçu." << std::endl;
            break;
    }
}

void Game::addLuckyPoints(BonusPayload& payload) {
    int prob = rand() % 100;
    std::string msg;

    if (prob < 60) {  // 60% de chance de gagner 0 point
        payload.details = 0;

    } else if (prob < 90) {  // 30% de chance de gagner 100 point
        points_ += 100;
        payload.details = 100;

    } else {  // 10% de chance de gagner 1000 point
        points_ += 1000;
        payload.details = 1000;
    }
}

bool Game::isSpeedDown() {
    if (speedDownSec == 0) return false;

    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - speedDownStart)
            .count();

    return elapsed < speedDownSec;
}

void Game::askRoyalPermission(std::string type) {
    Header header;
    memset(&header, 0, sizeof(Header));
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader);

    GameTypeHeader gameHeader;
    memset(&gameHeader, 0, sizeof(GameTypeHeader));
    if (type == "MALUS") {
        gameHeader.type = GAME_TYPE::MALUS_AUTHORISATION;
    } else {
        gameHeader.type = GAME_TYPE::BONUS_AUTHORISATION;
    }

    char buffer[sizeof(Header) + sizeof(GameTypeHeader)];
    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));

    server_->sendMessage(buffer, sizeof(buffer));
    setControllerWait(true);
    if (energy > 0) energy--;
}

void Game::sendQuitParty() {
    Header header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader);

    GameTypeHeader gameHeader;
    gameHeader.type = GAME_TYPE::END;

    char buffer[sizeof(Header) + sizeof(GameTypeHeader)];
    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));

    server_->sendMessage(buffer, sizeof(Header) + sizeof(GameTypeHeader));
}

void Game::sendStartMessage() {
    Header header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader);

    GameTypeHeader gameHeader;
    gameHeader.type = GAME_TYPE::START;

    char buffer[sizeof(Header) + sizeof(GameTypeHeader)];
    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));

    server_->sendMessage(buffer, sizeof(buffer));
}
