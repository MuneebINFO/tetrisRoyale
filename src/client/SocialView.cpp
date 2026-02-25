#include "SocialView.h"


SocialView::SocialView() : signal_(Signal::getInstance()) {}

void SocialView::clearScreen() {
    viewCLI_->clearScreen();
}

void SocialView::showFriendList(std::vector<PlayerHeader> friendsList, int& idx) {
    WINDOW* newWin = viewCLI_->getWinMenu();
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
    viewCLI_->refreshScreen();
}


void SocialView::showInvitationMenu(std::shared_ptr<Player> player__) {
    WINDOW* newWin = viewCLI_->getWinMenu();
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    viewCLI_->basicMenuConfig();
    mvwprintw(newWin, 0, 2, "Invitation MENU");
    Social social;
    PlayerHeader player;
    player.idPlayer = player__->getPlayerId();
    strcpy(player.username, player__->getName().c_str());
    social.refreshFriendRequest(viewCLI_->getServer(), player__, player);
    for (int i = 0; i < int(player__->getVectorInvitations().size()); i++) {
        int invitingPlayerID = player__->getVectorInvitations()[i].idPlayer;
        char invitingPlayerName[MAX_NAME_LENGTH];
        strcpy(invitingPlayerName, player__->getVectorInvitations()[i].username);
        mvwprintw(newWin, 1 + i, 1, "Invitation from : %s with ID %d", invitingPlayerName, invitingPlayerID);
    }
    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press a to choose an invitation");
    mvprintw(messageY + 1, 0, "Press r to refresh the list");
    mvprintw(messageY + 2, 0, "Press Enter to accept the invitation");
    mvprintw(messageY + 3, 0, "Press Esc to return to Main Menu");
    viewCLI_->refreshScreen();
}

void SocialView::showInvitationList(std::vector<PlayerHeader> invitations, int& idx) {
    WINDOW* newWin = viewCLI_->getWinMenu();
    mvwprintw(newWin, 0, 2, "Invitation MENU");
    if (invitations.size() > 0) {
        for (int i = 0; i < int(invitations.size()); i++) {
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
        mvprintw(0, 1, "Cannot select an invitation, no invitation received sadge :(");
    }
    viewCLI_->refreshScreen();
}

void SocialView::showProfileMenu(std::shared_ptr<Player> player) {
    viewCLI_->basicMenuConfig();
    WINDOW* win = viewCLI_->getWinMenu();
    mvwprintw(win, 0, 2, "PROFILE MENU");
    mvwprintw(win, 2, 2, "Friends:");
    int ymax, xmax;
    getmaxyx(stdscr, ymax, xmax);
    PlayerHeader you = player->getPlayer();

    mvprintw(0, 1, "                                                   ");
    switch (friendRequestStatus_) {
        case FRIEND_REQUEST_STATUS::FRIEND_REQUEST_SENT:
            mvprintw(0, 1, "Friend request sent to %s!",
                     targetPlayerName_.c_str());
            break;
        case FRIEND_REQUEST_STATUS::ALREADY_IN_LIST:
            mvprintw(0, 1, "Player already in the list!");
            break;
        case FRIEND_REQUEST_STATUS::SELF_ADD_FORBIDDEN:
            mvprintw(0, 1, "You cannot add yourself as a friend!");
            break;
        case FRIEND_REQUEST_STATUS::PLAYER_NOT_FOUND:
            mvprintw(0, 1, "%s not found!", targetPlayerName_.c_str());
            break;
        case FRIEND_REQUEST_STATUS::NO_FRIENDS:
            mvprintw(0, 1, "No friends found!");
            break;
        case FRIEND_REQUEST_STATUS::FRIEND_REQUEST_ALREADY_SENT:
            mvprintw(0, 1, "Friend request already sent to %s!", targetPlayerName_.c_str());
            break;
        case FRIEND_REQUEST_STATUS::NONE:
            break;
        default:
            break;
    }
    const char* msg = "Always refresh the list before adding a friend";
    int centerX = (xmax - static_cast<int>(strlen(msg))) / 2;
    mvprintw(8, centerX, "%s", msg);

    Social social;
    std::shared_ptr<Server> server = viewCLI_->getServer();
    std::shared_ptr<Player> player = viewCLI_->getPlayer();
    social.refreshFriendList(server, player, you);

    for (int i = 0; i < int(player->getVectorFriends().size()); i++) {
        mvwprintw(win, 3 + i, 2, "%s with ID %i", player->getVectorFriends()[i].username, player->getVectorFriends()[i].idPlayer);
    }

    int messageY = ymax - 5;  // Afficher les messages près du bas de l'écran
    mvprintw(messageY, 0, "Press a to add a friend");
    mvprintw(messageY + 1, 0, "Press b to remove a friend");
    mvprintw(messageY + 2, 0, "Press r to refresh the list");
    mvprintw(messageY + 3, 0, "Press k to select a friend then Enter to chat with him or d to delete him");
    mvprintw(messageY + 4, 0, "Press Esc to return to Main Menu");

    viewCLI_->refreshScreen();
}
