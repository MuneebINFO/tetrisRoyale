#ifndef __INVITATION_MANAGER_H
#define __INVITATION_MANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "../common/CONSTANT.h"
#include "../common/header.h"

class Player;
class Server;

class InvitationManager {
   private:
    std::shared_ptr<Player> player_;

   public:
    InvitationManager(std::shared_ptr<Player> player);

    // Core functionality
    std::vector<LobbyInvitation> getLobbyInvitations();

};

#endif
