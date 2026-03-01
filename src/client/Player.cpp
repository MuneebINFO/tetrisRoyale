#include "Player.h"

#include <memory>
#include <vector>

#include "Controller.h"
#include "View.h"
#include "InvitationManager.h"
#include "Message.h"
#include "Server.h"
#include "Tetris.h"

Player::Player(std::shared_ptr<IView> view,
               std::shared_ptr<IController> controller,
               std::shared_ptr<Server> server)
    : signal_{Signal::getInstance()},
      pseudo_{""},
      playerId_{-1},
      noFriends_{false},
      requestAccepted_{false},
      vectorFriends_{},
      vectorInvitations_{},
      server_{server},
      view_{view},
      controller_{controller} {}

void Player::setHightScore(int score) {
    if (score > player_.highScore) {
        player_.highScore = score;
        if (auto server = getServer()) {
            server->handleSocialRequest(SOCIAL_TYPE::UPDATE_HIGHSCORE, player_);
        }
    }
}
bool Player::login(Tetris* tetris) {
    AccountResponseHeader response;
    ACCOUNT_RESPONSE result = ACCOUNT_RESPONSE::VALID;
    Message message;
    char buffer[BUFFER_SIZE];
    std::string pseudo;
    std::string password;
    std::shared_ptr<IView> view = getView();
    std::shared_ptr<Server> server = getServer();

    ACCOUNT_STATE step = view->showAccountConnection();
    ACCOUNT_TYPE type = translateAccountState(step);

    if (!server->isConnected()) {
        view->showMessage("Error: Connection to server failed", 20);
        view->showMessage("Disconnecting...", 21);
        sleep(2);
        return false;
    }
    do {
        std::pair<std::string, std::string> info = getUserInfo(step);
        pseudo = info.first;
        password = info.second;

        if (pseudo.empty() or password.empty()) {
            continue;
        }
        message.serializeConnection(type, pseudo, password, buffer);
        server->sendMessage(buffer, sizeof(Header) + sizeof(AccountHeader));
        server->receiveMessage(buffer);
        response = message.fromBuffer(buffer);
        result = response.responseType;
        if (result == ACCOUNT_RESPONSE::ERROR ||
            result == ACCOUNT_RESPONSE::EXIST) {
            view->setErrorMessage(std::string(response.message));
        }

    } while (result != ACCOUNT_RESPONSE::VALID and
             signal_.getSigIntFlag() == 0);

    setName(pseudo);
    setPlayerId(response.idPlayer);
    tetris->setMenuState(MENU_STATE::MAIN);
    return true;
}

std::pair<std::string, std::string> Player::getUserInfo(ACCOUNT_STATE step) {
    std::shared_ptr<IController> controller = getController();
    std::shared_ptr<IView> view = getView();
    if (step == ACCOUNT_STATE::LOGIN) {
        view->showLogin();
    } else if (step == ACCOUNT_STATE::SIGNUP) {
        view->showSignUp();
    }
    std::pair<std::string, std::string> info = controller->getUserLoginInfo();
    return info;
}

// Allow to switch between server talk and client usage
ACCOUNT_TYPE Player::translateAccountState(ACCOUNT_STATE menu) {
    switch (menu) {
        case ACCOUNT_STATE::LOGIN:
            return ACCOUNT_TYPE::LOGIN;
        case ACCOUNT_STATE::SIGNUP:
            return ACCOUNT_TYPE::REGISTER;
        case ACCOUNT_STATE::LOGOUT:
            return ACCOUNT_TYPE::LOGOUT;
        case ACCOUNT_STATE::ACCOUNT:
            return ACCOUNT_TYPE::LOGIN;
        default:
            return ACCOUNT_TYPE::LOGIN;
    }
}

std::vector<LobbyInvitation> Player::getLobbyInvitations() {
    InvitationManager invitationManager(shared_from_this());
    return invitationManager.getLobbyInvitations();
}
// GETTER AND SETTER

std::shared_ptr<Server> Player::getServer() {
    if (auto server = server_.lock()) {
        return server;
    }
    return nullptr;
}
std::shared_ptr<IView> Player::getView() {
    if (auto view = view_.lock()) {
        return view;
    }
    return nullptr;
}

std::shared_ptr<IController> Player::getController() {
    if (auto controller = controller_.lock()) {
        return controller;
    }
    return nullptr;
}
