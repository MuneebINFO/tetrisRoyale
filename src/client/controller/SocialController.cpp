#include "SocialController.h"
#include "../view/SocialView.h"

namespace {
    // Constants
    constexpr char KEY_ADD_FRIEND = 'a';
    constexpr char KEY_TO_REFRESH = 'r';
    constexpr char KEY_SELECT_NEXT = 'k';
    constexpr char KEY_TO_CONFIRM = '\r';
    constexpr char KEY_DELETE = 'd';
}

SocialController::SocialController() : signal_{Signal::getInstance()} {}

void SocialController::captureInputProfileMenu(Tetris& tetris) {
    int key;
    bool done = false;
    bool friendSelected = false;
    int selectedIndex = 0;
    PlayerHeader invitedPlayer;
    PlayerHeader currentUser = player_->getPlayer();
    Social socialManager;

    // Reset the selected friend when entering profile menu
    PlayerHeader emptyHeader = {};
    player_->setFriendSelected(emptyHeader);
    while (!done && signal_.getSigIntFlag() == 0) {
        key = getchar();
        
        switch (key) {
            case KEY_ADD_FRIEND:
                handleAddFriend(socialManager, invitedPlayer);
                done = true;
                break;
                
            case KEY_TO_REFRESH:
                handleRefreshFriends(socialManager, currentUser);
                done = true;
                break;
                
            case KEY_SELECT_NEXT:
                handleFriendSelection(friendSelected, selectedIndex);
                break;
                
            case KEY_TO_CONFIRM:
                handleOpenChat(friendSelected, selectedIndex, socialManager, tetris, currentUser);
                friendSelected = false;
                done = true;
                break;
                
            case KEY_DELETE:
                handleDeleteFriend(friendSelected, selectedIndex, socialManager);
                friendSelected = false;
                done = true;
                break;
                
            case ESCAPE:
                handleReturnToMainMenu(tetris);
                done = true;
                break;
                
            default:
                break;
        }
    }
}

void SocialController::handleAddFriend(Social& socialManager, PlayerHeader& invitedPlayer) {
    socialView_->clearScreen();
    socialManager.sendFriendRequestTerminalUI(server_, socialView_, invitedPlayer);
}

void SocialController::handleRefreshFriends(Social& socialManager, const PlayerHeader& currentUser) {
    socialView_->setFriendRequestStatus(FRIEND_REQUEST_STATUS::NONE);
    socialManager.refreshFriendList(server_, player_, currentUser);
}

void SocialController::handleFriendSelection(bool& friendSelected, int& selectedIndex) {
    friendSelected = true;
    socialView_->showFriendList(player_->getVectorFriends(), selectedIndex);
    
    if (!player_->getVectorFriends().empty()) {
        selectedIndex = (selectedIndex + 1) % static_cast<int>(player_->getVectorFriends().size());
    }
}

void SocialController::handleOpenChat(
    bool friendSelected, 
    int selectedIndex, 
    Social& socialManager, 
    Tetris& tetris, 
    PlayerHeader& currentUser) {
    
    if (!friendSelected) {
        return;
        
    }
    selectFriendByIndex(selectedIndex);
    socialManager.joiningChatRoom(server_, player_, tetris, currentUser);
}

void SocialController::selectFriendByIndex(int selectedIndex) {
    std::vector<PlayerHeader> friendsList = player_->getVectorFriends();
    
    if (!friendsList.empty()) {
        if (selectedIndex > 0) {
            player_->setFriendSelected(friendsList[selectedIndex - 1]);
        } else {
            player_->setFriendSelected(friendsList[friendsList.size() - 1]);
        }
    }
}

void SocialController::handleDeleteFriend(
    bool friendSelected, 
    int selectedIndex, 
    Social& socialManager) {
    
    if (friendSelected) {
        selectFriendByIndex(selectedIndex);
    }
    
    socialManager.removeFriend(server_, player_, player_->getFriendSelected());
}

void SocialController::handleReturnToMainMenu(Tetris& tetris) {
    socialView_->setFriendRequestStatus(FRIEND_REQUEST_STATUS::NONE);
    tetris.setMenuState(MENU_STATE::MAIN);
}

void SocialController::captureInputInvitationMenu(Tetris& tetris) {
    int key;
    bool done = false;
    int selectedIndex = 0;
    bool requestSelected = false;
    Social socialManager;
    std::vector<PlayerHeader> invitations = player_->getVectorInvitations();
    PlayerHeader currentUser = player_->getPlayer();
    
    while (!done && signal_.getSigIntFlag() == 0) {
        key = getchar();
        
        switch (key) {
            case KEY_ADD_FRIEND:
                handleSelectInvitation(invitations, requestSelected, selectedIndex);
                break;
                
            case KEY_TO_REFRESH:
                handleRefreshInvitations(socialManager, currentUser);
                done = true;
                break;
                
            case KEY_TO_CONFIRM:
                handleAcceptInvitation(requestSelected, selectedIndex, socialManager, invitations);
                requestSelected = false;
                selectedIndex = 0;
                done = true;
                break;
                
            case ESCAPE:
                handleReturnToMainMenu(tetris);
                requestSelected = false;
                selectedIndex = 0;
                done = true;
                break;
                
            default:
                break;
        }
    }
}

void SocialController::handleSelectInvitation(
    const std::vector<PlayerHeader>& invitations,
    bool& requestSelected,
    int& selectedIndex) {
    
    if (!invitations.empty()) {
        requestSelected = true;
        socialView_->showInvitationList(invitations, selectedIndex);
        selectedIndex = (selectedIndex + 1) % static_cast<int>(invitations.size());
    }
}

void SocialController::handleRefreshInvitations(Social& socialManager, const PlayerHeader& currentUser) {
    socialManager.refreshFriendRequest(server_, player_, currentUser);
}

void SocialController::handleAcceptInvitation(
    bool requestSelected,
    int selectedIndex,
    Social& socialManager,
    std::vector<PlayerHeader>& invitations) {
    
    if (requestSelected && !invitations.empty()) {
        if (selectedIndex > 0) {
            socialManager.acceptFriendRequest(player_, server_, invitations[selectedIndex - 1]);
        } else {
            socialManager.acceptFriendRequest(player_, server_, invitations[invitations.size() - 1]);
        }
    }
}