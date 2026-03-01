#ifndef __TETRIS_H
#define __TETRIS_H

#include <memory>

#include "../common/CONSTANT.h"
#include "Signal.h"


class Server;
class Player;
class IView;
class IController;
class Lobby;

class Tetris {
   private:
    Signal& signal_;
    std::shared_ptr<Server> server_;
    std::shared_ptr<Player> player_;
    std::shared_ptr<IView> view_;
    std::shared_ptr<IController> controller_;
    std::shared_ptr<Lobby> lobby_;
    MENU_STATE menuState_;

   public:
    Tetris(bool gui = false);
    ~Tetris();
    void run();

    // Getters et setters pour menuState_ et GUI_
    void setMenuState(MENU_STATE menu) { menuState_ = menu; }
    void addFriend(std::string friendName);
    MENU_STATE getMenuState() const { return menuState_; }
    std::shared_ptr<Lobby> getLobby() { return lobby_; }
    std::shared_ptr<Player> getPlayer() { return player_; }
    std::shared_ptr<Server> getServer() { return server_; }
};
#endif
