#ifndef __SOCIAL_H
#define __SOCIAL_H

#include <sys/socket.h>

#include <cstring>
#include <memory>

#include "../../common/CONSTANT.h"
#include "../../common/header.h"
#include "../view/SocialView.h"
#include "../view/ViewCLI.h"
#include "Player.h"
#include "Server.h"
#include "Tetris.h"

class Tetris;
class Player;
class ViewCLI;
class Server;
class SocialView;

class Social {
   private:
    PlayerHeader player_;

   public:
    Social() = default;
    ~Social() = default;
    std::vector<PlayerHeader> handleResponsesSocialRequest(
        std::shared_ptr<Server> server_, SOCIAL_TYPE expectedType);
    void refreshFriendList(std::shared_ptr<Server> server_,
                           std::shared_ptr<Player> player_,
                           const PlayerHeader& player);
    void refreshFriendRequest(std::shared_ptr<Server> server_,
                              std::shared_ptr<Player> player_,
                              const PlayerHeader& playerNameStr);
    void sendFriendRequestTerminalUI(std::shared_ptr<Server> server_,
                                     std::shared_ptr<SocialView> view_,
                                     PlayerHeader& playerNameStr);
    bool acceptFriendRequest(std::shared_ptr<Player> player_,
                             std::shared_ptr<Server> server_,
                             PlayerHeader& invitingPlayer);
    bool sendFriendRequestGUI(PlayerHeader& invitedPlayer, std::shared_ptr<Server> server_);

    void removeFriend(std::shared_ptr<Server> server_,
                      std::shared_ptr<Player> player_,
                      const PlayerHeader& playerNameStr);
    std::vector<PlayerHeader> getUsers(std::shared_ptr<Server> server_,
                                       const PlayerHeader& player);
};

#endif
