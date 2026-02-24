
#define NCURSES_NOMACROS 1
#include <ncursesw/curses.h>
#include <ncursesw/menu.h>
#include <sys/types.h>

#include <sstream>

#include "Game.h"
#include "InvitationManager.h"
#include "Lobby.h"
#include "Player.h"
#include "Server.h"
#include "ViewCLI.h"

void IView::setLobby(std::shared_ptr<Lobby> lobby) { lobby_ = lobby; }
void IView::setPlayer(std::shared_ptr<Player> player) { player_ = player; }

ViewCLI::ViewCLI() : IView{} {
    setupNcurses();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    winGame_ = newwin(ymax, xmax, 0, 0);
    winMenu_ = newwin(ymax / 2, xmax / 2, ymax / 4, xmax / 4);
    if (pthread_mutex_init(&mutexCoutGrid_, nullptr) != 0) {
        throw std::runtime_error("Error: mutex init failed");
    }
}

ViewCLI::~ViewCLI() { pthread_mutex_destroy(&mutexCoutGrid_); }

void ViewCLI::setupNcurses() {
    initscr();
    noecho();
    cbreak();
    curs_set(0);
    setlocale(LC_ALL, "");
    if (has_colors()) {
        start_color();
        use_default_colors();

        init_pair(1, COLOR_RED, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_GREEN);
        init_pair(3, COLOR_YELLOW, COLOR_YELLOW);
        init_pair(4, COLOR_BLUE, COLOR_BLUE);
        init_pair(5, COLOR_MAGENTA, COLOR_MAGENTA);
        init_pair(6, COLOR_CYAN, COLOR_CYAN);
        init_pair(7, COLOR_WHITE, COLOR_WHITE);
        init_pair(8, COLOR_BLACK, -1);
        init_pair(9, COLOR_WHITE, COLOR_BLACK);
        init_pair(100, COLOR_RED, -1);
    } else {
        printw("Erreur : Terminal ne supporte pas les couleurs !\n");
        refresh();
        getch();
        endwin();
        exit(1);
    }
}

void ViewCLI::showEndScreen(bool won) {
    clear(); // nettoie l'écran ncurses
    int centerY = LINES / 2;
    int centerX = COLS / 2 - 10;

    if (won) {
        mvprintw(centerY, centerX, "YOU WIN!");
    } else {
        mvprintw(centerY, centerX, "GAME OVER");
    }

    mvprintw(centerY + 2, centerX - 5, "Press any key to exit...");
    refresh();
    getch();  // attendre une touche
    endwin();
}

void ViewCLI::showBoards(Game* game) {
    std::uint16_t myId = static_cast<std::uint16_t>(game->getPlayer()->getPlayerId());
    auto boards = game->getBoards();

    int gridWidth = WIDTH + 4;
    int startX = 2 + 2 * gridWidth;
    int startY = 3;
    int blocColor = 7;

    for (const auto& [playerID, grid] : boards) {
        if (playerID == myId) continue;

        // vérifier si la grille est "vide"
        bool isEmpty = true;
        for (const auto& row : grid) {
            for (auto cell : row) {
                if (cell != 0) {
                    isEmpty = false;
                    break;
                }
            }
            if (!isEmpty) break;
        }

        if (isEmpty) continue; // ne pas afficher les boards vides

        mvaddch(startY, startX, ACS_ULCORNER);
        for (int i = 0; i < WIDTH; ++i)
            mvaddch(startY, startX + 1 + i, ACS_HLINE);
        mvaddch(startY, startX + WIDTH + 1, ACS_URCORNER);

        for (int i = 0; i < HEIGHT; ++i) {
            mvaddch(startY + 1 + i, startX, ACS_VLINE);
            for (int j = 0; j < WIDTH; ++j) {
                if (grid[i][j] == 0) {
                    mvprintw(startY + 1 + i, startX + 1 + j, "  ");
                } else {
                    attron(COLOR_PAIR(blocColor));
                    mvaddch(startY + 1 + i, startX + 1 + j, ACS_BLOCK);
                    mvaddch(startY + 1 + i, startX + 1 + j + 1, ACS_BLOCK);
                    attroff(COLOR_PAIR(blocColor));
                }
            }
            mvaddch(startY + 1 + i, startX + WIDTH + 1, ACS_VLINE);
        }

        mvaddch(startY + HEIGHT + 1, startX, ACS_LLCORNER);
        for (int i = 0; i < WIDTH; ++i)
            mvaddch(startY + HEIGHT + 1, startX + 1 + i, ACS_HLINE);
        mvaddch(startY + HEIGHT + 1, startX + WIDTH + 1, ACS_LRCORNER);

        mvprintw(startY - 2, startX, "Player %d", playerID);
        startX += gridWidth;
    }
}

void ViewCLI::showGame(Game* game) {
    std::vector<std::vector<std::uint8_t>> grid = game->getGrid();
    Tetramino current = game->getCurrent();

    int rows = int(current.shape_.size());
    int cols = int(current.shape_[0].size());

    // Ajouter le Tétramino actif dans la grille temporaire
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (current.shape_[i][j] == 1) {
                int x = current.x_ + i;
                int y = current.y_ + j;
                if (x >= 0 && x < HEIGHT && y >= 0 && y < WIDTH) {
                    grid[x][y] = current.colorIndex_;
                }
            }
        }
    }
    int marginX = 3;
    int marginY = 4;
    curs_set(0);
    mvprintw(1, marginX, "Points: %d", game->getPoints());
    // Bordure supérieure
    mvaddch(marginY - 1, 2, ACS_ULCORNER);
    for (int i = 0; i < 2 * WIDTH; ++i) mvaddch(marginX, 3 + i, ACS_HLINE);
    mvaddch(marginY - 1, marginX + 2 * WIDTH, ACS_URCORNER);

    // Affichage de la grille
    for (int i = 0; i < HEIGHT; ++i) {
        mvaddch(marginY + i, 2, ACS_VLINE);
        for (int j = 0; j < WIDTH; ++j) {
            if (grid[i][j] == 0) {
                mvprintw(marginY + i, marginX + 2 * j, "  ");
            } else {
                attron(COLOR_PAIR(grid[i][j]));
                mvaddch(marginY + i, marginX + 2 * j, ACS_BLOCK);
                mvaddch(marginY + i, marginX + 2 * j + 1, ACS_BLOCK);
                attroff(COLOR_PAIR(grid[i][j]));
            }
        }
        mvaddch(marginY + i, marginX + 2 * WIDTH, ACS_VLINE);
    }
    // Bordure inférieure
    mvaddch(marginY + HEIGHT, 2, ACS_LLCORNER);
    for (int i = 0; i < 2 * WIDTH; ++i)
        mvaddch(marginY + HEIGHT, marginX + i, ACS_HLINE);
    mvaddch(marginY + HEIGHT, marginX + 2 * WIDTH, ACS_LRCORNER);
    showBoards(game);
    refresh();
    // wnoutrefresh(stdscr);
    // doupdate();
}

// ASK: What do we think about this one ?
int ViewCLI::showMenuInviteFriendToParty(std::vector<FriendHeader> friends) {
    Signal& signal = Signal::getInstance();
    ITEM** my_items;
    WINDOW* newWin = getWinMenu();
    int c;
    MENU* my_menu;
    ssize_t n_choices;
    ITEM* cur_item;
    int idx = -1;
    bool done = false;

    /* Creating menu screen */
    basicMenuConfig();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    mvwprintw(newWin, 0, 2, "INVITING FRIENDS");
    mvprintw(1, 0, "Number of player: %i", getLobby()->getNumberOfPlayer());
    mvprintw(2, 0, "Mode choosen: %s", getLobby()->getGameMode().c_str());
    mvprintw(3, 0, "Player invited: 0/%i", getLobby()->getNumberOfPlayer());
    const char* msg = "Who would u like to play the game with ?";
    int centerX = (xmax - static_cast<int>(strlen(msg))) / 2;
    mvprintw(8, centerX, "%s", msg);
    mvwprintw(newWin, 2, 2, "Friends :");
    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press B to go down");
    mvprintw(messageY + 1, 0, "Press H to go up");
    /* MENU PART */
    n_choices = friends.size();
    my_items = static_cast<ITEM**>(calloc(n_choices + 1, sizeof(ITEM*)));
    for (int i = 0; i < n_choices; ++i) {
        my_items[i] = new_item(friends[i].username, "");
    }
    my_items[n_choices] = static_cast<ITEM*>(nullptr);
    my_menu = new_menu(static_cast<ITEM**>(my_items));
    set_menu_win(my_menu, newWin);
    set_menu_sub(my_menu, derwin(newWin, 6, 38, 3, 1));
    set_menu_mark(my_menu, " * ");
    post_menu(my_menu);
    refresh();
    while (!done and (c = wgetch(newWin)) >= 0 and
           signal.getSigIntFlag() == 0) {
        switch (c) {
            case 'b':
                menu_driver(my_menu, REQ_DOWN_ITEM);
                break;
            case 'h':
                menu_driver(my_menu, REQ_UP_ITEM);
                break;
            case ENTER:
                cur_item = current_item(my_menu);
                idx = item_index(cur_item);
                done = true;
                break;
            case ESCAPE:
                idx = -2;
                done = true;
                break;
            default:
                break;
        }
    }
    unpost_menu(my_menu);
    free_menu(my_menu);
    for (int i = 0; i < n_choices; ++i) {
        free_item(my_items[i]);
    }
    clear();
    return idx;
}

void ViewCLI::showOptionInviteFriend(LobbyInviteFriend& lobbyFriend) {
    Signal& signal = Signal::getInstance();
    ITEM** my_items;
    WINDOW* newWin = getWinMenu();
    int c;
    MENU* my_menu;
    ssize_t n_choices;
    ITEM* cur_item;
    basicMenuConfig();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    mvwprintw(newWin, 0, 2, "INVITING FRIENDS");
    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press B to go down");
    mvprintw(messageY + 1, 0, "Press H to go up");
    mvprintw(messageY + 2, 0, "Press Enter to confirm");
    mvprintw(messageY + 3, 0, "Press Esc to return");

    my_items = static_cast<ITEM**>(calloc(3, sizeof(ITEM*)));
    my_items[0] = new_item("Gamer", "");
    my_items[1] = new_item("Spectator", "");
    my_items[2] = static_cast<ITEM*>(nullptr);
    n_choices = 2;
    my_menu = new_menu(static_cast<ITEM**>(my_items));
    set_menu_win(my_menu, newWin);
    set_menu_sub(my_menu, derwin(newWin, 6, 38, 3, 1));
    set_menu_mark(my_menu, " * ");
    post_menu(my_menu);
    refresh();
    while ((c = wgetch(newWin)) and c >= 0 and signal.getSigIntFlag() == 0) {
        switch (c) {
            case 'b':
                menu_driver(my_menu, REQ_DOWN_ITEM);
                break;
            case 'h':
                menu_driver(my_menu, REQ_UP_ITEM);
                break;
            case ENTER: {
                cur_item = current_item(my_menu);
                int idx = item_index(cur_item);
                if (idx == 0) {
                    lobbyFriend.asGamer = true;
                } else {
                    lobbyFriend.asGamer = false;
                }
                unpost_menu(my_menu);
                free_menu(my_menu);
                for (int i = 0; i < n_choices; ++i) {
                    free_item(my_items[i]);
                }
                return;
                break;
            }
            default:
                break;
        }
    }
}

ACCOUNT_STATE ViewCLI::showAccountConnection() {
    Signal& signal = Signal::getInstance();
    clear();
    noecho();
    WINDOW* win = getWinMenu();
    box(win, 0, 0);

    mvwprintw(win, 2, 2, "s - Se connecter");
    mvwprintw(win, 3, 2, "c - Créer un compte");

    char choice;
    do {
        choice = static_cast<char>(wgetch(win));
    } while (choice != 's' && choice != 'c' && choice != 'q' and
             signal.getSigIntFlag() == 0);

    switch (choice) {
        case 's':
            return ACCOUNT_STATE::LOGIN;
        case 'c':
            return ACCOUNT_STATE::SIGNUP;
        default:
            return ACCOUNT_STATE::ACCOUNT;
    }
}

void ViewCLI::showLogin() {
    WINDOW* win = getWinMenu();

    werase(win);
    showErrorMessage(getErrorMessage());
    echo();
    curs_set(1);

    box(win, 0, 0);
    mvwprintw(win, 0, 2, "CONNEXION");
    mvwprintw(win, 2, 2, "Nom d'utilisateur: ");
    mvwprintw(win, 3, 2, "Mot de passe: ");
    refreshScreen();

    wnoutrefresh(win);
    return;
}

void ViewCLI::showSignUp() {
    WINDOW* win = getWinMenu();

    werase(win);
    showErrorMessage(getErrorMessage());
    echo();
    curs_set(1);

    box(win, 0, 0);
    mvwprintw(win, 0, 2, "INSCRIPTION");
    mvwprintw(win, 2, 2, "Nom d'utilisateur: ");
    mvwprintw(win, 3, 2, "Mot de passe: ");
    refreshScreen();

    wnoutrefresh(win);
    return;
}

void ViewCLI::showMenu(MENU_STATE menu) {
    switch (menu) {
        case MENU_STATE::MAIN: {
            showMainMenu();
            break;
        }
        case MENU_STATE::LOBBY: {
            showMenuLobby();
            break;
        }
        case MENU_STATE::GAME: {
            showPlayMenu();
            break;
        }
        case MENU_STATE::RANKING: {
            showRankingMenu();
            break;
        }
        case MENU_STATE::CREATING: {
            showCreatingMenu();
            break;
        }
        case MENU_STATE::PROFILE:
        case MENU_STATE::INVITATION:
        case MENU_STATE::GAME_INVITATION:
        default:
            break;
    }
}

void ViewCLI::showMenuLobby() {
    WINDOW* newWin = getWinMenu();
    basicMenuConfig();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    mvwprintw(newWin, 0, 2, "LOBBY");
    mvwprintw(newWin, 1, 2, "Number of players : ");
    mvwprintw(newWin, 12, 2, "Game mode : ");
    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press a to choose the number of players");
    mvprintw(messageY + 1, 0, "Press b to choose game mode");
    mvprintw(messageY + 2, 0, "Press space to create the game");
    mvprintw(messageY + 3, 0, "Press Esc to return to Main Menu");
    refreshScreen();
}

void ViewCLI::showLobbyModify() {
    int c;
    int idx = 0, base = 3, nChoices;
    WINDOW* win = getWinMenu();
    std::shared_ptr<Lobby> lobby = getLobby();
    std::vector<std::string> gameMode = {"Endless", "Dual", "Classic"};

    basicMenuConfig();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    int messageY = ymax - 5;
    mvwprintw(win, 1, 3, "Game mode : ");
    mvprintw(messageY, 0, "Press B to go down");
    mvprintw(messageY + 1, 0, "Press ENTER to confirm");
    mvprintw(messageY + 2, 0, "Press ESC to return");

    for (int i = 0; i < int(gameMode.size()); i++) {
        mvwprintw(win, base + i, base, "%s", gameMode[i].c_str());
    }
    nChoices = int(gameMode.size());

    wattron(win, COLOR_PAIR(1));
    mvwprintw(win, idx + base, base, "%s", gameMode[idx].c_str());
    wattroff(win, COLOR_PAIR(1));
    refresh();
    while ((c = wgetch(win)) >= 0) {
        switch (c) {
            case 'B':
            case 'b':
                idx = (idx + 1) % nChoices;
                wattron(win, COLOR_PAIR(1));
                mvwprintw(win, idx + base, base, "%s", gameMode[idx].c_str());
                wattroff(win, COLOR_PAIR(1));
                mvwprintw(
                    win, (((idx - 1) % nChoices + nChoices) % nChoices) + base,
                    base, "%s",
                    gameMode[(((idx - 1) % nChoices + nChoices) % nChoices)]
                        .c_str());
                break;
            case ENTER:
                lobby->setGameMode(gameMode[idx]);
                if (idx == 0) {
                    lobby->setNumberOfPlayer(1);
                } else if (idx == 1) {
                    lobby->setNumberOfPlayer(2);
                } else {
                    handleChoiceNumber();
                }
            case ESCAPE:
            default:
                return;
        }
        wrefresh(win);
    }
}

void ViewCLI::handleChoiceNumber() {
    WINDOW* win = getWinMenu();
    int base = 3, c, i = 0, ip = 0, nChoices = MAX_PLAYERS_NUMBERS - 2;
    std::shared_ptr<Lobby> lobby = getLobby();

    for (int j = 0; j < MAX_PLAYERS_NUMBERS - 2; j++) {
        mvwprintw(win, base + j, 20, "%i", j + 3);
    }
    wattron(win, COLOR_PAIR(1));
    mvwprintw(win, base, 20, "%i", 3);
    wattroff(win, COLOR_PAIR(1));
    refresh();
    while ((c = wgetch(win)) >= 0) {
        switch (c) {
            case 'B':
            case 'b':
                i = (i + 1) % nChoices;
                ip = ((i - 1) % nChoices + nChoices) % nChoices;
                wattron(win, COLOR_PAIR(1));
                mvwprintw(win, i + base, 20, "%i", i + 3);
                wattroff(win, COLOR_PAIR(1));
                mvwprintw(win, ip + base, 20, "%i", ip + 3);
                break;
            case ENTER:
            case ESCAPE:
                lobby->setNumberOfPlayer(i + 3);
                return;
            default:
                break;
        }
        refresh();
    }
}

void ViewCLI::showLobbyWaitingRoom(bool isLeader) {
    std::shared_ptr<Lobby> lobby = getLobby();
    std::string gameMode = lobby->getGameMode();
    const std::vector<std::pair<int, std::string>>& players =
        lobby->getPlayers();
    int numberOfPlayers = int(players.size());
    const std::vector<std::pair<int, std::string>>& spectators =
        lobby->getSpectators();
    int numberOfSpectator = int(spectators.size());
    erase();
    showErrorMessage(getErrorMessage());
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    int centerX = (xmax) / 2;
    mvprintw(10, centerX - 20, "WAITING FOR LEADER TO START THE GAME");
    mvprintw(14, centerX - 25, "Gamer:");
    mvprintw(14, centerX + 10, "Spectator:");
    mvprintw(12, centerX - 25, "Game mode: %s", gameMode.c_str());
    for (int i = 0; i < numberOfPlayers; i++) {
        mvprintw(15 + i, centerX - 20, "%s", players[i].second.c_str());
    }
    for (int i = 0; i < numberOfSpectator; i++) {
        mvprintw(15 + i, centerX + 15, "%s", spectators[i].second.c_str());
    }
    if (isLeader) {
        mvprintw(0, 0, "You are the leader of the lobby");
        mvprintw(ymax - 5, 0, "Press Enter to start the game");
        mvprintw(ymax - 4, 0, "Press Esc to leave the lobby");
        mvprintw(ymax - 3, 0, "Press c to invite a friend");
        mvprintw(ymax - 2, 0, "Press m to modify the settings");
    }
    refresh();
}

void ViewCLI::showMessage(std::string message, int y, int x) {
    mvprintw(y, x, "%s", message.c_str());
    refreshScreen();
}

void ViewCLI::showErrorMessage(std::string message, int y, int x) {
    attron(COLOR_PAIR(50));
    mvprintw(y, x, "%s", message.c_str());
    attroff(COLOR_PAIR(50));
    setErrorMessage("");
}

void ViewCLI::showMainMenu() {
    WINDOW* newWin = getWinMenu();
    basicMenuConfig();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    // Afficher le menu dans la boîte
    mvwprintw(newWin, 0, 2, "MENU");
    mvwprintw(newWin, 1, 2, "Player: %s", getPlayer()->getName().c_str());
    mvwprintw(newWin, 5, 5, "PLAY");
    mvwprintw(newWin, 8, 5, "INVITATION");
    mvwprintw(newWin, 11, 5, "GAME INVITATION");
    mvwprintw(newWin, 14, 5, "PROFILE");
    mvwprintw(newWin, 17, 5, "RANKING");

    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran

    // Afficher le message en dehors de la boîte sur stdscr
    mvprintw(messageY, 0, "Press a to Play");
    mvprintw(messageY + 1, 0, "Press b to open Option Menu");
    mvprintw(messageY + 2, 0, "Press c to open Profile Menu");
    mvprintw(messageY + 3, 0, "Press d to open Ranking Menu");

    refreshScreen();
}

void ViewCLI::showInvitationMenu(std::shared_ptr<Player> player_) {
    WINDOW* newWin = getWinMenu();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    basicMenuConfig();
    mvwprintw(newWin, 0, 2, "Invitation MENU");

    InvitationManager invitationManager(player_);
    FriendHeader player;
    player.idPlayer = player_->getPlayerId();
    strcpy(player.username, player_->getName().c_str());
    invitationManager.refreshInvitationsList(getServer(), player);

    for (int i = 0; i < int(player_->getVectorInvitations().size()); i++) {
        int invitingPlayerID = player_->getVectorInvitations()[i].idPlayer;
        char invitingPlayerName[MAX_NAME_LENGTH];
        strcpy(invitingPlayerName, player_->getVectorInvitations()[i].username);
        mvwprintw(newWin, 1 + i, 1, "Invitation from : %s with ID %d",
                  invitingPlayerName, invitingPlayerID);
    }
    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press a to choose an invitation");
    mvprintw(messageY + 1, 0, "Press r to refresh the list");
    mvprintw(messageY + 2, 0, "Press Enter to accept the invitation");
    mvprintw(messageY + 3, 0, "Press Esc to return to Main Menu");
    refreshScreen();
}

void ViewCLI::showInvitationList(std::vector<FriendHeader> invitations,
                                 int& idx) {
    WINDOW* newWin = getWinMenu();
    mvwprintw(newWin, 0, 2, "Invitation MENU");
    if (invitations.size() > 0) {
        for (int i = 0; i < invitations.size(); i++) {
            if (i == idx) {
                wattron(newWin, COLOR_PAIR(1));
                mvwprintw(newWin, 1 + i, 1, "Invitation from : %s with ID %i",
                          invitations[i].username, invitations[i].idPlayer);
                wattroff(newWin, COLOR_PAIR(1));
            } else {
                mvwprintw(newWin, 1 + i, 1, "Invitation from : %s with ID %i",
                          invitations[i].username, invitations[i].idPlayer);
            }
        }
    } else {
        mvprintw(
            0, 1,
            "Cannot select an invitation, no invitation received sadge :(");
    }
    refreshScreen();
}

int ViewCLI::showGameInvitationMenu(std::vector<LobbyInvitation> invitations) {
    WINDOW* newWin = getWinMenu();
    ITEM** my_items;
    int c;
    MENU* my_menu;
    ssize_t nChoices;
    ITEM* cur_item;
    int idx = -1;

    /* Creating menu screen */
    basicMenuConfig();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    mvwprintw(newWin, 0, 2, "GAME INVITATION");
    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press B to go down");
    mvprintw(messageY + 1, 0, "Press H to go up");
    mvprintw(messageY + 2, 0, "Press ESC to leave");
    nChoices = invitations.size();
    std::vector<char*> toShow(nChoices);

    /* MENU PART */
    if (nChoices == 0) {
        showNoGameInvitation();
        return -1;
    }

    my_items = static_cast<ITEM**>(calloc(nChoices + 1, sizeof(ITEM*)));
    for (int i = 0; i < nChoices; ++i) {
        std::ostringstream oss;
        oss << "By: " << invitations[i].invitingPlayer
            << " to play: " << invitations[i].gameMode;
        std::string show = oss.str();
        toShow[i] = new char[show.size() + 1];
        std::copy(show.begin(), show.end(), toShow[i]);
        toShow[i][show.size()] = '\0';

        my_items[i] = new_item(toShow[i], "");
    }

    my_items[nChoices] = static_cast<ITEM*>(nullptr);
    my_menu = new_menu(static_cast<ITEM**>(my_items));
    set_menu_win(my_menu, newWin);
    set_menu_sub(my_menu, derwin(newWin, 18, 38, 3, 1));
    set_menu_mark(my_menu, " * ");
    set_menu_format(my_menu, 18, 1);
    post_menu(my_menu);
    refresh();
    wrefresh(newWin);
    refreshScreen();
    bool done = false;
    while (!done and (c = wgetch(newWin)) and c >= 0) {
        switch (c) {
            case 'b':
                menu_driver(my_menu, REQ_DOWN_ITEM);
                break;
            case 'h':
                menu_driver(my_menu, REQ_UP_ITEM);
                break;
            case ENTER: {
                cur_item = current_item(my_menu);
                idx = item_index(cur_item);
                done = true;
                break;
            }
            case ESCAPE:
                idx = -1;
                done = true;
                break;
            default:
                break;
        }
        wrefresh(newWin);
    }

    unpost_menu(my_menu);
    free_menu(my_menu);
    for (int i = 0; i < nChoices + 1; ++i) {
        free_item(my_items[i]);
        delete[] toShow[i];
    }
    free(my_items);
    return idx;
}

void ViewCLI::showNoGameInvitation() {
    WINDOW* newWin = getWinMenu();
    basicMenuConfig();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    mvwprintw(newWin, 0, 2, "GAME INVITATION");
    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvwprintw(newWin, 3, 1, "No game invitation received");
    mvprintw(messageY + 1, 0, "Press Esc to return to Main Menu");
    refreshScreen();

    int c;
    while ((c = wgetch(newWin)) != ESCAPE and c >= 0) {
    }
}

void ViewCLI::showFriendList(std::vector<FriendHeader> friendsList, int& idx) {
    WINDOW* newWin = getWinMenu();
    mvwprintw(newWin, 0, 2, "PROFILE MENU");

    if (int(friendsList.size()) > 0) {
        for (int i = 0; i < int(friendsList.size()); i++) {
            if (i == idx) {
                wattron(newWin, COLOR_PAIR(1));
                mvwprintw(newWin, 3 + i, 2, "%s with ID %i",
                          friendsList[i].username, friendsList[i].idPlayer);
                wattroff(newWin, COLOR_PAIR(1));
            } else {
                mvwprintw(newWin, 3 + i, 2, "%s with ID %i",
                          friendsList[i].username, friendsList[i].idPlayer);
            }
        }
    } else {
        mvprintw(0, 1, "Cannot select a friend, no friends found sadge :(");
    }
    refreshScreen();
}

void ViewCLI::showRankingMenu() {
    WINDOW* newWin = getWinMenu();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    basicMenuConfig();
    mvwprintw(newWin, 0, 2, "RANKING MENU");
    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press Esc to return to Main Menu");

    refreshScreen();
}

void ViewCLI::showProfileMenu(std::shared_ptr<Player> player) {
    basicMenuConfig();
    WINDOW* win = getWinMenu();
    mvwprintw(win, 0, 2, "PROFILE MENU");
    mvwprintw(win, 2, 2, "Friends:");
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);

    mvprintw(0, 1, "                                                   ");
    switch (friendRequestStatus_) {
        case FriendRequestStatus::FRIEND_REQUEST_SENT:
            mvprintw(0, 1, "Friend request sent to %s!",
                     targetPlayerName_.c_str());
            break;
        case FriendRequestStatus::ALREADY_IN_LIST:
            mvprintw(0, 1, "Player already in the list!");
            break;
        case FriendRequestStatus::SELF_ADD_FORBIDDEN:
            mvprintw(0, 1, "You cannot add yourself as a friend!");
            break;
        case FriendRequestStatus::PLAYER_NOT_FOUND:
            mvprintw(0, 1, "%s not found!", targetPlayerName_.c_str());
            break;
        case FriendRequestStatus::NO_FRIENDS:
            mvprintw(0, 1, "No friends found!");
            break;
        case FriendRequestStatus::FRIEND_REQUEST_ALREADY_SENT:
            mvprintw(0, 1, "Friend request already sent to %s!",
                     targetPlayerName_.c_str());
            break;
        default:
            break;
    }

    const char* msg = "Always refresh the list before adding a friend";
    int centerX = (xmax - static_cast<int>(strlen(msg))) / 2;
    mvprintw(8, centerX, "%s", msg);

    for (int i = 0; i < int(player->getVectorFriends().size()); i++) {
        std::string friendName = player->getVectorFriends()[i].username;
        int friendID = player->getVectorFriends()[i].idPlayer;
        mvwprintw(win, 3 + i, 2, "%s with ID %i", friendName.c_str(), friendID);
    }

    mvwprintw(win, 15, 2, "Add a friend");
    mvwprintw(win, 16, 2, "Remove a friend");

    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press a to add a friend");
    mvprintw(messageY + 1, 0, "Press b to remove a friend");
    mvprintw(messageY + 2, 0, "Press Esc to return to Main Menu");

    refreshScreen();
}
void ViewCLI::showPlayMenu() {
    WINDOW* newWin = getWinMenu();
    basicMenuConfig();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    mvwprintw(newWin, 0, 2, "GAME MENU");
    mvwprintw(newWin, 2, 2, "Join game");
    mvwprintw(newWin, 3, 2, "Create game");
    mvwprintw(newWin, 4, 2, "Play solo game");

    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press a to join a game");
    mvprintw(messageY + 1, 0, "Press b to create a game");
    mvprintw(messageY + 2, 0, "Press c to play a solo game");
    mvprintw(messageY + 3, 0, "Press Esc to return to Main Menu");

    refreshScreen();
}

void ViewCLI::showChoiceNumber(int& selectedItem_1, int& selectedItem_2) {
    WINDOW* newWin = getWinMenu();
    mvwprintw(newWin, 0, 2, "LOBBY");
    mvwprintw(newWin, 1, 2, "Number of players : %i", selectedItem_1 + 3);

    if (selectedItem_2 == 0) mvwprintw(newWin, 12, 2, "Game mode : ");

    if (selectedItem_2 - 1 > 0)
        mvwprintw(newWin, 12, 2, "Game mode : %s",
                  GAME_MODE[selectedItem_2 - 1]);

    clearChoiceMode();
    for (int i = 0; i < MAX_PLAYERS_NUMBERS - 2; i++) {
        if (i == selectedItem_1) {
            wattron(newWin, COLOR_PAIR(1));
            mvwprintw(newWin, 1 + (i + 1), 2, "%i", i + 3);
            wattroff(newWin, COLOR_PAIR(1));
        } else {
            mvwprintw(newWin, 1 + (i + 1), 2, "%i", i + 3);
        }
    }
    selectedItem_1 = (selectedItem_1 + 1) % 6;
    refreshScreen();
}

void ViewCLI::showChoiceMode(int& selectedItem_2) {
    WINDOW* newWin = getWinMenu();
    mvwprintw(newWin, 0, 2, "LOBBY");
    mvwprintw(newWin, 12, 2, "Game mode : %s", GAME_MODE[selectedItem_2]);

    clearChoiceNumber();
    for (int i = 0; i < MODE_NUMBERS; i++) {
        if (i == selectedItem_2) {
            wattron(newWin, COLOR_PAIR(1));
            mvwprintw(newWin, 12 + (i + 1), 2, "%s", GAME_MODE[i]);
            wattroff(newWin, COLOR_PAIR(1));
        } else {
            mvwprintw(newWin, 12 + (i + 1), 2, "%s", GAME_MODE[i]);
        }
    }
    selectedItem_2 = (selectedItem_2 + 1) % 3;
    refreshScreen();
}

void ViewCLI::updateChoiceNumber(int& selectedItem_1) {
    WINDOW* newWin = getWinMenu();
    mvwprintw(newWin, 0, 2, "LOBBY");
    for (int i = 0; i < MAX_PLAYERS_NUMBERS - 2; i++) {
        mvwprintw(newWin, 1 + (i + 1), 2, " ");
        if (selectedItem_1 - 3 > 2) {
            mvwprintw(newWin, 1, 2, "Number of players : %i",
                      selectedItem_1 + 2);
        }
    }
    refreshScreen();
}

void ViewCLI::clearChoiceMode() {
    WINDOW* newWin = getWinMenu();
    for (int i = 0; i < 3; i++) {
        mvwprintw(newWin, 12 + (i + 1), 2, "                  ");
    }
}

void ViewCLI::clearChoiceNumber() {
    WINDOW* newWin = getWinMenu();
    for (int i = 0; i < MAX_PLAYERS_NUMBERS; i++) {
        mvwprintw(newWin, 1 + (i + 1), 2, " ");
    }
}

void ViewCLI::showCreatingMenu() {
    std::shared_ptr<Lobby> lobby = getLobby();
    // basicMenuConfig();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    // mvwprintw(newWin, 0, 2, "CREATING");
    mvprintw(1, 0, "Number of player: %i", int(lobby->getPlayers().size()));
    mvprintw(2, 0, "Mode choosen: %s", lobby->getGameMode().c_str());
    const char* msg = "Who would u like to play the game with ?";
    int centerX = (xmax - static_cast<int>(strlen(msg))) / 2;
    mvprintw(8, centerX, "%s", msg);
    // mvwprintw(newWin, 2, 2, "Friends :");
    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press c to choose friends to invite");
    mvprintw(messageY + 1, 0, "Press Enter to invite friends");
    mvprintw(messageY + 2, 0, "Press Space to play the game");
    mvprintw(messageY + 3, 0, "Press Esc to return to Lobby Menu");
    refreshScreen();
    refresh();
}

void ViewCLI::showChatRoom() {
    clearScreen();
    FriendHeader friendSelected = getPlayer()->getFriendSelected();
    mvprintw(0, 0, "Chat Room with %s (ESC to exit, Enter to send)",
             friendSelected.username);
    std::shared_ptr<Server> server = getServer();
    server->handleSocialRequest(SOCIAL_TYPE::GET_MESSAGE, friendSelected);
    std::vector<ChatHeader> messages = server->receiveChatListHeader();

    for (int i = 0; i < int(messages.size()); i++) {
        if (messages[i].idPlayer == getPlayer()->getPlayerId()) {
            mvprintw(1 + i, 1, "You: %s", messages[i].message);
        } else {
            mvprintw(1 + i, 50, "%s says: %s", messages[i].username,
                     messages[i].message);
        }
    }
    getPlayer()->setLine(1 + int(messages.size()));
    refreshScreen();
}
void ViewCLI::updateLobbyView(std::shared_ptr<Lobby> lobby) {
    WINDOW* newWin = getWinMenu();
    mvwprintw(newWin, 0, 2, "LOBBY");
    clearChoiceMode();
    clearChoiceNumber();

    std::string mode = lobby->getGameMode();

    if (mode == "Endless") {
        mvwprintw(newWin, 1, 2, "Number of players : 1");
    } else if (mode == "Dual") {
        mvwprintw(newWin, 1, 2, "Number of players : 2");
    } else if (mode == "Classic") {
        mvwprintw(newWin, 1, 2, "Number of players : 3");
    } else {
        mvwprintw(newWin, 1, 2, "Number of players : ");
    }
    refreshScreen();
}
void ViewCLI::clearChoiceDisplay() {
    WINDOW* newWin = getWinMenu();
    mvwprintw(newWin, 1, 2, "Number of players :       ");
    mvwprintw(newWin, 12, 2, "                               ");
    refreshScreen();
}

void ViewCLI::clearScreen() {
    clear();
    refresh();
}

void ViewCLI::refreshScreen() {
    WINDOW* newWin = getWinMenu();
    refresh();
    wrefresh(newWin);
}

void ViewCLI::basicMenuConfig() {
    clearScreen();
    werase(winMenu_);
    noecho();
    curs_set(0);
    box(winMenu_, 0, 0);
}
