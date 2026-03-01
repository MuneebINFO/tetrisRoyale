#pragma once
#define NCURSES_NOMACROS 1
#include <ncursesw/curses.h>

#include "Social.h"
#include "View.h"

/*
 *
 * ViewCLI
 * This class is responsible for displaying the game interface in the
 * terminal.
 * It uses the ncurses library to create a text-based user interface.
 *
 */

class ControllerCLI;
class Social;
class ViewCLI final : public IView {
   private:
    WINDOW* winMenu_;
    WINDOW* winGame_;
    pthread_mutex_t mutexCoutGrid_;
    FRIEND_REQUEST_STATUS friendRequestStatus_;
    std::string targetPlayerName_ = "";
    std::shared_ptr<Server> server_;

    void showMainMenu() override;

    void showPlayMenu() override;
    void showRankingMenu() override;
    void setupNcurses();
    void showNoGameInvitation();
    void handleChoiceNumber();

   public:
    ViewCLI();
    ~ViewCLI();

    void clearScreen() override;
    void refreshScreen();
    void basicMenuConfig();

    void showBoards(Game* game) override;
    void showEndScreen(bool won) override;
    void showGame(Game* game) override;
    int showMenuInviteFriendToParty(std::vector<PlayerHeader> friends) override;
    void showOptionInviteFriend(LobbyInviteFriend& lobbyFriend);
    void showMenu(MENU_STATE menu) override;
    void showMenuLobby() override;
    void showLobbyModify() override;
    void showLobbyWaitingRoom(bool) override;
    int showMalusType() override;
    int showBonusType() override;
    int showMalusTarget(Game* game_) override;
    void showMalus(Game* game) override;

    void showErrorMessage(std::string message, int y = 5, int x = 20) override;
    void showMessage(std::string message, int y = 20, int x = 20) override;
    void showCreatingMenu() override;
    void showLogin() override;
    void showSignUp() override;
    void showChoiceNumber(int& idx, int& idx2);
    void showChoiceMode(int& idx2);
    int showGameInvitationMenu(std::vector<LobbyInvitation>) override;
    void clearChoiceDisplay();
    void handleBackSpace(int pos, char ch);
    void updateLobbyView(std::shared_ptr<Lobby> lobby);
    void updateChoiceNumber(int& idx);
    void clearChoiceNumber();
    void clearChoiceMode();
    void showPlayerWithHightScore(std::shared_ptr<Player> player_,
                                  std::shared_ptr<Server> server);
    void setFriendRequestStatus(FRIEND_REQUEST_STATUS status,
                                const std::string& playerName = "") {
        friendRequestStatus_ = status;
        if (!playerName.empty()) targetPlayerName_ = playerName;
    }
    WINDOW* getWinGame() { return winGame_; }
    WINDOW* getWinMenu() { return winMenu_; }
    ACCOUNT_STATE showAccountConnection() override;
};
