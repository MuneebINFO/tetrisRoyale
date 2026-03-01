#include "Lobby.h"

#include <pthread.h>

#include <memory>

#include "Controller.h"
#include "View.h"
#include "Game.h"
#include "Message.h"
#include "Player.h"
#include "Server.h"
#include "Tetris.h"

Lobby::Lobby(Tetris& tetris, std::shared_ptr<IController> controller,
             std::shared_ptr<Player> player, std::shared_ptr<IView> view,
             std::shared_ptr<Server> server)
    : signal_{Signal::getInstance()},
      tetris_{tetris},
      controller_{controller},
      player_{player},
      view_{view},
      server_{server},
      groupeLeader_{false},
      numberOfPlayer_{},
      gameMode_{},
      isSetup_{false} {
    groupeLeader_ = player_->getPlayerId();
}

Lobby::~Lobby() {
    char buffer[BUFFER_SIZE];
    Message message;
    if (groupeLeader_ == player_->getPlayerId()) {
        message.serializeLobby(LOBBY_TYPE::LEAVE, *this, buffer);
    } else if (groupeLeader_ != -1) {
        message.serializeLobby(LOBBY_TYPE::LEAVE, *this, buffer);
    }
    server_->sendMessage(buffer, sizeof(Header) + sizeof(LobbyHeader));
}

void Lobby::setupGame() {
    if (getIsSetup()) {
        return modifyLobby();
    }
    char buffer[BUFFER_SIZE];
    Message message;
    message.serializeLobby(LOBBY_TYPE::CREATE, *this, buffer);
    server_->sendMessage(buffer, sizeof(Header) + sizeof(LobbyHeader));
    std::string response = server_->receive();
    LobbyResponseHeader responseHeader;
    memcpy(&responseHeader, response.c_str(), sizeof(LobbyResponseHeader));
    if (responseHeader.responseType == LOBBY_RESPONSE::CREATED) {
        gameRoomId_ = responseHeader.idRoom;
    }
    setIsGamer(true);
    setGroupeLeader(player_->getPlayerId());
    addPlayer(player_->getPlayerId(), player_->getName(), getIsGamer());
    setIsSetup(true);
}

void Lobby::startGame() {
    /* if (player_->getPlayerId() == groupeLeader_) {
        char buffer[BUFFER_SIZE];
        Message message;
        message.serializeLobby(LOBBY_TYPE::START, *this, buffer);
        server_->sendMessage(buffer, sizeof(Header) + sizeof(LobbyHeader));
    } */

    try {
        std::unique_ptr<Game> game =
            std::make_unique<Game>(player_, view_, controller_, server_);

        game->setupBoards({1, 2, 3, 4, 5, 6, 7}, true);
        game->setIsSpectator(!getIsGamer());
        game->setGameMode(gameMode_);
        if (getIsGamer()) {
            game->start();
        } else {
            game->startSpectator();
        }
        game = nullptr;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return;
    }
    reset();
}

void Lobby::sendNotification(LobbyInviteFriend& lIF) {
    size_t size = sizeof(Header) + sizeof(LobbyHeader);
    char buffer[BUFFER_SIZE];
    Message message;
    message.serializeLobby(LOBBY_TYPE::INVITE, *this, buffer);
    LobbyInviteFriend lobbyInviteFriend;
    memset(&lobbyInviteFriend, 0, sizeof(LobbyInviteFriend));

    lobbyInviteFriend.idRoom = getRoomId();
    memcpy(lobbyInviteFriend.nameInviting, lIF.nameInviting,
           sizeof(lIF.nameInviting));
    memcpy(lobbyInviteFriend.gameMode, gameMode_.c_str(), gameMode_.size());
    lobbyInviteFriend.asGamer = lIF.asGamer;

    memcpy(buffer + size, &lobbyInviteFriend, sizeof(LobbyInviteFriend));
    server_->sendMessage(buffer, sizeof(Header) + sizeof(LobbyHeader) +
                                     sizeof(LobbyInviteFriend));
}

void Lobby::sendStartNotification() {
    char buffer[BUFFER_SIZE];
    Message message;
    message.serializeLobby(LOBBY_TYPE::START, *this, buffer);
    server_->sendMessage(buffer, sizeof(Header) + sizeof(LobbyHeader));
}

void Lobby::reset() {
    if (!getIsSetup()) {
        return;
    }
    leaveLobby();
    groupeLeader_ = -1;
    numberOfPlayer_ = 0;
    gameMode_.clear();
    gameRoomId_ = -1;
    players_.clear();
    spectators_.clear();
    isSetup_ = false;
}
void Lobby::modifyLobby() {
    view_->showLobbyModify();
    sendUpdateLobby();
}
void Lobby::sendUpdateLobby() {
    char buffer[BUFFER_SIZE];
    Message message;
    message.serializeLobby(LOBBY_TYPE::MODIFY, *this, buffer);
    LobbyHeader lobbyHeader;
    memcpy(&lobbyHeader, buffer + sizeof(Header), sizeof(LobbyHeader));
    server_->sendMessage(buffer, sizeof(Header) + sizeof(LobbyHeader));
}
void Lobby::leaveLobby() {
    char buffer[BUFFER_SIZE];
    Header header;
    header.type = MESSAGE_TYPE::LOBBY;
    header.idInstance = player_->getPlayerId();
    header.sizeMessage = sizeof(LobbyHeader);
    LobbyHeader lobbyHeader;
    lobbyHeader.type = LOBBY_TYPE::LEAVE;
    lobbyHeader.idRoom = getRoomId();
    lobbyHeader.nbPlayers = 0;
    memcpy(lobbyHeader.gameMode, getGameMode().c_str(), getGameMode().size());
    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &lobbyHeader, sizeof(LobbyHeader));
    server_->sendMessage(buffer, sizeof(Header) + sizeof(LobbyHeader));
}
// Send the message to the server for joining the lobby
void Lobby::joinLobby(LobbyInvitation invitation) {
    int gameRoomId = invitation.roomId;
    setIsGamer(invitation.asGamer);
    setGameMode(invitation.gameMode);
    setRoomId(gameRoomId);
    setGroupeLeader(invitation.idPlayerInviting);
    addPlayer(player_->getPlayerId(), player_->getName(), getIsGamer());

    char buffer[BUFFER_SIZE];
    Message message;
    message.serializeLobby(LOBBY_TYPE::JOIN, *this, buffer);
    LobbyHeader header;
    memcpy(&header, buffer + sizeof(Header), sizeof(LobbyHeader));
    header.idRoom = gameRoomId;
    memcpy(buffer + sizeof(Header), &header, sizeof(LobbyHeader));
    LobbyJoinning LbJ = {.asGamer = getIsGamer()};
    memcpy(buffer + sizeof(Header) + sizeof(LobbyHeader), &LbJ,
           sizeof(LobbyJoinning));
    server_->sendMessage(
        buffer, sizeof(Header) + sizeof(LobbyHeader) + sizeof(LobbyJoinning));
    setIsSetup(true);
}

// Wait for the game to start in the lobby
// Process the server messages and user input
void Lobby::waitGameStart(bool isLeader) {
    Signal& signal = Signal::getInstance();
    bool running = true;
    int socketFd = server_->getSocket();
    fd_set readfds;
    int maxfd = std::max(STDIN_FILENO, socketFd);
    while (running and getIsSetup() and signal.getSigIntFlag() == 0) {
        view_->showLobbyWaitingRoom(isLeader);
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(socketFd, &readfds);
        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select() in waiting room");
            break;
        }
        if (FD_ISSET(socketFd, &readfds)) {
            LobbyMessage lobbyMessage = LobbyMessage(server_);
            lobbyMessage.handleWaitingRoom(this, running);
        } else if (FD_ISSET(STDIN_FILENO, &readfds)) {
            controller_->waitingRoomInput(isLeader, running);
        }
    }
    reset();
    tetris_.setMenuState(MENU_STATE::MAIN);
}

void Lobby::addPlayer(int playerId, std::string playerName, bool isGamer) {
    if (isGamer) {
        players_.push_back(std::make_pair(playerId, playerName));
    } else {
        spectators_.push_back(std::make_pair(playerId, playerName));
    }
}

void Lobby::removePlayer(int playerId, bool asGamer) {
    if (asGamer) {
        for (auto it = players_.begin(); it != players_.end(); ++it) {
            if (it->first == playerId) {
                players_.erase(it);
                break;
            }
        }
    } else {
        for (auto it = spectators_.begin(); it != spectators_.end(); ++it) {
            if (it->first == playerId) {
                spectators_.erase(it);
                break;
            }
        }
    }
}
