#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <sys/select.h>
#include <sys/socket.h>

#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <random>

#include "../common/CONSTANT.h"
#include "../common/header.h"

class GameRoom;
struct Position {
    int x_;
    int y_;
};

struct GameState {
    Position current_;
    int points_ = 0;
    std::vector<std::vector<int>> grid_;
    std::vector<std::vector<int>>
        currentShape_;  // matrice du tetramino courant

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
    fd_set allActiveSockets_, readySockets_;
    int maxFD_;
    std::string gameMode_;

    void handlePlayerMessages(std::shared_ptr<Player> player);
    void processMessage(std::shared_ptr<Player> player, const std::string& message);

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
    const std::map<int, std::shared_ptr<Player>>& getPlayers() const { return players_; }
    std::shared_ptr<GameRoom> getShared() {
        return shared_from_this();
    } 
};

class GameServer {
   private:
    std::string currentMode_;

   public:
    bool handleSpawnTetramino(std::shared_ptr<Player>& player,
                              SpawnTetraminoPayload& payload);
    void handleMove(std::shared_ptr<Player>& player, GameUpdateHeader& update,
                    MovementPayload& payload);
    void handleRotate(std::shared_ptr<Player>& player, GameUpdateHeader& update,
                      RotationPayload& payload);
    void handleConfirmMalus(std::shared_ptr<Player>& player, 
                            MalusPayload& payload);
    void handleMalus();
    void handleBonus();
    void handlestart();
    void handleEnd();

    std::vector<std::vector<int>> rotate(
        const std::vector<std::vector<int>>& shape, bool clockwise);
    void clearOrReplaceForMove(std::vector<std::vector<int>>& shape,
                               std::vector<std::vector<int>>& grid,
                               const Position& t, bool info);
    void clearOrReplaceForRotate(GameState& gs,
                                 std::vector<std::vector<int>>& shape,
                                 const Position& t, bool info);
    bool canMoveOrPlace(const GameState& gameState, int dx, int dy);
    void clearFullRow(GameState& gs, GameUpdateHeader& update);
    bool checkRotation(std::vector<std::vector<int>>& rotatedShape,
                        GameState& gs, Position& t);
    void checkIfMalus(GameUpdateHeader &update, std::shared_ptr<Player> &player);
    void sendGridToOpponent(std::shared_ptr<Player>& player,
                            GameState& gs);
    void sendMalus(std::string gameMode, int targetSocket, int lines);
    int randomTarget(std::shared_ptr<Player>& sender);
    int getOpponentSocket(std::shared_ptr<Player>& sender);
};

#endif
