#include "SocialController.h"

#include "SocialView.h"  // Include the full definition of SocialView

SocialController::SocialController() : signal_{Signal::getInstance()} {}
//   chatModel_{std::make_shared<ChatModel>()},
//   chatController_{std::make_shared<ChatController>()},

void SocialController::captureInputProfileMenu(Tetris& tetris) {
    int key;
    bool done = false;
    bool friendSelected = false;
    int index = 0;
    PlayerHeader invitedPlayer;
    PlayerHeader you = player_->getPlayer();
    Social social;
    while (!done and signal_.getSigIntFlag() == 0) {
        key = getchar();
        switch (key) {
            case 'a': {
                socialView_->clearScreen();
                social.sendFriendRequestTerminalUI(server_, socialView_,
                                                   invitedPlayer);
                done = true;
            } break;

            case 'r': {
                socialView_->setFriendRequestStatus(FRIEND_REQUEST_STATUS::NONE);
                social.refreshFriendList(server_, player_, you);
                done = true;
            } break;

            case 'k': {
                friendSelected = true;
                socialView_->showFriendList(player_->getVectorFriends(), index);
                if (player_->getVectorFriends().size() > 0)
                    index =
                        (index + 1) %
                        static_cast<int>(player_->getVectorFriends().size());
            } break;

            case '\r': {
                std::vector<PlayerHeader> friendsList =
                    player_->getVectorFriends();
                if (friendSelected) {
                    if (index > 0) {
                        player_->setFriendSelected(friendsList[index - 1]);
                    } else {
                        player_->setFriendSelected(
                            friendsList[friendsList.size() - 1]);
                    }
                }
                // social.joiningChatRoom(server_, player_, tetris, you);
                friendSelected = false;
                done = true;
            } break;

            case 'd': {
                if (friendSelected) {
                    if (index > 0) {
                        player_->setFriendSelected(
                            player_->getVectorFriends()[index - 1]);
                    } else {
                        player_->setFriendSelected(
                            player_->getVectorFriends()
                                [player_->getVectorFriends().size() - 1]);
                    }
                }
                social.removeFriend(server_, player_,
                                    player_->getFriendSelected());
                friendSelected = false;
                done = true;
            } break;

            case ESCAPE: {
                socialView_->setFriendRequestStatus(FRIEND_REQUEST_STATUS::NONE);
                tetris.setMenuState(MENU_STATE::MAIN);
                done = true;
            } break;
            default:
                break;
        }
    }
}

void SocialController::captureInputInvitationMenu(Tetris& tetris) {
    int key;
    bool done = false;
    int index = 0;
    bool requestSelected = false;
    Social social;
    std::vector<PlayerHeader> invitations = player_->getVectorInvitations();
    PlayerHeader you = player_->getPlayer();
    while (!done and signal_.getSigIntFlag() == 0) {
        key = getchar();
        switch (key) {
            case 'a': {
                if (static_cast<int>(invitations.size()) > 0) {
                    requestSelected = true;
                    socialView_->showInvitationList(invitations, index);
                    index = (index + 1) % int(invitations.size());
                }

            } break;
            case 'r': {
                social.refreshFriendRequest(server_, player_, you);
                done = true;
            } break;
            case '\r': {
                if (requestSelected) {
                    if (index > 0) {
                        social.acceptFriendRequest(player_, server_,
                                                   invitations[index - 1]);

                    } else {
                        social.acceptFriendRequest(
                            player_, server_,
                            invitations[invitations.size() - 1]);
                    }
                    requestSelected = false;
                    index = 0;
                }
                done = true;
            } break;
            case ESCAPE:
                tetris.setMenuState(MENU_STATE::MAIN);
                requestSelected = false;
                index = 0;
                done = true;
                break;
            default:
                break;
        }
    }
}
