#pragma once
#define NCURSES_NOMACROS 1
#include <ncursesw/curses.h>

#include "View.h"

/*
 *
 * ViewCLI
 * This class is responsible for displaying the game interface in the
 * terminal.
 * It uses the ncurses library to create a text-based user interface.
 *
 */

class ViewCLI final : public IView {
   private:
    WINDOW* winMenu_;
    WINDOW* winGame_;
    pthread_mutex_t mutexCoutGrid_;
    // WTF is this? +1; +2
    FriendRequestStatus friendRequestStatus_;
    std::string targetPlayerName_ = "";
    std::shared_ptr<Server> server_;

    void showMainMenu() override;
    void showInvitationMenu(std::shared_ptr<Player> player) override;

    void showPlayMenu() override;
    void showRankingMenu() override;
    void setupNcurses();
    void basicMenuConfig();
    void showNoGameInvitation();
    void handleChoiceNumber();

   public:
    ViewCLI();
    ~ViewCLI();

    void clearScreen() override;
    void refreshScreen();

    void showBoards(Game* game) override;
    void showEndScreen(bool won) override;
    void showGame(Game* game) override;
    int showMenuInviteFriendToParty(std::vector<FriendHeader> friends) override;
    void showOptionInviteFriend(LobbyInviteFriend& lobbyFriend);
    void showMenu(MENU_STATE menu) override;
    void showMenuLobby() override;
    void showLobbyModify() override;
    void showLobbyWaitingRoom(bool) override;

    void showErrorMessage(std::string message, int y = 5, int x = 20) override;
    void showMessage(std::string message, int y = 20, int x = 20) override;
    void showCreatingMenu() override;
    void showProfileMenu(std::shared_ptr<Player> play) override;
    void showLogin() override;
    void showSignUp() override;
    void showChoiceNumber(int& idx, int& idx2);
    void showChoiceMode(int& idx2);
    int showGameInvitationMenu(std::vector<LobbyInvitation>) override;
    void showInvitationList(std::vector<FriendHeader> invitations, int& index);
    void showFriendList(std::vector<FriendHeader> friends, int& index);
    void showChatRoom() override;
    void clearChoiceDisplay();
    void updateLobbyView(std::shared_ptr<Lobby> lobby);
    void updateChoiceNumber(int& idx);
    void clearChoiceNumber();
    void clearChoiceMode();
    void setFriendRequestStatus(FriendRequestStatus status,
                                const std::string& playerName = "") {
        friendRequestStatus_ = status;
        if (!playerName.empty()) targetPlayerName_ = playerName;
    }
    WINDOW* getWinGame() { return winGame_; }
    WINDOW* getWinMenu() { return winMenu_; }
    ACCOUNT_STATE showAccountConnection() override;
};
