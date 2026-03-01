#include "Tetris.h"

#include <ncurses.h>

#include <cstdio>

#include "ControllerCLI.h"
#include "InvitationManager.h"
#include "Lobby.h"
#include "Player.h"
#include "Server.h"
#include "Signal.h"
#include "ViewCLI.h"


Tetris::Tetris(bool gui)
    : signal_{Signal::getInstance()}, menuState_{MENU_STATE::MAIN} {
    
    view_ = std::make_shared<ViewCLI>();
    controller_ = std::make_shared<ControllerCLI>(view_);

    server_ = std::make_shared<Server>();
    player_ = std::make_shared<Player>(view_, controller_, server_);
    lobby_ = std::make_shared<Lobby>(*this, controller_, player_, view_, server_);
    view_->setLobby(lobby_);
    view_->setPlayer(player_);
    controller_->setLobby(lobby_);
    controller_->setServer(server_);
    controller_->setPlayer(player_);
    server_->setPlayer(player_);
}

Tetris::~Tetris() {
    endwin();
}


void Tetris::run() {
    if (server_->connectToServer() != 0) {
        return;
    }
    if (!player_->login(this)) return;

    bool running = true;
    MENU_STATE menu;

    while (running and signal_.getSigIntFlag() == 0) {
        menu = getMenuState();
        switch (menu) {
            case MENU_STATE::LOBBY: {
                // Créer objet Lobby
                lobby_->setGroupeLeader(player_->getPlayerId());
                if (!lobby_->isChoosing()) {
                    view_->showMenuLobby();
                }
                controller_->captureInput(menu, *this);
                break;
            }
            case MENU_STATE::MAIN:
                view_->showMenu(menu);
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::INVITATION:
                view_->showInvitationMenu(player_);
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::GAME_INVITATION:
                // view_->showGameInvitationMenu(player_);
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::PROFILE:
                view_->showProfileMenu(player_);
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::RANKING:
                view_->showMenu(menu);
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::CREATING:
                view_->showMenu(menu);
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::CHATROOM:
                view_->showMenu(menu);
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::GAME:
            default:
                running = false;
                break;
        }
    }
}
