#include "InvitationManager.h"

#include "Message.h"
#include "Player.h"
#include "Server.h"

InvitationManager::InvitationManager(std::shared_ptr<Player> player)
    : player_(player) {}

void InvitationManager::refreshInvitationsList(std::shared_ptr<Server> server_,
                                               FriendHeader player) {
    server_->handleSocialRequest(SOCIAL_TYPE::GET_FRIEND_REQUEST, player);
    std::vector<FriendHeader> requestList = server_->receiveFriendListHeader();
    player_->setVectorInvitations(requestList);
}

bool InvitationManager::acceptInvitation(int invitationId) {
    std::vector<FriendHeader> invitations = player_->getVectorInvitations();
    if (invitations.empty()) return false;

    for (auto it = invitations.begin(); it != invitations.end(); ++it) {
        if (it->idPlayer == invitationId) {
            invitations.erase(it);
            break;
        }
    }
    player_->setVectorInvitations(invitations);
    return true;
}

bool InvitationManager::hasInvitations() const {
    return !player_->getVectorInvitations().empty();
}

std::vector<LobbyInvitation> InvitationManager::getLobbyInvitations() {
    std::shared_ptr<Server> server = player_->getServer();
    Message message;
    // MESSAGE_TYPE::SOCIAL;
    char buffer[BUFFER_SIZE];
    message.serializeSocialGetLobbyInvite(SOCIAL_TYPE::GET_LOBBY_INVITE,
                                          player_, buffer);
    server->sendMessage(buffer, sizeof(Header) + sizeof(SocialHeader));
    std::vector<LobbyInvitation> lobbyInvitations;

    bool done = false;
    while (!done) {
        // reponse := INT + LobbyInvitation * INT
        server->receiveMessage(buffer);

        SocialResponseLobbyInvite response;
        memcpy(&response, buffer + sizeof(HeaderResponse),
               sizeof(SocialResponseLobbyInvite));
        size_t baseSize =
            sizeof(HeaderResponse) + sizeof(SocialResponseLobbyInvite);
        size_t size = response.numberOfInvitations;
        done = response.completed;

        for (size_t i = 0; i < size; i++) {
            LobbyInvitation lobbyInvite;
            memcpy(&lobbyInvite,
                   buffer + baseSize + (sizeof(LobbyInvitation) * i),
                   sizeof(LobbyInvitation));
            lobbyInvitations.push_back(lobbyInvite);
        }
    }
    return lobbyInvitations;
}
