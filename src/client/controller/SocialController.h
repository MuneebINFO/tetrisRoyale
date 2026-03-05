#ifndef __SOCIALCONTROLLER_H
#define __SOCIALCONTROLLER_H

#include "../model/Tetris.h"

class Tetris;
class SocialView;
class Player;
class Server;
class Social;
struct PlayerHeader;

/*
    SocialController is responsible for managing the social interactions
    between players, including adding friends, refreshing friend lists,
    handling invitations, and managing chat rooms. It interacts with the
    SocialView to display information to the user and with the Player
    and Server classes to manage the underlying data and network
*/


class SocialController {
   private:
    std::shared_ptr<Player> player_;
    std::shared_ptr<Server> server_;
    std::shared_ptr<SocialView> socialView_;
    Signal& signal_;

   public:
    SocialController();
    ~SocialController() = default;
    void handleAddFriend(Social& socialManager, PlayerHeader& invitedPlayer);
    void handleRefreshFriends(Social& socialManager, const PlayerHeader& currentUser);
    void handleFriendSelection(bool& friendSelected, int& selectedIndex);
    void handleOpenChat(bool friendSelected, int selectedIndex, Social& socialManager, 
                       Tetris& tetris, PlayerHeader& currentUser);
    void selectFriendByIndex(int selectedIndex);
    void handleDeleteFriend(bool friendSelected, int selectedIndex, Social& socialManager);
    void handleReturnToMainMenu(Tetris& tetris);
    
    void handleSelectInvitation(const std::vector<PlayerHeader>& invitations, 
                               bool& requestSelected, int& selectedIndex);
    void handleRefreshInvitations(Social& socialManager, const PlayerHeader& currentUser);
    void handleAcceptInvitation(bool requestSelected, int selectedIndex, Social& socialManager, 
                                std::vector<PlayerHeader>& invitations);
    void captureInputProfileMenu(Tetris& tetris);
    void captureInputInvitationMenu(Tetris& tetris);
    void setPlayer(std::shared_ptr<Player> player) { player_ = player; }
    void setServer(std::shared_ptr<Server> server) { server_ = server; }
    void setSocialView(std::shared_ptr<SocialView> socialView) {
        socialView_ = socialView;
    }
};

#endif
