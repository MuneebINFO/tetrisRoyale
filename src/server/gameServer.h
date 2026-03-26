#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <sys/select.h>
#include <sys/socket.h>

#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <vector>
#include <algorithm>

#include "../common/CONSTANT.h"
#include "../common/header.h"

class GameRoom;
struct Position {
    int x_;
    int y_;
};

struct Tetramino {
    int x_, y_;  // position de départ
    std::vector<std::vector<int>> shape_;
    std::uint8_t colorIndex_;

    Tetramino() : x_(0), y_(0), shape_(4, std::vector<int>(4, 0)), colorIndex_(0) {}

    Tetramino(int x, int y, std::vector<std::vector<int>> shape,
              std::uint8_t colorIndex)
        : x_(x), y_(y), shape_(shape), colorIndex_(colorIndex) {}
};

struct GameState {
    Position current_;
    Tetramino currentTetramino;
    int points_ = 0;
    int miniTetraCount_ = 0;
    std::vector<std::vector<int>> grid_;
    std::vector<std::vector<int>>
        currentShape_;  // matrice du tetramino courant
    // les précédentes couleurs des tetraminos
    std::vector<std::uint8_t> previousColors_;

    // initialise la grille et le shape du tetramino
    void init() {
        grid_.resize(HEIGHT, std::vector<int>(WIDTH, 0));
        currentShape_.clear();
    }
};

// Structure to store player information
struct Player {
    int socket;
    std::string username;
    bool isPlaying;
    int ID;
    bool isInChatRoom;
    char receiver[MAX_NAME_LENGTH];
    std::shared_ptr<GameRoom> gameRoom;    // lien vers la salle de jeu
    std::shared_ptr<GameState> gameState;  // état du joueur
    int energy = 0;
    bool alive = true;
};

struct UserAccount {
    std::string username;
    std::string password;
    int playerID;
};

// Class to manage a game room
class GameRoom : public std::enable_shared_from_this<GameRoom> {
   private:
    bool running_;
    bool playing_;
    int roomId_;  // Unique ID for the game room
    int groupeLeader_;
    std::vector<int> gamers_;  // their id's
    uint8_t nbrGamerMax_;
    int nbrPlayers_;
    std::map<int, std::shared_ptr<Player>> players_;
    // les sockets suivis et les sockets prêts à lire/écrire
    fd_set allActiveSockets_, readySockets_;
    // plus grand des descripteurs de fichiers
    int maxFD_;
    std::string gameMode_;

    void handlePlayerMessages(std::shared_ptr<Player> player);
    void processMessage(std::shared_ptr<Player> player,
                        const std::string& message);

   public:
    static void* loop(void*);
    // Broadcast a message to all players in the room
    void broadcast(int excludePlayer, char* buffer, ssize_t size);
    bool isPlayerGamer(int playerID);

    void addPlayer(std::shared_ptr<Player> player, bool asGamer);
    void removePlayer(std::shared_ptr<Player> player);
    void playerLeaves(std::shared_ptr<Player> player);

    void sendLobbyUpdate();
    void sendParticipantList(std::shared_ptr<Player> player);
    void sendEndLobby();
    void sendParticipantLeaving(std::shared_ptr<Player> player, bool);
    void handleStartRequest(std::shared_ptr<Player> player);
    void deleteAllInvitations(int roomId);
    int countAlivePlayers();

    // Getters and setters
    void setRunning(bool running) { running_ = running; }
    bool isRunning() { return running_; }
    void setPlaying(bool playing) { playing_ = playing; }
    bool isPlaying() { return playing_; }
    void setRoomId(int id) { roomId_ = id; }
    int getRoomId() { return roomId_; }
    void setGroupeLeader(int leader) { groupeLeader_ = leader; }
    int getGroupeLeader() { return groupeLeader_; }
    void setNbrPlayers(int nbr);
    int getNbrPlayers() { return nbrPlayers_; }
    void setNbrGamerMax(uint8_t nbr) { nbrGamerMax_ = nbr; }
    uint8_t getNbrGamerMax() { return nbrGamerMax_; }
    void setGameMode(const std::string& mode) { gameMode_ = mode; }
    std::string getGameMode() { return gameMode_; }
    const std::map<int, std::shared_ptr<Player>>& getPlayers() const {
        return players_;
    }

    // Permet d’obtenir un shared_ptr vers cette instance
    std::shared_ptr<GameRoom> getShared() { return shared_from_this(); }
};

class GameServer {
   private:
    std::shared_ptr<Player> currentPlayer;
    std::shared_ptr<GameState> currentPlayerGameState;

   public:
    GameServer(std::shared_ptr<Player> player_, std::shared_ptr<GameState> gs)
        : currentPlayer(player_), currentPlayerGameState(gs) {}
    

    std::shared_ptr<Player>& getPlayer() { return currentPlayer; }
    std::shared_ptr<GameState>& getPlayerGameState() {
        return currentPlayerGameState;
    }

    Tetramino generateRandomTetramino();
    bool handleTetramino(SpawnTetraminoPayload& payload);
    void handleMove(GameUpdateHeader& update, MovementPayload& payload);
    bool handleRotate(GameUpdateHeader& update, RotationPayload& payload);
    void handleMalus(MalusPayload& payload);
    void handleBonus(BonusPayload& payload);

    std::vector<std::vector<int>> rotate(
        const std::vector<std::vector<int>>& shape, bool clockwise);
    // vérifie si le tetramino peut être déplace à une position donnée
    bool canMoveOrPlace(int dx, int dy);
    void clearFullRow(GameUpdateHeader& update);
    bool checkRotation(std::vector<std::vector<int>>& rotatedShape,
                       Tetramino& t);
    void checkIfMalus(GameUpdateHeader& update);
    void sendGridToOpponent();
    // envoie le malus du mode Classic et Duel
    void sendRowMalus(int targetSocket, int lines, int emptyIdx);
    int randomTarget();
    int getOpponentSocket();
    int IdToSocket(int playerID);
    bool isMiniTetraActive(std::shared_ptr<GameState> &gs) { return gs->miniTetraCount_ > 0; }
    void setMiniTetra(std::shared_ptr<GameState> &gs) { gs->miniTetraCount_ = 2; }
    void reduceMiniTetraCount(std::shared_ptr<GameState> &gs) { gs->miniTetraCount_--; }
    void addPoints(std::shared_ptr<GameState> &gs, int points) { gs->points_ += points; }
    int addMalusRow(std::vector<std::vector<int>>& grid, int malusCount);
    // renvoie la réference de la grille d'un autre joueur de la gameroom
    std::vector<std::vector<int>>& getGridFromSocket(int socket);
};

#endif
