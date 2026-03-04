#include <memory>
#include <string>
#define NCURSES_NOMACROS 1
#include <ncursesw/curses.h>
#include <ncursesw/menu.h>
#include <sys/types.h>

#include <algorithm>
#include <sstream>

#include "../model/Game.h"
#include "../model/InvitationManager.h"
#include "../model/Lobby.h"
#include "../model/Player.h"
#include "../model/Server.h"
#include "../view/ViewCLI.h"

void IView::setLobby(std::shared_ptr<Lobby> lobby) { lobby_ = lobby; }
void IView::setPlayer(std::shared_ptr<Player> player) { player_ = player; }

// change to vector
std::vector<std::string> IView::getVectorGameMode() {
    std::vector<std::string> gameMode;
    for (int i = 0; i < MODE_NUMBERS; ++i) {
        gameMode.push_back(GAME_MODE[i]);
    }
    return gameMode;
}
std::shared_ptr<Lobby> IView::getLobby() {
    if (auto lobby = lobby_.lock()) {
        return lobby;
    }
    return nullptr;
}
std::shared_ptr<Player> IView::getPlayer() {
    if (auto player = player_.lock()) {
        return player;
    }
    return nullptr;
}
std::shared_ptr<Server> IView::getServer() {
    if (auto server = server_.lock()) {
        return server;
    }
    return nullptr;
}

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

        init_pair(1, COLOR_RED, COLOR_RED);
        init_pair(2, COLOR_GREEN, COLOR_GREEN);
        init_pair(3, COLOR_YELLOW, COLOR_YELLOW);
        init_pair(4, COLOR_BLUE, COLOR_BLUE);
        init_pair(5, COLOR_MAGENTA, COLOR_MAGENTA);
        init_pair(6, COLOR_CYAN, COLOR_CYAN);
        init_pair(7, COLOR_WHITE, COLOR_WHITE);
        init_pair(8, COLOR_BLACK, -1);
        init_pair(9, COLOR_WHITE, COLOR_BLACK);
        init_pair(10, COLOR_GREEN, COLOR_BLACK);
        init_pair(50, COLOR_RED, COLOR_BLACK);
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
    clear();
    nodelay(stdscr, FALSE);   // getch bloquant
    keypad(stdscr, TRUE);
    curs_set(0);

    const char* title = won ? "YOU WIN!" : "GAME OVER";
    const char* subtitle = "Press any key to exit...";

    // Dimensions "carte"
    int boxW = 44;
    int boxH = 9;

    if (boxW > COLS - 2) boxW = COLS - 2;
    if (boxH > LINES - 2) boxH = LINES - 2;

    int startY = (LINES - boxH) / 2;
    int startX = (COLS - boxW) / 2;

    // Ombre légère (si ça rentre)
    if (startY + boxH < LINES && startX + boxW < COLS) {
        attron(A_DIM);
        for (int y = 1; y <= boxH; ++y) {
            if (startY + y >= 0 && startY + y < LINES && startX + boxW < COLS)
                mvaddch(startY + y, startX + boxW, ' ');
        }
        for (int x = 1; x <= boxW; ++x) {
            if (startY + boxH < LINES && startX + x >= 0 && startX + x < COLS)
                mvaddch(startY + boxH, startX + x, ' ');
        }
        attroff(A_DIM);
    }

    // Bordure
    mvaddch(startY, startX, ACS_ULCORNER);
    for (int i = 1; i < boxW - 1; ++i) mvaddch(startY, startX + i, ACS_HLINE);
    mvaddch(startY, startX + boxW - 1, ACS_URCORNER);

    for (int i = 1; i < boxH - 1; ++i) {
        mvaddch(startY + i, startX, ACS_VLINE);
        for (int j = 1; j < boxW - 1; ++j) mvaddch(startY + i, startX + j, ' ');
        mvaddch(startY + i, startX + boxW - 1, ACS_VLINE);
    }

    mvaddch(startY + boxH - 1, startX, ACS_LLCORNER);
    for (int i = 1; i < boxW - 1; ++i) mvaddch(startY + boxH - 1, startX + i, ACS_HLINE);
    mvaddch(startY + boxH - 1, startX + boxW - 1, ACS_LRCORNER);

    // Texte centré
    int titleY = startY + 3;
    int titleX = startX + (boxW - (int)std::strlen(title)) / 2;
    int subY   = startY + 5;
    int subX   = startX + (boxW - (int)std::strlen(subtitle)) / 2;

    attron(A_BOLD);
    mvprintw(titleY, titleX, "%s", title);
    attroff(A_BOLD);

    attron(A_DIM);
    mvprintw(subY, subX, "%s", subtitle);
    attroff(A_DIM);

    refresh();
    getch();
}

void ViewCLI::showBoards(Game* game) {
    if (game->getIsSpectator()) clear();

    int myId = static_cast<int>(game->getPlayer()->getPlayerId());
    auto boards = game->getBoards();

    int gridWidth = WIDTH + 4;
    int startX = game->getIsSpectator() ? 4 : 2 + 2 * gridWidth;
    int startY = 3;
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

        if (isEmpty) continue;  // ne pas afficher les boards vides

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
                    attron(COLOR_PAIR(grid[i][j]));
                    mvaddch(startY + 1 + i, startX + 1 + j, ACS_BLOCK);
                    mvaddch(startY + 1 + i, startX + 1 + j + 1, ACS_BLOCK);
                    attroff(COLOR_PAIR(grid[i][j]));
                }
            }
            mvaddch(startY + 1 + i, startX + WIDTH + 1, ACS_VLINE);
        }

        mvaddch(startY + HEIGHT + 1, startX, ACS_LLCORNER);
        for (int i = 0; i < WIDTH; ++i)
            mvaddch(startY + HEIGHT + 1, startX + 1 + i, ACS_HLINE);
        mvaddch(startY + HEIGHT + 1, startX + WIDTH + 1, ACS_LRCORNER);

        std::string username = game->getPlayerName(playerID);
        mvprintw(startY - 2, startX, "%s", username.c_str());
        startX += gridWidth;
    }
    if (game->getIsSpectator()) refresh();
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
    if (game->getGameMode() == "Tetris Royal") {
        mvprintw(2, marginX, "Energy: %d", game->getEnergy());
        showMalus(game);
    }

    if (!game->isBlackScreen()) {
        // Bordure supérieure
        mvaddch(marginY - 1, 2, ACS_ULCORNER);
        for (int i = 0; i < 2 * WIDTH; ++i)
            mvaddch(marginY - 1, 3 + i, ACS_HLINE);
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

    } else {
        // Effacer la grille
        for (int i = 0; i < HEIGHT; ++i) {
            for (int j = 0; j < WIDTH; ++j) {
                mvprintw(marginY + i, marginX + 2 * j, "  ");
            }
        }
    }
    showBoards(game);
    refresh();
}

// show the menu for inviting a friend to the party
int ViewCLI::showMenuInviteFriendToParty(std::vector<PlayerHeader> friends) {
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
    mvprintw(messageY, 0, "Press S to go down");
    mvprintw(messageY + 1, 0, "Press Z to go up");
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
            case 's':
                menu_driver(my_menu, REQ_DOWN_ITEM);
                break;
            case 'z':
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
    free(my_items);
    clear();
    return idx;
}

// show a menu to choose if the player invited is a gamer or not
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
    mvprintw(messageY, 0, "Press S to go down");
    mvprintw(messageY + 1, 0, "Press Z to go up");
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
    int end = 0;
    while (end >= 0 and (c = wgetch(newWin)) and c >= 0 and
           signal.getSigIntFlag() == 0) {
        switch (c) {
            case 's':
                menu_driver(my_menu, REQ_DOWN_ITEM);
                break;
            case 'z':
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
                end = -1;
                break;
            }
            default:
                break;
        }
    }
    unpost_menu(my_menu);
    free_menu(my_menu);
    for (int i = 0; i < n_choices; ++i) {
        free_item(my_items[i]);
    }
    free(my_items);
}

ACCOUNT_STATE ViewCLI::showAccountConnection() {
    Signal& signal = Signal::getInstance();

    noecho();
    curs_set(0);

    WINDOW* win = getWinMenu();
    const int h = getmaxy(win);
    const int w = getmaxx(win);

    const char* gameTitle1 = "TETRIS ROYALE";
    const char* gameTitle2 = "MULTIJOUEUR";

    auto centerX = [&](const char* s) {
        int x = (w - (int)std::strlen(s)) / 2;
        return (x < 0) ? 0 : x;
    };

    const char* title = "Compte";

    // Menu options
    const char* items[] = {"CONNEXION", "INSCRIPTION"};
    constexpr int itemCount = 2;
    int selected = 0;

    auto drawCentered = [&](int y, const char* txt, bool highlight) {
        int x = (w - (int)std::strlen(txt)) / 2;
        if (x < 2) x = 2;

        if (highlight) {
            wattron(win, COLOR_PAIR(10) | A_BOLD);
            mvwprintw(win, y, x, "%s", txt);
            wattroff(win, COLOR_PAIR(10) | A_BOLD);
        } else {
            wattron(win, A_BOLD);
            mvwprintw(win, y, x, "%s", txt);
            wattroff(win, A_BOLD);
        }
    };

    auto redraw = [&]() {
        werase(win);

        wattron(win, A_BOLD);
        mvwprintw(win, 0, centerX(gameTitle1), "%s", gameTitle1);
        mvwprintw(win, 1, centerX(gameTitle2), "%s", gameTitle2);
        wattroff(win, A_BOLD);

        int frameTop = 3;
        int frameLeft = 0;
        int frameRight = w - 1;
        int frameBottom = h - 1;

        // Coins
        mvwaddch(win, frameTop, frameLeft, ACS_ULCORNER);
        mvwaddch(win, frameTop, frameRight, ACS_URCORNER);
        mvwaddch(win, frameBottom, frameLeft, ACS_LLCORNER);
        mvwaddch(win, frameBottom, frameRight, ACS_LRCORNER);

        // Lignes horizontales
        for (int x = frameLeft + 1; x < frameRight; ++x) {
            mvwaddch(win, frameTop, x, ACS_HLINE);
            mvwaddch(win, frameBottom, x, ACS_HLINE);
        }
        // Lignes verticales
        for (int y = frameTop + 1; y < frameBottom; ++y) {
            mvwaddch(win, y, frameLeft, ACS_VLINE);
            mvwaddch(win, y, frameRight, ACS_VLINE);
        }

        // Titre de page (dans le cadre)
        const int titleY = frameTop + 1;
        const int titleX = (w - (int)std::strlen(title)) / 2;

        wattron(win, A_BOLD);
        mvwprintw(win, titleY, std::max(2, titleX), "%s", title);
        wattroff(win, A_BOLD);

        // Séparateur sous le titre
        const int sepY = titleY + 2;
        for (int x = 2; x < w - 2; ++x) mvwaddch(win, sepY, x, ACS_HLINE);

        // Options au milieu (dans le cadre)
        int baseY = (frameTop + frameBottom) / 2 - 1;
        drawCentered(baseY + 0, items[0], selected == 0);
        drawCentered(baseY + 2, items[1], selected == 1);

        wattron(win, A_DIM);
        mvwprintw(win, frameBottom - 2, 2, "z/s : naviguer | Entrer : choisir");
        wattroff(win, A_DIM);

        wrefresh(win);
    };

    keypad(win, TRUE);
    redraw();

    while (signal.getSigIntFlag() == 0) {
        int ch = wgetch(win);

        if (ch == 'q') {
            return ACCOUNT_STATE::ACCOUNT;
        }
        if (ch == 's' || ch == KEY_DOWN) {
            selected = (selected + 1) % itemCount;
            redraw();
            continue;
        }
        if (ch == 'z' || ch == KEY_UP) {
            selected = (selected - 1 + itemCount) % itemCount;
            redraw();
            continue;
        }
        if (ch == '\n' || ch == KEY_ENTER || ch == 10 || ch == 13) {
            return (selected == 0) ? ACCOUNT_STATE::LOGIN : ACCOUNT_STATE::SIGNUP;
        }
    }

    return ACCOUNT_STATE::ACCOUNT;
}

void ViewCLI::showLogin() {
    WINDOW* win = getWinMenu();

    werase(win);
    showErrorMessage(getErrorMessage());
    echo();
    curs_set(1);

    box(win, 0, 0);

    const int h = getmaxy(win);
    const int w = getmaxx(win);

    // Titre centré
    const char* title = "Connexion";
    int titleX = (w - (int)std::strlen(title)) / 2;
    if (titleX < 2) titleX = 2;

    wattron(win, A_BOLD);
    mvwprintw(win, 2, titleX, "%s", title);
    wattroff(win, A_BOLD);

    // Deux "input boxes" centrées
    const int boxW = std::min(46, w - 10);
    const int boxH = 3;

    int startX = (w - boxW) / 2;
    int userY  = h / 2 - 4;
    int passY  = h / 2;

    loginUserBox_ = { userY + 1, startX + 2, boxW - 4 };
    loginPassBox_ = { passY + 1, startX + 2, boxW - 4 };

    auto drawInputBox = [&](int topY, const char* label) {
        // label centré au-dessus
        int lx = (w - (int)std::strlen(label)) / 2;
        mvwprintw(win, topY - 1, std::max(2, lx), "%s", label);

        // rectangle
        mvwaddch(win, topY, startX, ACS_ULCORNER);
        mvwaddch(win, topY, startX + boxW - 1, ACS_URCORNER);
        mvwaddch(win, topY + boxH - 1, startX, ACS_LLCORNER);
        mvwaddch(win, topY + boxH - 1, startX + boxW - 1, ACS_LRCORNER);

        for (int x = 1; x < boxW - 1; ++x) {
            mvwaddch(win, topY, startX + x, ACS_HLINE);
            mvwaddch(win, topY + boxH - 1, startX + x, ACS_HLINE);
        }
        for (int y = 1; y < boxH - 1; ++y) {
            mvwaddch(win, topY + y, startX, ACS_VLINE);
            mvwaddch(win, topY + y, startX + boxW - 1, ACS_VLINE);
        }

        // zone intérieure vide
        for (int x = 1; x < boxW - 1; ++x) mvwaddch(win, topY + 1, startX + x, ' ');
    };

    drawInputBox(userY, "Nom d'utilisateur");
    drawInputBox(passY, "Mot de passe");

    wattron(win, A_DIM);
    mvwprintw(win, h - 3, 2, "Entrer : valider");
    wattroff(win, A_DIM);

    refreshScreen();
    wnoutrefresh(win);
}

void ViewCLI::showSignUp() {
    WINDOW* win = getWinMenu();

    werase(win);
    showErrorMessage(getErrorMessage());
    echo();
    curs_set(1);

    box(win, 0, 0);

    const int h = getmaxy(win);
    const int w = getmaxx(win);

    // Titre centré
    const char* title = "Inscription";
    int titleX = (w - (int)std::strlen(title)) / 2;
    if (titleX < 2) titleX = 2;

    wattron(win, A_BOLD);
    mvwprintw(win, 2, titleX, "%s", title);
    wattroff(win, A_BOLD);

    // Deux "input boxes" centrées
    const int boxW = std::min(46, w - 10);
    const int boxH = 3;

    int startX = (w - boxW) / 2;
    int userY  = h / 2 - 4;
    int passY  = h / 2;

    auto drawInputBox = [&](int topY, const char* label) {
        int lx = (w - (int)std::strlen(label)) / 2;
        mvwprintw(win, topY - 1, std::max(2, lx), "%s", label);

        mvwaddch(win, topY, startX, ACS_ULCORNER);
        mvwaddch(win, topY, startX + boxW - 1, ACS_URCORNER);
        mvwaddch(win, topY + boxH - 1, startX, ACS_LLCORNER);
        mvwaddch(win, topY + boxH - 1, startX + boxW - 1, ACS_LRCORNER);

        for (int x = 1; x < boxW - 1; ++x) {
            mvwaddch(win, topY, startX + x, ACS_HLINE);
            mvwaddch(win, topY + boxH - 1, startX + x, ACS_HLINE);
        }
        for (int y = 1; y < boxH - 1; ++y) {
            mvwaddch(win, topY + y, startX, ACS_VLINE);
            mvwaddch(win, topY + y, startX + boxW - 1, ACS_VLINE);
        }

        for (int x = 1; x < boxW - 1; ++x) mvwaddch(win, topY + 1, startX + x, ' ');
    };

    drawInputBox(userY, "Nom d'utilisateur");
    drawInputBox(passY, "Mot de passe");

    wattron(win, A_DIM);
    mvwprintw(win, h - 3, 2, "Entrer : creer le compte");
    wattroff(win, A_DIM);

    refreshScreen();
    wnoutrefresh(win);
}

void ViewCLI::showMenu(MENU_STATE menu) {
    switch (menu) {
        case MENU_STATE::MAIN: {
            showMainMenu(0);
            break;
        }
        case MENU_STATE::LOBBY: {
            showLobbyModify();
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


// Show the menu to change the settings of the lobby
void ViewCLI::showLobbyModify() {
    WINDOW* win = getWinMenu();
    std::shared_ptr<Lobby> lobby = getLobby();
    std::vector<std::string> gameMode = IView::getVectorGameMode();

    basicMenuConfig();
    keypad(win, TRUE);
    curs_set(0);
    noecho();

    const int h = getmaxy(win);
    const int w = getmaxx(win);

    const int listTopY = 6;
    const int listX    = 5;

    int idx = 0;
    const int nChoices = (int)gameMode.size();

    lobby->setIsSetup(false);

    auto drawFrame = [&]() {
        werase(win);
        box(win, 0, 0);

        // Titre centré
        const char* title = "LOBBY - CONFIGURATION";
        int tx = (w - (int)std::strlen(title)) / 2;
        if (tx < 2) tx = 2;

        wattron(win, A_BOLD);
        mvwprintw(win, 1, tx, "%s", title);
        wattroff(win, A_BOLD);

        // Sous-titre
        mvwprintw(win, 3, 3, "Game mode:");

        // Séparateur
        for (int x = 2; x < w - 2; ++x) mvwaddch(win, 4, x, ACS_HLINE);

        // Hint
        wattron(win, A_DIM);
        mvwprintw(win, h - 3, 2, "z/s : naviguer | Entrer : confirmer");
        wattroff(win, A_DIM);
    };

    auto drawList = [&]() {
        // On dessine juste la liste, sans tout effacer
        for (int i = 0; i < nChoices; ++i) {
            // clear ligne
            mvwprintw(win, listTopY + i, 2, "%*s", w - 4, "");

            if (i == idx) {
                wattron(win, COLOR_PAIR(10) | A_BOLD);
                mvwprintw(win, listTopY + i, listX, "%s", gameMode[i].c_str());
                wattroff(win, COLOR_PAIR(10) | A_BOLD);
            } else {
                mvwprintw(win, listTopY + i, listX, "%s", gameMode[i].c_str());
            }
        }
    };

    drawFrame();
    drawList();
    wrefresh(win);

    int c;
    while ((c = wgetch(win)) >= 0) {
        switch (c) {
            case 's':
            case KEY_DOWN:
                idx = (idx + 1) % nChoices;
                drawList();
                wrefresh(win);
                break;

            case 'z':
            case KEY_UP:
                idx = (idx - 1 + nChoices) % nChoices;
                drawList();
                wrefresh(win);
                break;

            case ENTER:
            case 13: {
                lobby->setGameMode(gameMode[idx]);

                // Même logique que ton code :
                // idx 0 -> 1 joueur
                // idx 1 -> 2 joueurs
                // sinon -> choisir le nombre
                if (idx == 0) {
                    lobby->setNumberOfPlayer(1);
                    lobby->setIsSetup(true);
                    return;
                }
                if (idx == 1) {
                    lobby->setNumberOfPlayer(2);
                    lobby->setIsSetup(true);
                    return;
                }

                lobby->setIsSetup(false);
                handleChoiceNumber();        // choisit 3..MAX
                // si handleChoiceNumber() valide -> isSetup=true
                // si ESC -> retour sans valider
                return;
            }

            case ESCAPE:
                return;

            default:
                break;
        }
    }
}

// Handle the choice of the number of players in the lobby
void ViewCLI::handleChoiceNumber() {
    WINDOW* win = getWinMenu();
    std::shared_ptr<Lobby> lobby = getLobby();

    keypad(win, TRUE);
    curs_set(0);
    noecho();

    const int h = getmaxy(win);
    const int w = getmaxx(win);

    // Choix : 3..MAX_PLAYERS_NUMBERS (inclus)
    const int minPlayers = 3;
    const int maxPlayers = MAX_PLAYERS_NUMBERS;
    const int nChoices = (maxPlayers - minPlayers + 1);

    int idx = 0; // 0 => 3 joueurs

    const int listTopY = 7;
    const int listX    = w / 2; // centré approximatif

    auto drawFrame = [&]() {
        // On redessine un écran propre dans le même win
        werase(win);
        box(win, 0, 0);

        const char* title = "CHOISIR NOMBRE DE JOUEURS";
        int tx = (w - (int)std::strlen(title)) / 2;
        if (tx < 2) tx = 2;

        wattron(win, A_BOLD);
        mvwprintw(win, 1, tx, "%s", title);
        wattroff(win, A_BOLD);

        for (int x = 2; x < w - 2; ++x) mvwaddch(win, 3, x, ACS_HLINE);

        wattron(win, A_DIM);
        mvwprintw(win, h - 3, 2, "z/s : naviguer | Entrer : confirmer");
        wattroff(win, A_DIM);

        mvwprintw(win, 5, 3, "Players:");
    };

    auto drawChoices = [&]() {
        for (int i = 0; i < nChoices; ++i) {
            int players = minPlayers + i;

            // clear ligne
            mvwprintw(win, listTopY + i, 2, "%*s", w - 4, "");

            // centrer le nombre
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", players);
            int x = (w - (int)std::strlen(buf)) / 2;
            if (x < 2) x = 2;

            if (i == idx) {
                wattron(win, COLOR_PAIR(10) | A_BOLD);
                mvwprintw(win, listTopY + i, x, "%s", buf);
                wattroff(win, COLOR_PAIR(10) | A_BOLD);
            } else {
                mvwprintw(win, listTopY + i, x, "%s", buf);
            }
        }
    };

    drawFrame();
    drawChoices();
    wrefresh(win);

    int c;
    while ((c = wgetch(win)) >= 0) {
        switch (c) {
            case 's':
            case KEY_DOWN:
                idx = (idx + 1) % nChoices;
                drawChoices();
                wrefresh(win);
                break;

            case 'z':
            case KEY_UP:
                idx = (idx - 1 + nChoices) % nChoices;
                drawChoices();
                wrefresh(win);
                break;

            case ENTER:
            case 13:
                lobby->setNumberOfPlayer(minPlayers + idx);
                lobby->setIsSetup(true);
                return;

            case ESCAPE:
                return;

            default:
                break;
        }
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

void ViewCLI::showMainMenu(int selected) {
    WINDOW* win = getWinMenu();
    if (!win) return;

    basicMenuConfig();

    const int h = getmaxy(win);
    const int w = getmaxx(win);

    const char* items[] = {"PLAY", "INVITATION", "GAME INVITATION", "PROFILE", "RANKING"};
    constexpr int itemCount = 5;

    if (selected < 0) selected = 0;
    if (selected >= itemCount) selected = itemCount - 1;

    const int topY = 6;
    const int gap  = 2;

    auto centerX = [&](const char* s) -> int {
        int x = (w - static_cast<int>(std::strlen(s))) / 2;
        return (x < 2) ? 2 : x;
    };

    auto drawItemLine = [&](int idx, bool isSelected) {
        const char* label = items[idx];
        int y = topY + idx * gap;
        int x = centerX(label);

        // Efface la ligne (dans la fenêtre)
        mvwprintw(win, y, 2, "%*s", w - 4, "");

        if (isSelected) {
            wattron(win, COLOR_PAIR(10) | A_BOLD);
            mvwprintw(win, y, x, "%s", label);
            wattroff(win, COLOR_PAIR(10) | A_BOLD);
        } else {
            wattron(win, A_BOLD);
            mvwprintw(win, y, x, "%s", label);
            wattroff(win, A_BOLD);
        }
    };

    werase(win);
    box(win, 0, 0);

    // Header
    const char* title = "MENU PRINCIPAL";
    wattron(win, A_BOLD);
    mvwprintw(win, 1, centerX(title), "%s", title);
    wattroff(win, A_BOLD);

    // Separator
    for (int x = 2; x < w - 2; ++x) {
        mvwaddch(win, 4, x, ACS_HLINE);
    }

    // Items
    for (int i = 0; i < itemCount; ++i) {
        drawItemLine(i, i == selected);
    }

    // Hint
    wattron(win, A_DIM);
    mvwprintw(win, h - 4, 2, "z/s : naviguer | Entrer : choisir");
    wattroff(win, A_DIM);

    // Refresh
    wrefresh(win);
    refresh();
}

// show the menu with the game invitations
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
    mvprintw(messageY, 0, "Press S to go down");
    mvprintw(messageY + 1, 0, "Press Z to go up");
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
            case 's':
                menu_driver(my_menu, REQ_DOWN_ITEM);
                break;
            case 'z':
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

    for (int i = 0; i < nChoices; ++i) {
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

void ViewCLI::showRankingMenu() {
    clearScreen();

    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    int centerX = xmax / 2;

    int boxWidth = 29;
    int startY = 2;
    int startX = centerX - boxWidth / 2;

    attron(A_BOLD);

    mvaddch(startY, startX, ACS_ULCORNER);
    for (int i = 1; i < boxWidth - 1; ++i)
        mvaddch(startY, startX + i, ACS_HLINE);
    mvaddch(startY,     startX + boxWidth - 1, ACS_URCORNER);

    mvaddch(startY + 1, startX, ACS_VLINE);
    mvprintw(startY + 1, startX + (boxWidth - 11) / 2, "LEADERBOARD");
    mvaddch(startY + 1, startX + boxWidth - 1, ACS_VLINE);

    mvaddch(startY + 2, startX, ACS_LLCORNER);
    for (int i = 1; i < boxWidth - 1; ++i)
        mvaddch(startY + 2, startX + i, ACS_HLINE);
    mvaddch(startY + 2, startX + boxWidth - 1, ACS_LRCORNER);

    attroff(A_BOLD);

    std::shared_ptr<Server> server = getServer();
    std::shared_ptr<Player> player_ = getPlayer();
    showPlayerWithHightScore(player_, server);

    mvprintw(ymax - 5, 0, "Press Esc to return to Main Menu");
    refresh();
}


void ViewCLI::showPlayerWithHightScore(std::shared_ptr<Player> player_,
                                       std::shared_ptr<Server> server) {
    int xmax = getmaxx(stdscr);
    int centerX = xmax / 2;
    PlayerHeader player;
    Social social;

    strcpy(player.username, player_->getName().c_str());
    std::vector<PlayerHeader> players = social.getUsers(server, player);
    
    mvprintw(7, centerX - 15, "USERNAME");
    mvprintw(7, centerX + 5, "HIGH SCORE");

    for (int i = centerX - 15; i < centerX + 15; ++i)
        mvaddch(8, i, ACS_HLINE);

    for (int i = 0; i < int(players.size()); i++) {
        mvprintw(9 + i, centerX - 15, "%s", players[i].username);
        mvprintw(9 + i, centerX + 5, "%d", players[i].highScore);
    }
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
            wattron(newWin, COLOR_PAIR(50));
            mvwprintw(newWin, 1 + (i + 1), 2, "%i", i + 3);
            wattroff(newWin, COLOR_PAIR(50));
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
            wattron(newWin, COLOR_PAIR(50));
            mvwprintw(newWin, 12 + (i + 1), 2, "%s", GAME_MODE[i]);
            wattroff(newWin, COLOR_PAIR(50));
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


void ViewCLI::handleBackSpace(int pos, char ch) {
    int bottomLine = getmaxy(stdscr) - 1;
    mvaddch(bottomLine, 1 + promptLength + pos, ch);
    move(bottomLine, 1 + promptLength + pos);
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

int ViewCLI::showMalusType() {
    int c;
    int idx = 0, base = 3;
    WINDOW* win = getWinMenu();

    std::vector<std::string> malusOptions = {"Commandes bloqués",
                                             "Inversion Commandes",
                                             "Accélération", "Ecran Noir"};
    std::vector<int> malusValues = {2, 3, 4, 5};

    basicMenuConfig();

    for (int i = 0; i < int(malusOptions.size()); i++) {
        mvwprintw(win, base + i, base, "%s", malusOptions[i].c_str());
    }
    wrefresh(win);

    keypad(win, true);
    nodelay(win, false);

    wattron(win, COLOR_PAIR(50));
    mvwprintw(win, base + idx, base, "%s", malusOptions[idx].c_str());
    wattroff(win, COLOR_PAIR(50));
    wrefresh(win);

    while (true) {
        c = wgetch(win);
        switch (c) {
            case 's':
            case 'S':
            case KEY_DOWN:
                mvwprintw(win, base + idx, base, "%s",
                          malusOptions[idx].c_str());
                idx = (idx + 1) % malusOptions.size();
                wattron(win, COLOR_PAIR(50));
                mvwprintw(win, base + idx, base, "%s",
                          malusOptions[idx].c_str());
                wattroff(win, COLOR_PAIR(50));
                break;

            case '\n':
            case '\r':
            case KEY_ENTER:
                clear();
                return malusValues[idx];

            default:
                break;
        }
        wrefresh(win);
    }

    clear();
    return -1;
}

int ViewCLI::showMalusTarget(Game* game_) {
    int c;
    int idx = 0, base = 3;
    WINDOW* win = getWinMenu();

    auto boards = game_->getBoards();
    int myID = game_->getPlayer()->getPlayerId();

    std::vector<int> targetIds;
    for (const auto& [playerID, _] : boards) {
        if (playerID != myID && playerID > 7) {
            targetIds.push_back(playerID);
        }
    }

    if (targetIds.empty()) {
        return -1;
    }

    basicMenuConfig();

    for (int i = 0; i < int(targetIds.size()); i++) {
        std::string username = game_->getPlayerName(targetIds[i]);
        mvwprintw(win, base + i, base, "Joueur : %s", username.c_str());
    }
    wrefresh(win);

    keypad(win, true);
    nodelay(win, false);
    flushinp();

    wattron(win, COLOR_PAIR(50));
    std::string username = game_->getPlayerName(targetIds[idx]);
    mvwprintw(win, base + idx, base, "Joueur : %s", username.c_str());
    wattroff(win, COLOR_PAIR(50));
    wrefresh(win);

    while (true) {
        c = wgetch(win);
        switch (c) {
            case 's':
            case 'S':
            case KEY_DOWN: {
                std::string oldName = game_->getPlayerName(targetIds[idx]);
                mvwprintw(win, base + idx, base, "Joueur : %s",
                          oldName.c_str());

                idx = (idx + 1) % targetIds.size();

                wattron(win, COLOR_PAIR(50));
                std::string newName = game_->getPlayerName(targetIds[idx]);
                mvwprintw(win, base + idx, base, "Joueur : %s",
                          newName.c_str());
                wattroff(win, COLOR_PAIR(50));
                break;
            }

            case '\n':
            case '\r':
            case KEY_ENTER:
                clear();
                return targetIds[idx];

            default:
                break;
        }
        wrefresh(win);
    }

    clear();
    return -1;
}

int ViewCLI::showBonusType() {
    int c;
    int idx = 0, base = 3;
    WINDOW* win = getWinMenu();

    std::vector<std::string> bonusOptions = {"Lotterie", "Ralentissement",
                                             "Tetramino 1x1"};
    std::vector<int> bonusValues = {1, 2, 3};

    basicMenuConfig();

    for (int i = 0; i < int(bonusOptions.size()); i++) {
        mvwprintw(win, base + i, base, "%s", bonusOptions[i].c_str());
    }
    wrefresh(win);

    keypad(win, true);
    nodelay(win, false);

    wattron(win, COLOR_PAIR(10));
    mvwprintw(win, base + idx, base, "%s", bonusOptions[idx].c_str());
    wattroff(win, COLOR_PAIR(10));
    wrefresh(win);

    while (true) {
        c = wgetch(win);
        switch (c) {
            case 's':
            case 'S':
            case KEY_DOWN:
                mvwprintw(win, base + idx, base, "%s",
                          bonusOptions[idx].c_str());
                idx = (idx + 1) % bonusOptions.size();
                wattron(win, COLOR_PAIR(10));
                mvwprintw(win, base + idx, base, "%s",
                          bonusOptions[idx].c_str());
                wattroff(win, COLOR_PAIR(10));
                break;

            case '\n':
            case '\r':
            case KEY_ENTER:
                clear();
                return bonusValues[idx];

            default:
                break;
        }
        wrefresh(win);
    }

    clear();
    return -1;
}

void ViewCLI::showMalus(Game* game) {
    std::vector<std::string> malusList = game->getActiveMalus();

    int marginX = 3;
    int marginY = 4;
    int messageY = marginY + HEIGHT + 2;

    int linesToClear = 5;  // vu qu'il n'y a que max 5 malus
    for (int i = 0; i < linesToClear; ++i) {
        move(messageY + i, 0);
        clrtoeol();
    }

    for (const auto& malus : malusList) {
        mvprintw(messageY++, marginX, "Malus: %s", malus.c_str());
    }
}
