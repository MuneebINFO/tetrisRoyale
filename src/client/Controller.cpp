#include <pthread.h>
#include <unistd.h>

#include <cstring>

#include "ControllerCLI.h"
#include "Game.h"
#include "InvitationManager.h"
#include "Lobby.h"
#include "Player.h"
#include "Server.h"
#include "Tetris.h"
#include "ViewCLI.h"

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
        
    } else if (mode == "Classic"){
        lobby->setNumberOfPlayer(3);
        setMultiplayerMode(true);
    }
}

ControllerCLI::ControllerCLI(std::shared_ptr<IView> view) : IController(view) {
    view_ = std::static_pointer_cast<ViewCLI>(view);
    data_ = std::make_unique<ThreadData>();  // remplace new ThreadData()
    int ret = tcgetattr(STDIN_FILENO, &old_.oldt);
    old_.oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    pthread_mutex_init(&chatMutex, nullptr);
    if (ret == -1 or old_.oldf == -1) {
        throw std::runtime_error("Error: tcgetattr or fcntl failed");
    }
}
ControllerCLI::~ControllerCLI() {
    if (threadActive) {
        stop();
    }
    pthread_mutex_destroy(&chatMutex);
}

void ControllerCLI::captureInput(Game* game) {
    data_->toControl = static_cast<void*>(game);
    data_->controller = this;

    pthread_create(&thread, nullptr, GameInputUser, data_.get());
}

void ControllerCLI::captureInput(MENU_STATE menu, Tetris& tetris) {
    switch (menu) {
        case MENU_STATE::MAIN:
            captureInputMainMenu(tetris);
            break;
        case MENU_STATE::INVITATION:
            captureInputInvitationMenu(tetris);
            break;
        case MENU_STATE::GAME_INVITATION:
            captureInputGameInvitationMenu(tetris);
            break;
        case MENU_STATE::RANKING:
            captureInputRankingMenu(tetris);
            break;
        case MENU_STATE::PROFILE:
            captureInputProfileMenu(tetris);
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
        key = fgetc(stdin);

        if (key == ERR) {
            continue;
        } else if (key < 0) {
            if (errno == EINTR) {
                game->setRunning(false);
                game->sendSignalMutex();
            }
        }
        if (key == EOF) {
            game->setRunning(false);
            game->sendSignalMutex();
        }
        if (key == 'x') {
            game->setRunning(false);
            game->sendSignalMutex();
            break;
        }
        if (key == 'p') {
            // view_->showPauseMenu();
            // std::cout << "Game is paused" << std::endl;
            if (!signal.checkSigTstp(*game)) {
                game->setPaused(!game->getPaused());
            }
            game->sendSignalMutex();
        }
        if (game->getPaused()) {
            continue;
        }

        if (game->isInputBlocked()) {
            game->unblockInput();
            continue;  // on skip l'action
        }

        if (key == 'q') {  // Gauche
            game->sendMovementMessage(0, -1);

        } else if (key == 'd') {  // Droite
            game->sendMovementMessage(0, 1);

        } else if (key == 's') {  // Descendre
            game->sendMovementMessage(1, 0);

        } else if (key == 'z') {  // Rotation horlogique
            game->sendRotationMessage(true);

        } else if (key == 'e') {  // Rotation anti horlogique
            game->sendRotationMessage(false);

        } /*else if (key == 'm' && game->getGameMode() ==
                                     "Royal") {  // Activation manuelle d'un
                                                 // malus en mode Royal
            int malusType = 2;                   // TMP
            int targetPlayerId = game->getRandomOpponent();

            if (targetPlayerId != -1) {
                game->sendMalusMessage(malusType, targetPlayerId);
            }*/

        /*} else if (key == 'b' && game->getGameMode() ==
                                     "Royal") {  // Activation manuelle d'un
                                                 // bonus en mode Royal
            BonusPayload bonus;
            bonus.type = 1;  // TMP
            game->sendBonusMessage(bonus.type);
            game->applyBonus(bonus.type);
        }*/

        if (moved) {
        }  // Check invariant ?
    }
    game->setRunning(false);
    return nullptr;
}

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
    Signal& signal = Signal::getInstance();
    std::shared_ptr<Lobby> lobby = getLobby();
    int key;
    bool done = false;
    WINDOW* newWin = view_->getWinMenu();
    box(newWin, 0, 0);
    int idx1;
    int idx2;
    while (!done and signal.getSigIntFlag() == 0) {
        key = getchar();
        switch (key) {
            case 'a': {
                if (getMultiplayerMode()) {
                    captureInputChoiceMenu(key, idx1, idx2);
                    done = true;
                }
            } break;
            case 'b': {
                captureInputChoiceMenu(key, idx1, idx2);
                done = true;
            } break;
            // Enter
            case '\r': {
                if (lobby->isChoosing()) {
                    if (lobby->isChoosingNumber()) {
                        view_->updateChoiceNumber(idx1);
                        if (idx1 == 0)
                            lobby->setNumberOfPlayer(MAX_PLAYERS_NUMBERS);
                        else
                            lobby->setNumberOfPlayer(idx1 + 2);
                        view_->refreshScreen();
                    }
                    if (lobby->isChoosingMode()) {
                        if (idx2 - 1 >= 0) {
                            handleMode(GAME_MODE[idx2 - 1]);
                            view_->updateLobbyView(lobby);
                        } else {
                            handleMode(GAME_MODE[MODE_NUMBERS - 1]);
                            view_->updateLobbyView(lobby);
                        }
                        view_->refreshScreen();
                    }
                    done = true;
                }
            } break;
            // space to create the game
            case SPACE: {
                if (lobby->hasChoosedMode() && lobby->hasChoosedNumber()) {
                    lobby->setupGame();
                    if (lobby->getNumberOfPlayer() > 1) {
                        lobby->setChoosing(false);
                        setMultiplayerMode(false);

                        lobby->waitGameStart(true);
                    } else {
                        lobby->setChoosing(false);
                        setMultiplayerMode(false);
                        lobby->startGame();
                    }
                    done = true;
                }
            } break;
            case ESCAPE:
                tetris.setMenuState(MENU_STATE::MAIN);
                setMultiplayerMode(false);
                done = true;
                lobby->reset();
                break;
            default:
                break;
        }
    }
}

void ControllerCLI::waitingRoomInput(bool isLeader, bool& running) {
    std::shared_ptr<Lobby> lobby = getLobby();
    // Add key to both ?
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
    std::vector<FriendHeader> friends = player->getVectorFriends();
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
void ControllerCLI::captureInputInvitationMenu(Tetris& tetris) {
    std::shared_ptr<Server> server = getServer();
    std::shared_ptr<Player> player = getPlayer();
    Signal& signal = Signal::getInstance();
    int key;
    bool done = false;
    int index = 0;
    bool requestSelected = false;
    InvitationManager invitationManager(player);
    std::vector<FriendHeader> invitations = player->getVectorInvitations();
    FriendHeader friendPlayer;
    friendPlayer.idPlayer = player->getPlayerId();
    strcpy(friendPlayer.username, player->getName().c_str());
    while (!done and signal.getSigIntFlag() == 0) {
        chatThreadActive = true;
        key = getchar();
        switch (key) {
            case 'a': {
                if (invitations.size() > 0) {
                    requestSelected = true;
                    view_->showInvitationList(invitations, index);
                    index = (index + 1) % int(invitations.size());
                } else {
                    mvprintw(0, 1, "No invitation received");
                    refresh();
                }

            } break;
            case 'r': {
                invitationManager.refreshInvitationsList(server, friendPlayer);
                done = true;
            } break;
            case '\r': {
                if (requestSelected) {
                    if (index > 0) {
                        invitationManager.acceptInvitation(
                            invitations[index - 1].idPlayer);
                        server->handleSocialRequest(SOCIAL_TYPE::ACCEPT,
                                                    invitations[index - 1]);

                    } else {
                        invitationManager.acceptInvitation(
                            invitations[invitations.size() - 1].idPlayer);
                        server->handleSocialRequest(
                            SOCIAL_TYPE::ACCEPT,
                            invitations[invitations.size() - 1]);
                    }
                    requestSelected = false;
                    index = 0;
                }
                done = true;
            } break;

            case ESCAPE:
                tetris.setMenuState(MENU_STATE::MAIN);
                requestSelected = false;
                index = 0;
                done = true;
                break;
            default:
                break;
        }
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
    }
    tetris.setMenuState(MENU_STATE::MAIN);
}

void ControllerCLI::captureInputRankingMenu(Tetris& tetris) {
    Signal& signal = Signal::getInstance();
    int key;
    bool done = false;
    while (!done and signal.getSigIntFlag() == 0) {
        key = getchar();
        switch (key) {
            // Need to implement the ranking
            case ESCAPE:
                tetris.setMenuState(MENU_STATE::MAIN);
                done = true;
                break;
            default:
                break;
        }
    }
}

void ControllerCLI::handleFriendRequestStatus(
    const HeaderResponse& responseHeader, const FriendHeader& playerNameStr) {
    switch (responseHeader.status) {
        case FRIEND_REQUEST_STATUS::FRIEND_REQUEST_SENT:
            view_->setFriendRequestStatus(
                FriendRequestStatus::FRIEND_REQUEST_SENT,
                playerNameStr.username);
            break;

        case FRIEND_REQUEST_STATUS::PLAYER_NOT_FOUND:
            view_->setFriendRequestStatus(FriendRequestStatus::PLAYER_NOT_FOUND,
                                          playerNameStr.username);
            break;

        case FRIEND_REQUEST_STATUS::ALREADY_IN_LIST:
            view_->setFriendRequestStatus(FriendRequestStatus::ALREADY_IN_LIST,
                                          playerNameStr.username);
            break;

        case FRIEND_REQUEST_STATUS::SELF_ADD_FORBIDDEN:
            view_->setFriendRequestStatus(
                FriendRequestStatus::SELF_ADD_FORBIDDEN,
                playerNameStr.username);
            break;

        case FRIEND_REQUEST_STATUS::FRIEND_REQUEST_ALREADY_SENT:
            view_->setFriendRequestStatus(
                FriendRequestStatus::FRIEND_REQUEST_ALREADY_SENT,
                playerNameStr.username);
            break;

        default:
            break;
    }
}

void ControllerCLI::captureInputProfileMenu(Tetris& tetris) {
    std::shared_ptr<Server> server = getServer();
    std::shared_ptr<Player> player = getPlayer();
    Signal& signal = Signal::getInstance();
    int key;
    bool done = false;
    bool foundInFriendList = false;
    bool friendSelected = false;
    int index = 0;
    FriendHeader playerNameStr;
    FriendHeader friendPlayer;
    friendPlayer.idPlayer = player->getPlayerId();
    strcpy(friendPlayer.username, player->getName().c_str());
    while (!done and signal.getSigIntFlag() == 0) {
        key = getchar();
        switch (key) {
            case 'a': {
                view_->clearScreen();
                mvprintw(0, 1, "Enter the player's name to add as a friend");
                echo();
                char playerName[MAX_NAME_LENGTH];
                mvgetstr(1, 1, playerName);
                strcpy(playerNameStr.username, playerName);
                server->handleSocialRequest(SOCIAL_TYPE::INVITE, playerNameStr);

                char response[MAX_LENGTH_MESSAGES];
                HeaderResponse responseHeader;
                int bytesRead = server->receiveMessage(response);
                if (bytesRead > 0) {  // Check for positive return value
                    memcpy(&responseHeader, response, sizeof(HeaderResponse));
                    if (responseHeader.type == MESSAGE_TYPE::INVITEFRIEND) {
                        handleFriendRequestStatus(responseHeader,
                                                  playerNameStr);
                    } else {
                        view_->setFriendRequestStatus(
                            FriendRequestStatus::FRIEND_REQUEST_ALREADY_SENT,
                            playerNameStr.username);
                    }
                } else {
                    view_->setFriendRequestStatus(
                        FriendRequestStatus::PLAYER_NOT_FOUND,
                        playerNameStr.username);
                }
                noecho();
                done = true;
            } break;

            case 'r': {
                view_->setFriendRequestStatus(FriendRequestStatus::NONE);
                server->handleSocialRequest(SOCIAL_TYPE::GET_FRIEND_LIST,
                                            friendPlayer);
                std::vector<FriendHeader> friendList =
                    server->receiveFriendListHeader();
                player->setVectorFriends(friendList);
                done = true;
            } break;

            case 'k': {
                friendSelected = true;
                view_->showFriendList(player->getVectorFriends(), index);
                if (player->getVectorFriends().size() > 0)
                    index = (index + 1) % player->getVectorFriends().size();
            } break;

            case '\r': {
                std::vector<FriendHeader> friendsList =
                    player->getVectorFriends();
                if (friendSelected) {
                    if (index > 0) {
                        player->setFriendSelected(friendsList[index - 1]);
                    } else {
                        player->setFriendSelected(
                            friendsList[friendsList.size() - 1]);
                    }
                }
                friendSelected = false;
                tetris.setMenuState(MENU_STATE::CHATROOM);
                // need to add the other player with who we want to chat
                strcpy(friendPlayer.receiver,
                       player->getFriendSelected().username);
                server->handleSocialRequest(SOCIAL_TYPE::INCHATROOM,
                                            friendPlayer);
                done = true;
            } break;
            case ESCAPE: {
                view_->setFriendRequestStatus(FriendRequestStatus::NONE);
                tetris.setMenuState(MENU_STATE::MAIN);
                done = true;
            } break;
            default:
                break;
        }
    }
}

void* ControllerCLI::receiveMessages(void* arg) {
    ControllerCLI* controller = static_cast<ControllerCLI*>(arg);
    std::shared_ptr<Server> server = controller->getServer();
    ChatThreadArgs* args = static_cast<ChatThreadArgs*>(arg);
    std::shared_ptr<Player> player = args->player;

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int bottomLine = max_y - 1;

    while (controller->chatThreadActive) {
        std::string message = server->receive();
        if (!message.empty()) {
            int line = player->getLine();
            pthread_mutex_lock(&controller->chatMutex);
            std::string lastMessage = player->getLastMessageSent();
            if (strcmp(message.c_str(), "Message sent") == 0) {
                mvprintw(line, lastMessage.size() + 20, "%s", message.c_str());
            } else {
                mvprintw(line, 50, "%s", message.c_str());
            }
            line += 2;
            player->setLine(line);
            move(bottomLine, 1 + strlen("> "));
            refresh();
            pthread_mutex_unlock(&controller->chatMutex);
        }
    }
    refresh();
    delete args;
    return nullptr;
}

void ControllerCLI::captureInputChatRoom(Tetris& tetris) {
    std::shared_ptr<Player> player = getPlayer();
    std::shared_ptr<Server> server = getServer();
    Signal& signal = Signal::getInstance();
    bool done = false;
    chatThreadActive = true;
    FriendHeader friendSelected = player->getFriendSelected();
    ChatThreadArgs* args =
        new ChatThreadArgs{this, friendSelected.username, player};
    pthread_create(&chatThread, nullptr, receiveMessages, args);

    FriendHeader friendPlayer;
    friendPlayer.idPlayer = player->getPlayerId();
    strcpy(friendPlayer.username, player->getName().c_str());

    cbreak();
    noecho();
    curs_set(1);
    keypad(stdscr, TRUE);

    const char* PROMPT_TEXT = "> ";
    char message[MAX_NAME_LENGTH];
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int bottomLine = max_y - 1;

    pthread_mutex_lock(&chatMutex);
    mvprintw(bottomLine, 1, "%s", PROMPT_TEXT);
    const int promptLength = strlen(PROMPT_TEXT);
    refresh();
    pthread_mutex_unlock(&chatMutex);

    std::string buffer;

    while (!done && signal.getSigIntFlag() == 0) {
        // Safely access line value
        pthread_mutex_lock(&chatMutex);
        int line = args->player->getLine();
        int pos = buffer.length();
        move(bottomLine, 1 + promptLength + pos);
        pthread_mutex_unlock(&chatMutex);
        int initialLine = 1;
        int ch = getch();

        switch (ch) {
            case 27: {
                strcpy(friendPlayer.receiver, "");
                server->handleSocialRequest(SOCIAL_TYPE::LEAVE_CHAT,
                                            friendPlayer);
                pthread_mutex_lock(&chatMutex);
                chatThreadActive = false;
                pthread_mutex_unlock(&chatMutex);
                tetris.setMenuState(MENU_STATE::PROFILE);
                done = true;
            } break;

            case '\n': {
                pthread_mutex_lock(&chatMutex);

                move(bottomLine, 1);
                clrtoeol();
                mvprintw(bottomLine, 1, "%s", PROMPT_TEXT);

                line = args->player->getLine();
                move(line, 1);
                clrtoeol();
                mvprintw(line, 1, "You: %s", buffer.c_str());
                refresh();

                if (!buffer.empty()) {
                    args->player->setLastMessageSent(buffer);
                    strcpy(friendSelected.message, buffer.c_str());
                    server->handleSocialRequest(SOCIAL_TYPE::SEND_MESSAGE,
                                                friendSelected);
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
                    mvaddch(bottomLine, 1 + promptLength + pos - 1, ' ');
                    move(bottomLine, 1 + promptLength + pos - 1);
                    refresh();
                    pthread_mutex_unlock(&chatMutex);
                }
            } break;

            default: {
                if (isprint(ch) && buffer.length() < MAX_LENGTH_MESSAGES) {
                    buffer.push_back(ch);
                    pthread_mutex_lock(&chatMutex);
                    mvaddch(bottomLine, 1 + promptLength + pos, ch);
                    refresh();
                    pthread_mutex_unlock(&chatMutex);
                }
            } break;
        }
    }

    pthread_mutex_lock(&chatMutex);
    chatThreadActive = false;
    pthread_cancel(chatThread);
    pthread_join(chatThread, NULL);
    pthread_mutex_unlock(&chatMutex);
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

void ControllerCLI::captureInputCreatingMenu(Tetris& tetris) {
    std::shared_ptr<Player> player = getPlayer();
    std::shared_ptr<Lobby> lobby = getLobby();
    Signal& signal = Signal::getInstance();
    std::vector<FriendHeader> friends = player->getVectorFriends();
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

void ControllerCLI::captureInputChoiceMenu(int key, int& idx1, int& idx2) {
    std::shared_ptr<Lobby> lobby = getLobby();
    if (!lobby->isChoosing()) {
        idx1 = 0;
        idx2 = 0;
        lobby->setChoosing(true);
    }
    if (key == 'a') {
        lobby->setChoosingNumber(true);
        lobby->setChoosingMode(false);
        view_->showChoiceNumber(idx1, idx2);
    } else if (key == 'b') {
        lobby->setChoosingNumber(false);
        lobby->setChoosingMode(true);
        view_->clearChoiceDisplay();
        view_->showChoiceMode(idx2);
    }
}

std::string removeQuotes(const std::string& str) {
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}
