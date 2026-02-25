#ifndef __SOCIALCONTROLLER_H
#define __SOCIALCONTROLLER_H

#include "Tetris.h"

class Tetris;
class SocialView;
class Player;
class Server;

class SocialController {
   private:
    std::shared_ptr<Player> player_;
    std::shared_ptr<Server> server_;
    // std::shared_ptr<ChatModel> chatModel_;
    // std::shared_ptr<ChatController> chatController_;
    std::shared_ptr<SocialView> socialView_;
    Signal& signal_;

   public:
    SocialController();
    ~SocialController() = default;
    void captureInputProfileMenu(Tetris& tetris);
    void captureInputInvitationMenu(Tetris& tetris);
    void setPlayer(std::shared_ptr<Player> player) { player_ = player; }
    void setServer(std::shared_ptr<Server> server) { server_ = server; }
    void setSocialView(std::shared_ptr<SocialView> socialView) {
        socialView_ = socialView;
    }
};

#endif
