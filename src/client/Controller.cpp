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

void ControllerCLI::pushChatUiEvent_(ChatUiEvent ev) {
    std::lock_guard<std::mutex> lock(chatQueueMtx_);
    chatQueue_.push(std::move(ev));
}

bool ControllerCLI::popChatUiEvent_(ChatUiEvent& out) {
    std::lock_guard<std::mutex> lock(chatQueueMtx_);
    if (chatQueue_.empty()) return false;
    out = std::move(chatQueue_.front());
    chatQueue_.pop();
    return true;
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

    char buffer[sizeof(HeaderResponse) + sizeof(PlayerHeader)];

    while (controller->chatThreadActive) {
        int bytesRead = server->receiveMessage(buffer);
        if (bytesRead <= 0) continue;

        HeaderResponse header;
        memcpy(&header, buffer, sizeof(HeaderResponse));

        if (header.type != MESSAGE_TYPE::CHAT) continue;

        PlayerHeader msg;
        memcpy(&msg, buffer + sizeof(HeaderResponse), sizeof(PlayerHeader));

        if (strcmp(msg.message, "Message sent") == 0) {
            controller->pushChatUiEvent_({"Message sent", 20}); // status
        } else {
            std::string displayText =
                std::string(msg.username) + " says: " + std::string(msg.message);
            controller->pushChatUiEvent_({displayText, 50});
        }
    }

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
        // 1) afficher les messages entrants
        ChatUiEvent ev;
        while (popChatUiEvent_(ev)) {
            pthread_mutex_lock(&chatMutex);

            int bottomLine = getmaxy(stdscr) - 1;
            int line = player_->getLine();

            if (line >= bottomLine) {
                scrl(1);
                line = bottomLine - 1;
            }

            move(line, ev.col);
            clrtoeol();
            mvprintw(line, ev.col, "%s", ev.text.c_str());
            player_->setLine(line + 1);

            // redraw prompt
            move(bottomLine, 1);
            clrtoeol();
            mvprintw(bottomLine, 1, "%s", PROMPT_TEXT);
            pthread_mutex_unlock(&chatMutex);

            refresh();
        }

        // 2) input
        int ch = getch();
        if (ch == ERR) continue; // important si timeout()

        int pos = (int)buffer.size();
        int line = player_->getLine();

        switch (ch) {
            case 27: { // ESC
                chatThreadActive = false;
                strcpy(you.receiver, "");
                server_->handleSocialRequest(SOCIAL_TYPE::LEAVE_CHAT, you);

                // idéalement join propre
                pthread_join(chatThread, nullptr);

                tetris.setMenuState(MENU_STATE::PROFILE);
                done = true;
            } break;

            case '\n': {
                pthread_mutex_lock(&chatMutex);
                view_->refreshChatView(line, buffer);
                pthread_mutex_unlock(&chatMutex);

                if (!buffer.empty()) {
                    args->player->setLastMessageSent(buffer);
                    chatModel_->sendMessage(server_, friendSelected, buffer);
                    buffer.clear();
                }
            } break;

            case KEY_BACKSPACE:
            case 127:
            case '\b': {
                if (!buffer.empty()) {
                    buffer.pop_back();
                    pthread_mutex_lock(&chatMutex);
                    view_->handleBackSpace((int)buffer.size(), ' ');
                    pthread_mutex_unlock(&chatMutex);
                }
            } break;

            default: {
                if (isprint(ch) && buffer.size() < MAX_LENGTH_MESSAGES) {
                    buffer.push_back((char)ch);
                    pthread_mutex_lock(&chatMutex);
                    view_->renderChatPrompt((int)buffer.size() - 1, (char)ch);
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
