#include "Player.h"

#include <memory>
#include <vector>

#include "Controller.h"
#include "InvitationManager.h"
#include "Message.h"
#include "Server.h"
#include "Tetris.h"
#include "View.h"

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

bool Player::login(Tetris* tetris) {
    AccountResponseHeader response;
    ACCOUNT_RESPONSE result = ACCOUNT_RESPONSE::VALID;
    Message message;
    char buffer[BUFFER_SIZE];
    std::string pseudo;
    std::string password;

    ACCOUNT_STATE step = view_->showAccountConnection();
    ACCOUNT_TYPE type = translateAccountState(step);

    if (!server_->isConnected()) {
        view_->showMessage("Error: Connection to server failed", 20);
        view_->showMessage("Disconnecting...", 21);
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
        server_->sendMessage(buffer, sizeof(Header) + sizeof(AccountHeader));
        server_->receiveMessage(buffer);
        response = message.fromBuffer(buffer);
        result = response.responseType;
        if (result == ACCOUNT_RESPONSE::ERROR ||
            result == ACCOUNT_RESPONSE::EXIST) {
            view_->setErrorMessage(std::string(response.message));
        }

    } while (result != ACCOUNT_RESPONSE::VALID and
             signal_.getSigIntFlag() == 0);

    setName(pseudo);
    setPlayerId(response.idPlayer);
    tetris->setMenuState(MENU_STATE::MAIN);
    return true;
}

std::pair<std::string, std::string> Player::getUserInfo(ACCOUNT_STATE step) {
    if (step == ACCOUNT_STATE::LOGIN) {
        view_->showLogin();
    } else if (step == ACCOUNT_STATE::SIGNUP) {
        view_->showSignUp();
    }
    std::pair<std::string, std::string> info = controller_->getUserLoginInfo();
    return info;
}

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
