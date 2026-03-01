#include "Tetris.h"

#include <cstdio>

#include "ControllerCLI.h"
#include "SocialController.h"
#include "ViewCLI.h"
#include "InvitationManager.h"
#include "Lobby.h"
#include "Player.h"
#include "Server.h"
#include "Signal.h"

Tetris::Tetris()
    : signal_{Signal::getInstance()}, menuState_{MENU_STATE::MAIN} {
    server_ = std::make_shared<Server>();
}

Tetris::~Tetris() {
    endwin();
}

// More of a constructor than a function
void Tetris::init() {
    view_ = std::make_shared<ViewCLI>();
    controller_ = std::make_shared<ControllerCLI>(view_);
    socialController_ = std::make_shared<SocialController>();
    socialView_ = std::make_shared<SocialView>();
    player_ = std::make_shared<Player>(view_, controller_, server_);
    lobby_ =
        std::make_shared<Lobby>(*this, controller_, player_, view_, server_);
    view_->setLobby(lobby_);
    view_->setPlayer(player_);
    view_->setServer(server_);
    socialView_->setViewCLI(std::dynamic_pointer_cast<ViewCLI>(view_));

    controller_->setLobby(lobby_);
    controller_->setServer(server_);
    controller_->setPlayer(player_);

    socialController_->setPlayer(player_);
    socialController_->setServer(server_);
    socialController_->setSocialView(socialView_);

    server_->setPlayer(player_);
}

bool Tetris::run() {
    if (server_->connectToServer() != 0) {
        return 1;
    }
    if (!player_->login(this)) return 1;

    bool running = true;
    MENU_STATE menu;

    // Core loop
    while (running and signal_.getSigIntFlag() == 0) {
        menu = getMenuState();
        switch (menu) {
            case MENU_STATE::LOBBY: {
                controller_->captureInputMenuLobby(*this);
                break;
            }
            case MENU_STATE::MAIN:
                view_->showMenu(menu);
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::INVITATION:
                socialView_->showInvitationMenu(player_);
                socialController_->captureInputInvitationMenu(*this);
                break;
            case MENU_STATE::GAME_INVITATION:
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::PROFILE:
                socialView_->showProfileMenu(player_);
                socialController_->captureInputProfileMenu(*this);
                break;
            case MENU_STATE::RANKING:
                view_->showMenu(menu);
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::CREATING:
                view_->showMenu(menu);
                controller_->captureInput(menu, *this);
                break;
            case MENU_STATE::GAME:
            default:
                running = false;
                break;
        }
    }
    return 0;
}

