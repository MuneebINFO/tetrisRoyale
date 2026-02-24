#include "Game.h"

#include <fcntl.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>  // Pour std::rand()
#include <iostream>
#include <vector>

#include "Controller.h"
#include "Game.h"
#include "InvitationManager.h"
#include "Player.h"
#include "Server.h"
#include "Tetris.h"
#include "View.h"

// HACK: Pour simuler les spectateurs
void copyGrid(
    std::map<std::uint16_t, std::vector<std::vector<std::uint8_t>>>& grids,
    std::vector<std::vector<std::uint8_t>> grid);

Board::Board(std::vector<std::uint16_t> idPlayers, bool hasGrid)
    : hasGrid_{hasGrid} {
    grid_ = std::vector<std::vector<std::uint8_t>>(
        HEIGHT, std::vector<std::uint8_t>(WIDTH, 0));
    for (std::uint16_t id : idPlayers) {
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
      previousColors_{},
      current_{generateRandomTetramino()} {
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

void Game::tetraminoFall() {
    while (running_) {
        while (getPaused() && getRunning()) {
            pthread_mutex_lock(&mutexStateM);
            pthread_cond_wait(&mutexStateC, &mutexStateM);
            pthread_mutex_unlock(&mutexStateM);
        }

        sendMovementMessage(1, 0);

        pthread_mutex_lock(&mutexGrid_);
        view_->showGame(this);
        pthread_mutex_unlock(&mutexGrid_);

        usleep(SLEEP_TIME - points_ * 10);
        checkSignal();
    }
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
}

Tetramino Game::generateRandomTetramino() {
    int shapeIndex = int(rand() % SHAPES.size());

    // Si toutes les couleurs ont été utilisées, on réinitialise
    if (previousColors_.size() == 7) {
        previousColors_.clear();
    }

    // Pour choisir une couleur différente des précédentes
    std::uint8_t colorIndex;
    do {
        colorIndex = std::uint8_t(rand() % NUMBER_OF_COLORS + 1);
    } while (std::find(previousColors_.begin(), previousColors_.end(),
                       colorIndex) != previousColors_.end());

    previousColors_.push_back(colorIndex);
    return Tetramino(0, int(WIDTH / 2 - SHAPES[shapeIndex][0].size() / 2),
                     SHAPES[shapeIndex], colorIndex);
}

void Game::checkSignal() {
    if (signal_.getSigIntFlag()) {
        running_ = false;
    }
    if (signal_.checkSigTstp(*this) and paused_) {
        // std::cout << "Game is paused" << std::endl;
    }
}

void Game::start() {
    view_->clearScreen();
    srand(   // une graine différente pour chaque joueur
        static_cast<unsigned>(
            time(nullptr) + getpid() + player_->getPlayerId())); 
            
    // ecoute du serveur
    pthread_create(&listenerThread, nullptr, Game::receiveLoopHelper, this);

    sendSpawnTetramino(current_.shape_, current_.x_, current_.y_);

    controller_->captureInput(this);
    tetraminoFall();
    controller_->stop();
}

// reçoit et traite tout ce qui vient du serveur en continu
void* Game::receiveLoop() {
    while (running_) {
        processServerResponse();
    }
    return nullptr;
}

void* Game::receiveLoopHelper(void* arg) {
    return static_cast<Game*>(arg)->receiveLoop();
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

void Game::setupBoards(std::vector<std::uint16_t> idPlayers, bool hasGrid) {
    board_ = Board(idPlayers, hasGrid);
}

void Game::sendSignalMutex() {
    pthread_mutex_lock(&mutexStateM);
    pthread_cond_signal(&mutexStateC);
    pthread_mutex_unlock(&mutexStateM);
}

// toDelete after real implementation
void copyGrid(
    std::map<std::uint16_t, std::vector<std::vector<std::uint8_t>>>& grids,
    std::vector<std::vector<std::uint8_t>> grid) {
    for (auto& [id, g] : grids) {
        g = grid;
    }
}

void Game::sendSpawnTetramino(const std::vector<std::vector<int>>& shape, int x,
                              int y) {
    Header header;
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(SpawnTetraminoPayload);

    GameTypeHeader gameHeader;
    gameHeader.type = GAME_TYPE::SPAWN_TETRAMINO;

    SpawnTetraminoPayload payload{};
    payload.x = uint8_t(x);
    payload.y = uint8_t(y);
    payload.width = uint8_t(shape[0].size());
    payload.height = uint8_t(shape.size());

    for (int i = 0; i < payload.height; ++i) {
        for (int j = 0; j < payload.width; ++j) {
            payload.shape[i][j] = uint8_t(shape[i][j]);
        }
    }

    char buffer[sizeof(Header) + sizeof(GameTypeHeader) +
                sizeof(SpawnTetraminoPayload)];
    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(Header) + sizeof(GameTypeHeader), &payload,
           sizeof(SpawnTetraminoPayload));

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

/*void Game::sendMalusMessage(int malusType, int targetID) {
    Header header;
    memset(&header, 0, sizeof(Header));
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + 2 * sizeof(int);

    GameTypeHeader gameHeader;
    memset(&gameHeader, 0, sizeof(GameTypeHeader));
    gameHeader.type = GAME_TYPE::MALUS;

    MalusPayload payload;

    payload.malusType = malusType;
    payload.targetSocket = targetID;

    size_t totalSize =
        sizeof(Header) + sizeof(GameTypeHeader) + sizeof(MalusPayload);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(Header) + sizeof(GameTypeHeader), &payload,
           sizeof(MalusPayload));

    server_->sendMessage(buffer, totalSize);
}*/

/*void Game::sendBonusMessage(int bonusType) {
    Header header;
    memset(&header, 0, sizeof(Header));
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + 2 * sizeof(int);

    GameTypeHeader gameHeader;
    memset(&gameHeader, 0, sizeof(GameTypeHeader));
    gameHeader.type = GAME_TYPE::BONUS;

    BonusPayload payload;

    payload.type = bonusType;

    size_t totalSize =
        sizeof(Header) + sizeof(GameTypeHeader) + sizeof(BonusPayload);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(Header) + sizeof(GameTypeHeader), &payload,
           sizeof(BonusPayload));

    server_->sendMessage(buffer, totalSize);
}*/

void Game::processServerResponse() {
    char recvBuffer[BUFFER_SIZE];
    server_->receiveMessage(recvBuffer);

    HeaderResponse header;
    memcpy(&header, recvBuffer, sizeof(HeaderResponse));

    if (header.type == MESSAGE_TYPE::GAME) {
        GameTypeHeader gameHeader;
        memcpy(&gameHeader, recvBuffer + sizeof(HeaderResponse), sizeof(GameTypeHeader));

        if (gameHeader.type == GAME_TYPE::MOVE) {
            GameUpdateHeader update;
            memcpy(&update, recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader), sizeof(GameUpdateHeader));

            pthread_mutex_lock(&mutexTetramino_);
            current_.x_ = update.x;
            current_.y_ = update.y;
            pthread_mutex_unlock(&mutexTetramino_);

        } else if (gameHeader.type == GAME_TYPE::ROTATE) {
            GameUpdateHeader update;
            memcpy(&update, recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader), sizeof(GameUpdateHeader));

            pthread_mutex_lock(&mutexTetramino_);
            current_.shape_ = rotate(current_.shape_, update.rotation);
            pthread_mutex_unlock(&mutexTetramino_);

        } else if (gameHeader.type == GAME_TYPE::LOCK) {
            GameUpdateHeader update;
            memcpy(&update, recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader), sizeof(GameUpdateHeader));

            lockPending_ = true;
            pthread_mutex_lock(&mutexGrid_);
    
            newBoard();
            clearFullRows(update);
    
            pthread_mutex_unlock(&mutexGrid_);
    
            // on passe au suivant
            pthread_mutex_lock(&mutexTetramino_);
            current_ = generateRandomTetramino();
            sendSpawnTetramino(current_.shape_, current_.x_, current_.y_);
            lockPending_ = false;
            pthread_mutex_unlock(&mutexTetramino_);
            return;
            
        } else if (gameHeader.type == GAME_TYPE::UPDATEGRID) {
            GridUpdate updateGrid;
            memcpy(&updateGrid, recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader), sizeof(GridUpdate));

            if (updateGrid.playerID != player_->getPlayerId()) {
                pthread_mutex_lock(&mutexGrid_);
                board_.getBoards()[static_cast<std::uint16_t>(updateGrid.playerID)] = convertGrid(updateGrid.grid);
                pthread_mutex_unlock(&mutexGrid_);
            }

        } else if (gameHeader.type == GAME_TYPE::MALUS) {
            MalusPayload payload;
            memcpy(&payload, recvBuffer + sizeof(HeaderResponse) + sizeof(GameTypeHeader), sizeof(MalusPayload));

            applyMalus(payload);

        } else if (gameHeader.type == GAME_TYPE::LOST || gameHeader.type == GAME_TYPE::WIN) {
            running_ = false;

            bool won = (gameHeader.type == GAME_TYPE::WIN);
            view_->showEndScreen(won);
        }

        pthread_mutex_lock(&mutexGrid_);  
        view_->showGame(this);
        pthread_mutex_unlock(&mutexGrid_);
    }
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
                rotated[j][cols - 1 - i] = shape[i][j];
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

std::vector<std::vector<std::uint8_t>> Game::convertGrid(uint8_t raw[HEIGHT][WIDTH]) {
    std::vector<std::vector<std::uint8_t>> result(HEIGHT, std::vector<std::uint8_t>(WIDTH));
    for (int i = 0; i < HEIGHT; ++i)
        for (int j = 0; j < WIDTH; ++j)
            result[i][j] = raw[i][j];
    return result;
}

/*void Game::receiveMalus() {
    char buffer[sizeof(MalusPayload)];

    if (server_->receiveMessage(buffer) != 0) {
        std::cerr << "[CLIENT] Erreur lors de la réception du malus."
                  << std::endl;
        return;
    }

    MalusPayload payload;

    memcpy(&payload, buffer, sizeof(MalusPayload));

    applyMalus(payload);  // Appliquer immédiatement le malus
}*/

void Game::applyMalus(MalusPayload& payload) {
    int malusType = payload.malusType;

    switch (malusType) {
        case 1:  {    // Ajout d'une ligne grise
            int lines = payload.details;
            addMalusRow(lines);
            sendConfirmMalus(payload);
            break;
        }
        case 2:       // Bloquer les commandes pour le prochain bloc
            blockedInput_ = true;
            break;
        case 3:
            break;
        // etc
        default:
            std::cerr << "[CLIENT] Malus inconnu reçu." << std::endl;
            break;
    }

    // Mettre à jour l'affichage apres maus
    pthread_mutex_lock(&mutexGrid_);
    view_->showGame(this);
    pthread_mutex_unlock(&mutexGrid_);
}

/*void Game::applyBonus(int type) {
    switch (type) {
        case 1:
            addLuckyPoints();
            break;
        case 2:
            break;
        // etc
        default:
            std::cerr << "[CLIENT] Bonus inconnu reçu." << std::endl;
            break;
    }
}*/

// Ajout d'une ligne Malus
void Game::addMalusRow(int lines) {
    auto& grid = board_.getGrid();

    for (int l = 0; l < lines; ++l) {
        // supprimer la première ligne
        grid.erase(grid.begin());
        grid.push_back(std::vector<std::uint8_t> (WIDTH, 9));
    }
}

/*void Game::addLuckyPoints() {
    int prob = rand() % 100;

    if (prob < 60) {  // 60% de chance de gagner 0 point
        view_->showMessage("Bonus : Pas de chance, 0 point gagné !", 0);
    } else if (prob < 90) {  // 30% de chance de gagner 100 point
        points_ += 100;
        view_->showMessage("Bonus : Vous gagnez 100 points !", 0);
    } else {  // 10% de chance de gagner 1000 point
        points_ += 1000;
        view_->showMessage("Bonus : Jackpot ! Vous gagnez 1000 points !", 0);
    }
}*/

void Game::sendConfirmMalus(MalusPayload& malus) {
    Header header;
    memset(&header, 0, sizeof(Header));
    header.type = MESSAGE_TYPE::GAME;
    header.sizeMessage = sizeof(GameTypeHeader) + sizeof(MalusPayload);

    GameTypeHeader gameHeader;
    memset(&gameHeader, 0, sizeof(GameTypeHeader));
    gameHeader.type = GAME_TYPE::MALUS_CONFIRM;

    char buffer[sizeof(Header) + sizeof(GameTypeHeader) +
                sizeof(MalusPayload)];
    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &gameHeader, sizeof(GameTypeHeader));
    memcpy(buffer + sizeof(Header) + sizeof(GameTypeHeader), &malus,
           sizeof(MalusPayload));

    server_->sendMessage(buffer, sizeof(buffer));
}
