#ifndef __TETRIS_H
#define __TETRIS_H

#include <memory>

#include "../../common/CONSTANT.h"
#include "Signal.h"

class Server;
class Player;
class IView;
class IController;
class Lobby;
class SocialController;
class SocialView;

class Tetris {
   private:
    Signal& signal_;
    std::shared_ptr<Server> server_;
    std::shared_ptr<Player> player_;
    std::shared_ptr<IView> view_;
    std::shared_ptr<IController> controller_;
    std::shared_ptr<Lobby> lobby_;
    std::shared_ptr<SocialController> socialController_;
    std::shared_ptr<SocialView> socialView_;
    MENU_STATE menuState_;

   public:
    Tetris();
    ~Tetris();
    bool run();

    void init();

    // Getters et setters pour menuState_ et GUI_
    void setMenuState(MENU_STATE menu) { menuState_ = menu; }
    MENU_STATE getMenuState() const { return menuState_; }
    std::shared_ptr<Lobby> getLobby() { return lobby_; }
    std::shared_ptr<Player> getPlayer() { return player_; }
    std::shared_ptr<Server> getServer() { return server_; }
};
#endif
