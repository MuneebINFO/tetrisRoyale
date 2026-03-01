#include "Social.h"

void Social::refreshFriendList(std::shared_ptr<Server> server_,
                               std::shared_ptr<Player> player_,
                               const PlayerHeader& player) {
    server_->handleSocialRequest(SOCIAL_TYPE::GET_FRIEND_LIST, player);
    std::vector<PlayerHeader> friendList =
        handleResponsesSocialRequest(server_, SOCIAL_TYPE::GET_FRIEND_LIST);
    player_->setVectorFriends(friendList);
}

void Social::refreshFriendRequest(std::shared_ptr<Server> server_,
                                  std::shared_ptr<Player> player_,
                                  const PlayerHeader& player) {
    server_->handleSocialRequest(SOCIAL_TYPE::GET_FRIEND_REQUEST, player);
    std::vector<PlayerHeader> requestList =
        handleResponsesSocialRequest(server_, SOCIAL_TYPE::GET_FRIEND_REQUEST);
    player_->setVectorInvitations(requestList);
}

void handleFriendRequestStatus(const PlayerHeader& invitedPlayer,
                               std::shared_ptr<SocialView> view_) {
    switch (invitedPlayer.status) {
        case FRIEND_REQUEST_STATUS::FRIEND_REQUEST_SENT:
            view_->setFriendRequestStatus(
                FRIEND_REQUEST_STATUS::FRIEND_REQUEST_SENT,
                invitedPlayer.username);
            break;

        case FRIEND_REQUEST_STATUS::PLAYER_NOT_FOUND:
            view_->setFriendRequestStatus(
                FRIEND_REQUEST_STATUS::PLAYER_NOT_FOUND,
                invitedPlayer.username);
            break;

        case FRIEND_REQUEST_STATUS::ALREADY_IN_LIST:
            view_->setFriendRequestStatus(
                FRIEND_REQUEST_STATUS::ALREADY_IN_LIST, invitedPlayer.username);
            break;

        case FRIEND_REQUEST_STATUS::SELF_ADD_FORBIDDEN:
            view_->setFriendRequestStatus(
                FRIEND_REQUEST_STATUS::SELF_ADD_FORBIDDEN,
                invitedPlayer.username);
            break;

        case FRIEND_REQUEST_STATUS::FRIEND_REQUEST_ALREADY_SENT:
            view_->setFriendRequestStatus(
                FRIEND_REQUEST_STATUS::FRIEND_REQUEST_ALREADY_SENT,
                invitedPlayer.username);
            break;

        case FRIEND_REQUEST_STATUS::NONE:
            view_->setFriendRequestStatus(FRIEND_REQUEST_STATUS::NONE,
                                          invitedPlayer.username);
            break;

        default:
            break;
    }
}

void Social::sendFriendRequestTerminalUI(std::shared_ptr<Server> server_,
                                         std::shared_ptr<SocialView> view_,
                                         PlayerHeader& invitedPlayer) {
    mvprintw(0, 1, "Enter the player's name to add as a friend");
    echo();
    char playerName[MAX_NAME_LENGTH];
    mvgetstr(1, 1, playerName);
    strcpy(invitedPlayer.username, playerName);
    server_->handleSocialRequest(SOCIAL_TYPE::INVITE, invitedPlayer);

    char response[sizeof(HeaderResponse) + sizeof(PlayerHeader)];
    HeaderResponse responseHeader;
    PlayerHeader invitedPlayerResponse;
    int bytesRead = server_->receiveMessage(response);
    if (bytesRead > 0) {
        memcpy(&responseHeader, response, sizeof(HeaderResponse));
        memcpy(&invitedPlayerResponse, response + sizeof(HeaderResponse),
               sizeof(PlayerHeader));

        if (responseHeader.type == MESSAGE_TYPE::SOCIAL) {
            handleFriendRequestStatus(invitedPlayerResponse, view_);
        } else {
            std::cerr << "[CLIENT] Error: Invalid response type" << std::endl;
        }
    } else {
        std::cerr << "[CLIENT] Error: Failed to receive response" << std::endl;
    }
}


void Social::removeFriend(std::shared_ptr<Server> server_,
                          std::shared_ptr<Player> player_,
                          const PlayerHeader& playerNameStr) {
    (void)player_;
    server_->handleSocialRequest(SOCIAL_TYPE::REMOVE_FRIEND, playerNameStr);
}

std::vector<PlayerHeader> Social::handleResponsesSocialRequest(
    std::shared_ptr<Server> server_, SOCIAL_TYPE expectedType) {
    std::vector<PlayerHeader> responseHeaders;
    char countBuffer[sizeof(int) + sizeof(HeaderResponse)];
    int count;
    memset(countBuffer, 0, sizeof(countBuffer));
    server_->receiveMessage(countBuffer);
    memcpy(&count, countBuffer + sizeof(HeaderResponse), sizeof(int));

    for (int i = 0; i < count; i++) {
        char buffer[sizeof(PlayerHeader) + sizeof(HeaderResponse) +
                    sizeof(SocialHeader)];
        memset(buffer, 0, sizeof(buffer));
        server_->receiveMessage(buffer);
        SocialHeader socialHeader;
        memcpy(&socialHeader, buffer + sizeof(HeaderResponse),
               sizeof(SocialHeader));
        if (socialHeader.type == expectedType) {
            PlayerHeader header;
            memcpy(&header,
                   buffer + sizeof(HeaderResponse) + sizeof(SocialHeader),
                   sizeof(PlayerHeader));
            responseHeaders.push_back(header);
        } else {
            std::cerr << "[CLIENT] Error: Invalid response type" << std::endl;
        }
    }
    return responseHeaders;
}

std::vector<PlayerHeader> Social::getUsers(std::shared_ptr<Server> server_,
                                           const PlayerHeader& player) {
    server_->handleSocialRequest(SOCIAL_TYPE::GET_USERS, player);
    std::vector<PlayerHeader> users =
        handleResponsesSocialRequest(server_, SOCIAL_TYPE::GET_USERS);
    return users;
}

bool Social::acceptFriendRequest(std::shared_ptr<Player> player_,
                                 std::shared_ptr<Server> server_,
                                 PlayerHeader& invitingPlayer) {
    std::vector<PlayerHeader> invitations = player_->getVectorInvitations();
    if (invitations.empty()) return false;

    for (auto it = invitations.begin(); it != invitations.end(); ++it) {
        if (it->idPlayer == invitingPlayer.idPlayer) {
            invitations.erase(it);
            break;
        } else {
            std::cerr << "[CLIENT] Error: Invitation ID not found" << std::endl;
            return false;
        }
    }
    player_->setVectorInvitations(invitations);
    server_->handleSocialRequest(SOCIAL_TYPE::ACCEPT, invitingPlayer);
    return true;
}

bool Social::sendFriendRequestGUI(PlayerHeader& invitedPlayer, std::shared_ptr<Server> server_) {
    server_->handleSocialRequest(SOCIAL_TYPE::INVITE, invitedPlayer);

    char response[sizeof(HeaderResponse) + sizeof(PlayerHeader)];
    HeaderResponse responseHeader;
    PlayerHeader invitedPlayerResponse;
    int bytesRead = server_->receiveMessage(response);
    if (bytesRead > 0) {
        memcpy(&responseHeader, response, sizeof(HeaderResponse));
        memcpy(&invitedPlayerResponse, response + sizeof(HeaderResponse),
               sizeof(PlayerHeader));

        // Return true only if a request was successfully sent
        return (responseHeader.type == MESSAGE_TYPE::SOCIAL &&
                invitedPlayerResponse.status == FRIEND_REQUEST_STATUS::FRIEND_REQUEST_SENT);
    }
    return false;
}
