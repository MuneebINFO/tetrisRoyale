#ifndef __HEADER_H
#define __HEADER_H

#include <cstdint>
#include <iostream>
#include <string>

#include "CONSTANT.h"

enum class MESSAGE_TYPE {
    ACCOUNT,
    GAME,
    LOBBY,
    CHAT,
    SYSTEM,
    SOCIAL,
    INVITEFRIEND,
    FRIEND,
    ACCEPTFRIEND,
    FRIENDLIST,
    REQUESTFRIENDLIST
};
enum class FRIEND_REQUEST_STATUS {
    NONE,
    PLAYER_NOT_FOUND,
    FRIEND_REQUEST_SENT,
    ALREADY_IN_LIST,
    SELF_ADD_FORBIDDEN,
    FRIEND_REQUEST_ALREADY_SENT,
};

enum class ACCOUNT_TYPE { LOGIN, REGISTER, LOGOUT };
enum class LOBBY_TYPE { CREATE, MODIFY, JOIN, LEAVE, START, INVITE };
enum class SOCIAL_TYPE {
    SEND_MESSAGE,
    GET_MESSAGE,
    GET_FRIEND_REQUEST,
    GET_FRIEND_LIST,
    INVITE,
    ACCEPT,
    GET_LOBBY_INVITE,
    INCHATROOM,
    LEAVE_CHAT,
};

enum class GAME_TYPE {
    MOVE,
    ROTATE,
    MALUS,
    BONUS,
    START,
    END,
    SPAWN_TETRAMINO,
    LOCK,
    MALUS_CONFIRM,
    UPDATEGRID,
    WIN,
    LOST,
    TETRAMINO_REQUEST,
    MALUS_AUTHORISATION,
    BONUS_AUTHORISATION
};

struct Header {
    MESSAGE_TYPE type;
    uint32_t idInstance;
    uint32_t sizeMessage;
};

struct AccountHeader {
    ACCOUNT_TYPE type;
    char username[MAX_NAME_LENGTH];
    char password[MAX_NAME_LENGTH];
};

struct FriendHeader {
    char username[MAX_NAME_LENGTH];
    int idPlayer;
    char message[MAX_LENGTH_MESSAGES]; // pr inclure un message txt (chat)
    char receiver[MAX_NAME_LENGTH]; // pr inclure un message txt (chat)
};

struct ChatHeader {
    char username[MAX_NAME_LENGTH];
    int idPlayer;
    char message[MAX_LENGTH_MESSAGES];  // pr inclure un message txt (chat)
};

struct LobbyHeader {
    LOBBY_TYPE type;
    uint8_t nbPlayers;
    int idRoom;
    char gameMode[MAX_NAME_LENGTH];
};

#pragma pack(push, 1)
// Should be followed by LobbyInvitation * numberOfInvitations
struct LobbyInviteFriend {
    int idRoom;
    char nameInviting[MAX_NAME_LENGTH];
    char gameMode[MAX_NAME_LENGTH];
    bool asGamer;
};
#pragma pack(pop)

struct LobbyInvitation {
    char invitingPlayer[MAX_NAME_LENGTH];
    int idPlayerInviting;
    int roomId;
    char gameMode[MAX_NAME_LENGTH];
    bool asGamer;
};

struct SocialHeader {
    SOCIAL_TYPE type;
    uint32_t idPlayer;
};

struct GameTypeHeader {
    GAME_TYPE type;
};

struct MovementPayload {
    int dx;
    int dy;
};

struct RotationPayload {
    bool clockwise;
};

struct MalusPayload {
    int malusType;
    int targetSocket;
    int target;
    int details;  // si type == 1
    int emptyIdx; // si type == 1
};

struct BonusPayload {
    int type;
    int details;
};

struct GameUpdateHeader {
    GAME_TYPE type;
    uint8_t x;
    uint8_t y;
    uint32_t score;
    bool isWinner;
    bool rotation;
    uint8_t linesCleared;  // nb de lignes supprimées
    uint8_t rows[4];       // indices des lignes supprimées
};

struct GameEndHeader {
    GAME_TYPE type;
    uint32_t score;
    bool isWinner;
};

struct SpawnTetraminoPayload {
    uint8_t shape[4][4];  // max 4x4 pour toutes les pieces
    uint8_t width;
    uint8_t height;
    uint8_t x;
    uint8_t y;
    uint8_t color;

};

struct GridUpdate {
    int playerID;
    char username[32];
    uint8_t grid[HEIGHT][WIDTH];
};

/*
 * RESPONSE OF THE SERVER TO THE CLIENT
 */
enum class ACCOUNT_RESPONSE { VALID, EXIST, ERROR };
enum class LOBBY_RESPONSE {
    CREATED,
    STARTED,
    ERROR,
    END,
    UPDATE,
    UPDATE_PLAYER,
    INVITE_RECEIVED
};
enum class SYSTEM_TYPE { ERROR, SUCCESS };

struct AccountResponseHeader {
    ACCOUNT_RESPONSE responseType;
    int idPlayer;
    char message[MAX_LENGTH_MESSAGES];  // pr inclure un message txt (mais
                                        // optionnel)
};
struct HeaderResponse {
    MESSAGE_TYPE type;
    uint32_t sizeMessage;
    FRIEND_REQUEST_STATUS status;
};

struct LobbyResponseHeader {
    LOBBY_RESPONSE responseType;
    int idRoom;
};

struct LobbyErrorResponse {
    uint32_t sizeMessage;
};

struct LobbyUpdate {
    uint8_t nbGamerMax;
    char gameMode[MAX_NAME_LENGTH];
};

struct LobbyUpdatePlayer {
    int nbPlayers;
    bool completed;
};

struct LobbyUpdatePlayerList {
    bool added;
    int idPlayer;
    char name[MAX_NAME_LENGTH];
    bool asGamer;
};

struct SocialResponseLobbyInvite {
    int numberOfInvitations;
    bool completed;
};

struct SystemHeader {
    SYSTEM_TYPE type;
    uint32_t sizeMessage;
};

#endif
