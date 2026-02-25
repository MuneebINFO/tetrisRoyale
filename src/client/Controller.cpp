#include <pthread.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "Game.h"
#include "InvitationManager.h"
#include "Lobby.h"
#include "Player.h"
#include "Server.h"
#include "Tetris.h"
#include "ViewCLI.h"
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
    pthread_mutex_init(&chatMutex, nullptr);
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
    } else {
        pthread_create(&thread, nullptr, GameInputUser, data_.get());
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
        case MENU_STATE::CHATROOM:
            captureInputChatRoom(tetris);
            break;
        default:
            break;
    }
}

void* ControllerCLI::GameInputUser(void* c) {
    ThreadData* data = static_cast<ThreadData*>(c);
    ControllerCLI* controller = static_cast<ControllerCLI*>(data->controller);
    Game* game = static_cast<Game*>(data->toControl);
    controller->threadActive = true;
    int key;
    Signal& signal = Signal::getInstance();
    while (signal.getSigIntFlag() == 0 and game->getRunning()) {
        bool moved = false;
        // if (game->isControllerWaiting()) continue;

        key = getch();
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
        if (key == 'p') {
            // view_->showPauseMenu();
            // std::cout << "Game is paused" << std::endl;
            if (!signal.checkSigTstp(*game)) {
                game->setPaused(!game->getPaused());
            }
            game->sendSignalMutex();
        }

        if (game->getPaused()) continue;

        // malus (commandes bloquées)
        if (game->isInputBlocked()) continue;

        if (key == 'q') {  // Gauche
            if (game->isInvCMD())
                game->sendMovementMessage(
                    0, 1);  // si cmd inversé si malus -> envoyer à droite
            else
                game->sendMovementMessage(0, -1);  // sinon gauche comme prévu

        } else if (key == 'd') {  // Droite
            if (game->isInvCMD())
                game->sendMovementMessage(0, -1);
            else
                game->sendMovementMessage(0, 1);

        } else if (key == 's') {  // Descendre
            if (game->isInvCMD())
                game->sendMovementMessage(-1, 0);
            else
                game->sendMovementMessage(1, 0);

        } else if (key == 'z') {  // Rotation horlogique
            if (game->isInvCMD())
                game->sendRotationMessage(false);
            else
                game->sendRotationMessage(true);

        } else if (key == 'e') {  // Rotation anti horlogique
            if (game->isInvCMD())
                game->sendRotationMessage(true);
            else
                game->sendRotationMessage(false);

        } else if (key == 'm' && game->getGameMode() ==
                                     "Tetris Royal") {  // malus en mode Royal
            std::string type = "MALUS";
            game->askRoyalPermission(type);

        } else if (key == 'b' && game->getGameMode() ==
                                     "Tetris Royal") {  // bonus en mode Royal
            std::string type = "BONUS";
            game->askRoyalPermission(type);
        }

        if (moved) {
        }  // Check invariant ?
    }
    game->setRunning(false);
    return nullptr;
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

    valide = false;
    while (!valide and signal.getSigIntFlag() == 0) {
        char nom[MAX_NAME_LENGTH];
        int res = mvwgetnstr(win, 2, 21, nom, MAX_NAME_LENGTH);
        info.first = nom;
        if (res == ERR) {
            continue;
        }
        char password[MAX_NAME_LENGTH];
        mvwgetnstr(win, 3, 16, password, MAX_NAME_LENGTH);
        info.second = password;
        valide = IController::validateInput(info.first) and
                 IController::validateInput(info.second);
    }
    return info;
}

// MENU ControllerCLI
void ControllerCLI::captureInputMainMenu(Tetris& tetris) {
    Signal& signal = Signal::getInstance();
    int key;
    bool done = false;
    while (!done and signal.getSigIntFlag() == 0) {
        key = getchar();
        switch (key) {
            case 'a':
                tetris.setMenuState(MENU_STATE::LOBBY);
                done = true;
                break;
            case 'b':
                tetris.setMenuState(MENU_STATE::INVITATION);
                done = true;
                break;
            case 'e':
                tetris.setMenuState(MENU_STATE::GAME_INVITATION);
                done = true;
                break;
            case 'c':
                tetris.setMenuState(MENU_STATE::PROFILE);
                done = true;
                break;
            case 'd':
                tetris.setMenuState(MENU_STATE::RANKING);
                done = true;
                break;
            default:
                break;
        }
    }
}

void ControllerCLI::captureInputMenuLobby(Tetris& tetris) {
    std::shared_ptr<Lobby> lobby = getLobby();
    view_->showMenuLobby();
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

// process the messages from the server for the chat message
void* ControllerCLI::receiveMessages(void* arg) {
    ChatThreadArgs* args = static_cast<ChatThreadArgs*>(arg);
    ControllerCLI* controller = static_cast<ControllerCLI*>(args->controller);
    std::shared_ptr<Server> server = controller->getServer();
    std::shared_ptr<Player> player = args->player;

    int bottomLine = getmaxy(stdscr) - 1;
    char buffer[sizeof(HeaderResponse) + sizeof(PlayerHeader)];

    while (true) {
        int bytesRead = server->receiveMessage(buffer);

        if (bytesRead > 0) {
            HeaderResponse header;
            memcpy(&header, buffer, sizeof(HeaderResponse));

            int line = args->player->getLine();
            pthread_mutex_lock(&controller->chatMutex);

            if (header.type == MESSAGE_TYPE::CHAT) {
                PlayerHeader msg;
                memcpy(&msg, buffer + sizeof(HeaderResponse),
                       sizeof(PlayerHeader));

                if (strcmp(msg.message, "Message sent") == 0) {
                    std::string lastMessage =
                        args->player->getLastMessageSent();
                    mvprintw(line, static_cast<int>(lastMessage.size()) + 20,
                             "%s", msg.message);
                } else {
                    std::string displayText =
                        std::string(msg.username) +
                        " says: " + std::string(msg.message);
                    mvprintw(line, 50, "%s", displayText.c_str());
                }

                line += 2;
                args->player->setLine(line);
                move(bottomLine, 1 + strlen("> "));
                refresh();
            }
            pthread_mutex_unlock(&controller->chatMutex);
        }
    }

    refresh();
    delete args;
    return nullptr;
}

void ControllerCLI::captureInputChatRoom(Tetris& tetris) {
    std::shared_ptr<Server> server_ = getServer();
    std::shared_ptr<Player> player_ = getPlayer();
    Signal& signal_ = Signal::getInstance();
    bool done = false;
    chatThreadActive = true;
    PlayerHeader you = player_->getPlayer();
    PlayerHeader friendSelected = player_->getFriendSelected();
    ChatThreadArgs* args =
        new ChatThreadArgs{this, friendSelected.username, player_};
    pthread_create(&chatThread, nullptr, receiveMessages, args);
    std::string buffer;

    while (!done && signal_.getSigIntFlag() == 0) {
        pthread_mutex_lock(&chatMutex);
        int line = args->player->getLine();
        int pos = static_cast<int>(buffer.length());
        pthread_mutex_unlock(&chatMutex);
        int ch = getch();

        switch (ch) {
            case 27: {
                strcpy(you.receiver, "");
                server_->handleSocialRequest(SOCIAL_TYPE::LEAVE_CHAT, you);
                timespec timeout;
                clock_gettime(CLOCK_REALTIME, &timeout);
                timeout.tv_sec += 0;
                timeout.tv_nsec += 100000000;

                int join_result =
                    pthread_timedjoin_np(chatThread, nullptr, &timeout);
                if (join_result == ETIMEDOUT) {
                    pthread_cancel(chatThread);
                    pthread_detach(chatThread);
                }

                tetris.setMenuState(MENU_STATE::PROFILE);
                done = true;
            } break;
            case '\n': {
                pthread_mutex_lock(&chatMutex);
                view_->refreshChatView(line, buffer);
                if (!buffer.empty()) {
                    args->player->setLastMessageSent(buffer);
                    chatModel_->sendMessage(server_, friendSelected, buffer);
                    pthread_mutex_unlock(&chatMutex);
                } else {
                    pthread_mutex_unlock(&chatMutex);
                }
                buffer.clear();
            } break;

            case KEY_BACKSPACE:
            case 127:
            case '\b': {
                if (pos > 0) {
                    buffer.pop_back();
                    pthread_mutex_lock(&chatMutex);
                    view_->handleBackSpace(pos - 1, ' ');
                    pthread_mutex_unlock(&chatMutex);
                }
            } break;

            default: {
                if (isprint(ch) && buffer.length() < MAX_LENGTH_MESSAGES) {
                    buffer.push_back(static_cast<char>(ch));
                    pthread_mutex_lock(&chatMutex);
                    view_->renderChatPrompt(pos, ch);
                    pthread_mutex_unlock(&chatMutex);
                }
            } break;
        }
    }
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
