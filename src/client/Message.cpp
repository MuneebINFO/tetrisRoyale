#include "Message.h"

#include "Player.h"
#include "Server.h"
#include "View.h"

AccountResponseHeader Message::fromBuffer(char* buffer) {
    AccountResponseHeader accountResponseHeader;
    memcpy(&accountResponseHeader, buffer + sizeof(HeaderResponse),
           sizeof(AccountResponseHeader));
    return accountResponseHeader;
}

void Message::serializeConnection(ACCOUNT_TYPE type, std::string username,
                                  std::string password, char* buffer) {
    Header header;
    header.type = MESSAGE_TYPE::ACCOUNT;
    AccountHeader accountHeader;

    // Copie des données dans la structure
    memset(&accountHeader, 0, sizeof(AccountHeader));
    accountHeader.type = type;
    memcpy(accountHeader.username, username.c_str(), username.size());
    memcpy(accountHeader.password, password.c_str(), password.size());
    header.sizeMessage = sizeof(accountHeader);

    // Copie des données brut dans le buffer
    memcpy(buffer, &header, sizeof(Header));
    memcpy(buffer + sizeof(Header), &accountHeader, sizeof(accountHeader));
}

// LOBBY TYPE
void Message::serializeLobby(LOBBY_TYPE type, Lobby& lobby, char* buffer) {
    Header header;
    header.type = MESSAGE_TYPE::LOBBY;
    header.idInstance = lobby.getPlayer()->getPlayerId();
    header.sizeMessage = sizeof(LobbyHeader);
    LobbyHeader lobbyHeader;
    lobbyHeader.type = type;
    lobbyHeader.idRoom = lobby.getRoomId();
    lobbyHeader.nbPlayers = lobby.getNumberOfPlayer();
    memcpy(lobbyHeader.gameMode, lobby.getGameMode().c_str(),
           lobby.getGameMode().size());
    memcpy(buffer + sizeof(Header), &lobbyHeader, sizeof(LobbyHeader));
    switch (type) {
        case LOBBY_TYPE::INVITE:
            header.sizeMessage =
                sizeof(LobbyHeader) + sizeof(LobbyInviteFriend);
        case LOBBY_TYPE::CREATE:
        case LOBBY_TYPE::MODIFY:
        case LOBBY_TYPE::JOIN:
        case LOBBY_TYPE::LEAVE:
        case LOBBY_TYPE::START:
        default:
            break;
    }
    memcpy(buffer, &header, sizeof(Header));
}

void Message::serializeSocialGetLobbyInvite(SOCIAL_TYPE type,
                                            std::shared_ptr<Player> player,
                                            char* buffer) {
    Header header;
    header.type = MESSAGE_TYPE::SOCIAL;
    header.idInstance = player->getPlayerId();
    header.sizeMessage = sizeof(SocialHeader);
    memcpy(buffer, &header, sizeof(Header));
    SocialHeader socialHeader;
    socialHeader.type = type;
    memcpy(buffer + sizeof(Header), &socialHeader, sizeof(SocialHeader));
}

/*
 * LOBBY MESSAGE
 */

LobbyMessage::LobbyMessage(std::shared_ptr<Server> server) : server_(server) {}

void LobbyMessage::handleWaitingRoom(Lobby& lobby, bool& running) {
    char buffer[BUFFER_SIZE];
    server_->receiveMessage(buffer);
    HeaderResponse response;
    memcpy(&response, buffer, sizeof(HeaderResponse));
    LobbyResponseHeader responseHeader;
    memcpy(&responseHeader, buffer + sizeof(HeaderResponse),
           sizeof(LobbyResponseHeader));
    if (responseHeader.responseType == LOBBY_RESPONSE::END) {
        running = false;
    }
    if (responseHeader.responseType == LOBBY_RESPONSE::UPDATE) {
        handleUPDATE(lobby, buffer);
    }
    if (responseHeader.responseType == LOBBY_RESPONSE::UPDATE_PLAYER) {
        handleUPDATE_PLAYER(lobby, buffer);
    }
    if (responseHeader.responseType == LOBBY_RESPONSE::STARTED) {
        lobby.startGame();
    }
    if (responseHeader.responseType == LOBBY_RESPONSE::ERROR) {
        handleError(lobby, buffer);
    }
}

void LobbyMessage::handleUPDATE(Lobby& lobby, char* buffer) {
    LobbyUpdate lobbyUpdate;
    memcpy(&lobbyUpdate,
           buffer + sizeof(HeaderResponse) + sizeof(LobbyResponseHeader),
           sizeof(LobbyUpdate));
    lobby.setGameMode(std::string(lobbyUpdate.gameMode));
    int nbrPlayer = static_cast<int>(lobbyUpdate.nbGamerMax);
    lobby.setNumberOfPlayer(nbrPlayer);
}

void LobbyMessage::handleUPDATE_PLAYER(Lobby& lobby, char* buffer) {
    LobbyUpdatePlayer lobbyUpdatePlayer;
    memcpy(&lobbyUpdatePlayer,
           buffer + sizeof(HeaderResponse) + sizeof(LobbyResponseHeader),
           sizeof(LobbyUpdatePlayer));
    for (int i = 0; i < lobbyUpdatePlayer.nbPlayers; i++) {
        LobbyUpdatePlayerList lUPL;
        memcpy(&lUPL,
               buffer + sizeof(HeaderResponse) + sizeof(LobbyResponseHeader) +
                   sizeof(LobbyUpdatePlayer) +
                   (sizeof(LobbyUpdatePlayerList) * i),
               sizeof(LobbyUpdatePlayerList));
        if (lUPL.added) {
            lobby.addPlayer(lUPL.idPlayer, lUPL.name, lUPL.asGamer);
        } else {
            lobby.removePlayer(lUPL.idPlayer, lUPL.asGamer);
        }
    }
}

void LobbyMessage::handleError(Lobby& lobby, char* buffer) {
    LobbyErrorResponse lobbyErrorResponse;
    memcpy(&lobbyErrorResponse,
           buffer + sizeof(HeaderResponse) + sizeof(LobbyResponseHeader),
           sizeof(LobbyErrorResponse));
    char message[MAX_LENGTH_MESSAGES];
    memcpy(message,
           buffer + sizeof(HeaderResponse) + sizeof(LobbyResponseHeader) +
               sizeof(LobbyErrorResponse),
           lobbyErrorResponse.sizeMessage);
    lobby.getView()->setErrorMessage(
        std::string(message, lobbyErrorResponse.sizeMessage));
}
