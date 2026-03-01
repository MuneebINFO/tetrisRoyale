#ifndef __SOCIALVIEW_H
#define __SOCIALVIEW_H

#include "../common/header.h"
#include "Lobby.h"
#include "Social.h"
#include "Tetris.h"


class Tetris;
class Lobby;
class Player;
class ViewCLI;
class Signal;

class SocialView {
   private:
    Signal& signal_;
    FRIEND_REQUEST_STATUS friendRequestStatus_;
    std::string targetPlayerName_ = "";
    std::shared_ptr<ViewCLI> viewCLI_;

   public:
    SocialView();
    ~SocialView() = default;
    void showFriendList(std::vector<PlayerHeader> friendsList, int& idx);
    void showInvitationMenu(std::shared_ptr<Player> player__);
    void showInvitationList(std::vector<PlayerHeader> invitations, int& idx);
    void showProfileMenu(std::shared_ptr<Player> player);
    void setFriendRequestStatus(FRIEND_REQUEST_STATUS status,
                                const std::string& playerName = "") {
        friendRequestStatus_ = status;
        if (!playerName.empty()) targetPlayerName_ = playerName;
    }

    void setViewCLI(std::shared_ptr<ViewCLI> viewCLI) { viewCLI_ = viewCLI; }
    void clearScreen();
};

#endif
