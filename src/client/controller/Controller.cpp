#include <pthread.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "../model/Game.h"
#include "../model/InvitationManager.h"
#include "../model/Lobby.h"
#include "../model/Player.h"
#include "../model/Server.h"
#include "../model/Tetris.h"
#include "../view/ViewCLI.h"
#include "ControllerCLI.h"

IController::IController(std::shared_ptr<IView> view) : view_(view) {}
bool IController::validateInput(std::string input) {
    return input.size() > 0 and input.size() < MAX_NAME_LENGTH;
}
void IController::handleMode(std::string mode, int nbPlayer) {
    std::shared_ptr<Lobby> lobby = getLobby();
    lobby->setGameMode(mode);
    if (mode == "Endless") {
        lobby->setNumberOfPlayer(1);
        setMultiplayerMode(false);

    } else if (mode == "Dual") {
        lobby->setNumberOfPlayer(2);
        setMultiplayerMode(false);

    } else if (mode == "Classic" || mode == "Tetris Royal") {
        lobby->setNumberOfPlayer(nbPlayer);
        setMultiplayerMode(true);
    }
}
// GETTER AND SETTER

std::shared_ptr<IView> IController::getView() {
    if (auto v = view_.lock()) {
        return v;
    }
    return nullptr;
}
std::shared_ptr<Lobby> IController::getLobby() {
    if (auto l = lobby_.lock()) {
        return l;
    }
    return nullptr;
}
std::shared_ptr<Server> IController::getServer() {
    if (auto s = server_.lock()) {
        return s;
    }
    return nullptr;
}

std::shared_ptr<Player> IController::getPlayer() {
    if (auto p = player_.lock()) {
        return p;
    }
    return nullptr;
}

ControllerCLI::ControllerCLI(std::shared_ptr<IView> view) : IController(view) {
    view_ = std::static_pointer_cast<ViewCLI>(view);
    chatModel_ = std::make_shared<ChatModel>();
    socialView_ = std::make_shared<SocialView>();
    data_ = std::make_unique<ThreadData>();  // remplace new ThreadData()
    int ret = tcgetattr(STDIN_FILENO, &old_.oldt);
    old_.oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (ret == -1 or old_.oldf == -1) {
        throw std::runtime_error("Error: tcgetattr or fcntl failed");
    }
}
ControllerCLI::~ControllerCLI() {
    // if (threadActive) {
    //     stop();
    // }
    // pthread_mutex_destroy(&chatMutex);
}

// capture input for the game. Creating a thread
void ControllerCLI::captureInput(Game* game) {
    data_->toControl = static_cast<void*>(game);
    data_->controller = this;

    if (game->getIsSpectator()) {
        gameInputSpectator(data_.get());
    } 
}

// capture input for the menu
void ControllerCLI::captureInput(MENU_STATE menu, Tetris& tetris) {
    switch (menu) {
        case MENU_STATE::MAIN:
            captureInputMainMenu(tetris);
            break;
            break;
        case MENU_STATE::GAME_INVITATION:
            captureInputGameInvitationMenu(tetris);
            break;
        case MENU_STATE::RANKING:
            captureInputRankingMenu(tetris);
            break;
        case MENU_STATE::GAME:
            captureInputPlayMenu(tetris);
            break;
        case MENU_STATE::CREATING:
            captureInputCreatingMenu(tetris);
            break;
        case MENU_STATE::LOBBY:
            captureInputMenuLobby(tetris);
            break;
        default:
            break;
    }
}


void ControllerCLI::handleKey(int ch, Game* game) {

    if (game->isControllerWaiting()) return;
    if (game->isInputBlocked()) return;

    switch (ch) {

        case 'q':
            if (game->isInvCMD()) {
                game->sendMovementMessage(0, 1);
                break;
            }
            game->sendMovementMessage(0, -1);
            break;

        case 'd':
            if (game->isInvCMD()) {
                game->sendMovementMessage(0, -1);
                break;
            }
            game->sendMovementMessage(0, 1);
            break;

        case 's':
            if (game->isInvCMD()) {
                game->sendMovementMessage(-1, 0);
                break;
            }
            game->sendMovementMessage(1, 0);
            break;

        case 'z':
            if (game->isInvCMD()) {
                game->sendRotationMessage(false);
                break;
            }
            game->sendRotationMessage(true);
            break;

        case 'e':
            if (game->isInvCMD()) {
                game->sendRotationMessage(true);
                break;
            }
            game->sendRotationMessage(false);
            break;
        
        case 'm':
            if (game->getGameMode() == "Tetris Royal") {
                std::string type = "MALUS";
                game->askRoyalPermission(type);
            }
            break;

        case 'b':
            if (game->getGameMode() == "Tetris Royal") {
                std::string type = "BONUS";
                game->askRoyalPermission(type);
            }
            break;

        case 'x':
            game->sendQuitParty();
            game->setRunning(false);
            break;

        default:
            break;
    }
}

void* ControllerCLI::gameInputSpectator(void* c) {
    ThreadData* data = static_cast<ThreadData*>(c);
    ControllerCLI* controller = static_cast<ControllerCLI*>(data->controller);
    Game* game = static_cast<Game*>(data->toControl);
    controller->threadActive = true;
    int key;
    Signal& signal = Signal::getInstance();
    while (signal.getSigIntFlag() == 0 and game->getRunning()) {
        key = fgetc(stdin);
        if (key == ERR) {
            continue;
        } else if (key < 0) {
            if (errno == EINTR) {
                game->setRunning(false);
                game->sendSignalMutex();
            }
        }
        if (key == EOF or key == 'x') {
            game->setIsLeaving(true);
            game->setRunning(false);
            game->sendQuitParty();
            game->sendSignalMutex();
        }
    }
    game->setRunning(false);
    controller->threadActive = false;
    return nullptr;
}

// Stop the thread if it is running
void ControllerCLI::stop() {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_.oldt);
    fcntl(STDIN_FILENO, F_SETFL, old_.oldf);
    threadActive = false;
    pthread_cancel(thread);
}

std::pair<std::string, std::string> ControllerCLI::getUserLoginInfo() {
    Signal& signal = Signal::getInstance();
    std::pair<std::string, std::string> info;
    bool valide = false;
    WINDOW* win = view_->getWinMenu();

    auto ub = view_->getLoginUserBox();
    auto pb = view_->getLoginPassBox();

    valide = false;
    while (!valide and signal.getSigIntFlag() == 0) {
        char nom[MAX_NAME_LENGTH];
        int res = mvwgetnstr(win, ub.y, ub.x, nom, std::min(ub.maxLen, MAX_NAME_LENGTH - 1));
        info.first = nom;
        if (res == ERR) {
            continue;
        }
        char password[MAX_NAME_LENGTH];
        mvwgetnstr(win, pb.y, pb.x, password, std::min(pb.maxLen, MAX_NAME_LENGTH - 1));
        info.second = password;
        valide = IController::validateInput(info.first) and
                 IController::validateInput(info.second);
    }
    return info;
}

// MENU ControllerCLI
void ControllerCLI::captureInputMainMenu(Tetris& tetris) {
    Signal& signal = Signal::getInstance();

    int selected = 0; // 0..4
    view_->showMainMenu(selected);

    int key;
    bool done = false;
    while (!done && signal.getSigIntFlag() == 0) {
        key = getchar();

        switch (key) {
            // navigation
            case 's':
                selected = (selected + 1) % 5;
                view_->showMainMenu(selected);
                break;
            case 'z':
                selected = (selected + 4) % 5;
                view_->showMainMenu(selected);
                break;

            // valider avec Enter (optionnel, mais pro)
            case 10:
            case 13:
                switch (selected) {
                    case 0: tetris.setMenuState(MENU_STATE::LOBBY); break;          // PLAY
                    case 1: tetris.setMenuState(MENU_STATE::INVITATION); break;     // INVITATION
                    case 2: tetris.setMenuState(MENU_STATE::GAME_INVITATION); break;// GAME INVITATION
                    case 3: tetris.setMenuState(MENU_STATE::PROFILE); break;        // PROFILE
                    case 4: tetris.setMenuState(MENU_STATE::RANKING); break;        // RANKING
                }
                done = true;
                break;

            default:
                break;
        }
    }
}

void ControllerCLI::captureInputMenuLobby(Tetris& tetris) {
    std::shared_ptr<Lobby> lobby = getLobby();
    view_->showLobbyModify();
    if (lobby->getIsSetup()) {
        lobby->setIsSetup(false);
        lobby->setupGame();
        lobby->waitGameStart(true);
    } else {
        tetris.setMenuState(MENU_STATE::MAIN);
    }
}

void ControllerCLI::waitingRoomInput(bool isLeader, bool& running) {
    std::shared_ptr<Lobby> lobby = getLobby();
    if (isLeader) {
        waitingRoomAsLeader(running);
        return;
    }
    int key = fgetc(stdin);
    switch (key) {
        case ESCAPE:
            lobby->leaveLobby();
            running = false;
        default:
            break;
    }
}

void ControllerCLI::waitingRoomAsLeader(bool& running) {
    std::shared_ptr<Lobby> lobby = getLobby();
    std::shared_ptr<Player> player = getPlayer();
    std::vector<PlayerHeader> friends = player->getVectorFriends();
    int key = fgetc(stdin);

    LobbyInviteFriend lobbyFriend;
    switch (key) {
        case 'C':
        case 'c': {
            if (friends.size() == 0) {
                break;
            }
            int idx = view_->showMenuInviteFriendToParty(friends);
            if (idx >= 0) {
                view_->showOptionInviteFriend(lobbyFriend);
                memcpy(lobbyFriend.nameInviting, friends[idx].username,
                       MAX_NAME_LENGTH);
                lobby->sendNotification(lobbyFriend);
            }
            break;
        }
        case 'M':
        case 'm':
            lobby->modifyLobby();
            break;
        case 13:  // ENTER
            lobby->sendStartNotification();
            break;
        case ESCAPE:
            running = false;
            break;
        default:
            break;
    }
}

void ControllerCLI::captureInputGameInvitationMenu(Tetris& tetris) {
    std::shared_ptr<Player> player = getPlayer();
    std::shared_ptr<Lobby> lobby = getLobby();
    std::vector<LobbyInvitation> lobbyInvitation =
        player->getLobbyInvitations();
    int idx = view_->showGameInvitationMenu(lobbyInvitation);
    if (idx != -1) {
        lobby->joinLobby(lobbyInvitation[idx]);
        lobby->waitGameStart(false);
    }
    tetris.setMenuState(MENU_STATE::MAIN);
}

void ControllerCLI::captureInputRankingMenu(Tetris& tetris) {
    Signal& signal = Signal::getInstance();
    std::shared_ptr<Player> player = getPlayer();
    std::shared_ptr<Server> server = getServer();
    int key;
    bool done = false;
    while (!done and signal.getSigIntFlag() == 0) {
        key = getchar();
        switch (key) {
            // case 'r':
            //     view_->showPlayerWithHightScore(player, server);
            //     done = true;
            //     break;
            case ESCAPE:
                tetris.setMenuState(MENU_STATE::MAIN);
                done = true;
                break;
            default:
                break;
        }
    }
}

void ControllerCLI::handleFriendRequestStatus(const PlayerHeader& invitedPlayer,
                                              std::shared_ptr<IView> view) {
    (void)invitedPlayer;
    (void)view;
}


void ControllerCLI::captureInputPlayMenu(Tetris& tetris) {
    Signal& signal = Signal::getInstance();
    int key;
    bool done = false;
    while (!done and signal.getSigIntFlag() == 0) {
        key = getchar();
        switch (key) {
            case ESCAPE:
                tetris.setMenuState(MENU_STATE::MAIN);
                done = true;
                break;
            default:
                break;
        }
    }
}

// Input for creating a game room
void ControllerCLI::captureInputCreatingMenu(Tetris& tetris) {
    std::shared_ptr<Player> player = getPlayer();
    std::shared_ptr<Lobby> lobby = getLobby();
    Signal& signal = Signal::getInstance();
    std::vector<PlayerHeader> friends = player->getVectorFriends();
    int key;
    int idx = 0;
    bool done = false;
    LobbyInviteFriend lobbyInviteFriend;
    while (idx != -2 and !done and signal.getSigIntFlag() == 0) {
        view_->showMenu(MENU_STATE::CREATING);
        key = getchar();
        switch (key) {
            case 'a':
                break;
            case 'c':
                idx = view_->showMenuInviteFriendToParty(friends);
                memcpy(lobbyInviteFriend.nameInviting, friends[idx].username,
                       MAX_NAME_LENGTH);
                if (idx >= 0) {
                    view_->showOptionInviteFriend(lobbyInviteFriend);
                    lobby->sendNotification(lobbyInviteFriend);
                }
                break;
            case ESCAPE:
                tetris.setMenuState(MENU_STATE::LOBBY);
                done = true;
                break;
            default:
                break;
        }
    }
}

std::string removeQuotes(const std::string& str) {
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}
