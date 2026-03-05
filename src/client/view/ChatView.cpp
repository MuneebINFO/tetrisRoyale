#include "ChatView.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

// -----------------------------
// Layout + rendering helpers
// -----------------------------
namespace {

// Layout constants (simple, no special colors)
constexpr int TITLE_Y = 0;

// Chat box takes the upper part of the screen
constexpr int INPUT_BOX_HEIGHT = 3;      // top border + input line + bottom border
constexpr int MIN_BOX_WIDTH    = 20;

constexpr int PAD_X = 2;                 // inner padding inside boxes

// Message formatting
constexpr int BLOCK_SPACING = 1;         // empty line between message blocks
constexpr int USER_LABEL_INDENT   = 0;   // inside left column

// Behavior you requested
constexpr int MAX_CHAT_MESSAGES = 5;

// Helpers
static int clampi(int v, int lo, int hi) { return std::max(lo, std::min(v, hi)); }

static std::string truncateToFit(const std::string& s, int maxLen) {
    if (maxLen <= 0) return "";
    if ((int)s.size() <= maxLen) return s;
    if (maxLen <= 1) return "…";
    return s.substr(0, maxLen - 1) + "…";
}

static void printCentered(int y, const std::string& text) {
    int w = getmaxx(stdscr);
    int x = (w - (int)text.size()) / 2;
    if (x < 0) x = 0;
    mvprintw(y, x, "%s", text.c_str());
}

static void drawBox(int top, int left, int height, int width) {
    if (height < 2 || width < 2) return;

    int bottom = top + height - 1;
    int right  = left + width - 1;

    mvaddch(top, left, ACS_ULCORNER);
    mvaddch(top, right, ACS_URCORNER);
    mvaddch(bottom, left, ACS_LLCORNER);
    mvaddch(bottom, right, ACS_LRCORNER);

    for (int x = left + 1; x < right; ++x) {
        mvaddch(top, x, ACS_HLINE);
        mvaddch(bottom, x, ACS_HLINE);
    }
    for (int y = top + 1; y < bottom; ++y) {
        mvaddch(y, left, ACS_VLINE);
        mvaddch(y, right, ACS_VLINE);
    }
}

struct ChatLayout {
    int screenH{};
    int screenW{};

    int chatTop{};
    int chatLeft{};
    int chatH{};
    int chatW{};

    int chatInnerTop{};
    int chatInnerLeft{};
    int chatInnerH{};
    int chatInnerW{};

    int inputTop{};
    int inputLeft{};
    int inputH{};
    int inputW{};

    int inputInnerY{};
    int inputInnerX{};
};

static ChatLayout computeLayout() {
    ChatLayout L{};
    L.screenH = getmaxy(stdscr);
    L.screenW = getmaxx(stdscr);

    L.chatLeft = 0;
    L.chatTop  = 1;

    L.inputH = INPUT_BOX_HEIGHT;
    L.inputW = std::max(MIN_BOX_WIDTH, L.screenW);
    L.inputLeft = 0;
    L.inputTop  = std::max(L.chatTop + 2, L.screenH - L.inputH);

    L.chatW = std::max(MIN_BOX_WIDTH, L.screenW);
    L.chatH = std::max(3, L.inputTop - L.chatTop);

    // Inner chat region
    L.chatInnerTop  = L.chatTop + 1;
    L.chatInnerLeft = L.chatLeft + 1 + PAD_X;
    L.chatInnerH    = std::max(0, L.chatH - 2);
    L.chatInnerW    = std::max(0, L.chatW - 2 - 2 * PAD_X);

    // Input inner position
    L.inputInnerY = L.inputTop + 1;               // middle line
    L.inputInnerX = L.inputLeft + 1 + PAD_X;      // after left border + padding

    return L;
}

static void clearRect(int top, int left, int height, int width) {
    if (height <= 0 || width <= 0) return;
    for (int y = 0; y < height; ++y) {
        move(top + y, left);
        for (int x = 0; x < width; ++x) addch(' ');
    }
}

static void drawChatScaffold(const PlayerHeader& friendSelected) {
    const ChatLayout L = computeLayout();

    clear();

    std::string title = std::string("Chat avec ") + friendSelected.username;
    printCentered(TITLE_Y, title);

    drawBox(L.chatTop, L.chatLeft, L.chatH, L.chatW);
    drawBox(L.inputTop, L.inputLeft, L.inputH, L.inputW);

    // Clear inner areas (prevents leftovers)
    clearRect(L.chatInnerTop, L.chatLeft + 1, L.chatInnerH, L.chatW - 2);
    mvprintw(L.inputInnerY, L.inputLeft + 1, "%*s", L.inputW - 2, "");

    mvprintw(L.inputInnerY, L.inputInnerX, "%s", PROMPT_TEXT);

    refresh();
}

static void printLeftBlock(int y, int x, int maxW, const std::string& label, const std::string& msg) {
    mvprintw(y, x, "%s", truncateToFit(label, maxW).c_str());
    mvprintw(y + 1, x, "%s", truncateToFit(msg, maxW).c_str());
}

static void printRightBlock(int y, int leftX, int innerW, const std::string& label, const std::string& msg) {
    auto rightPrint = [&](int yy, const std::string& s) {
        std::string t = truncateToFit(s, innerW);
        int x = leftX + innerW - (int)t.size();
        if (x < leftX) x = leftX;
        mvprintw(yy, x, "%s", t.c_str());
    };

    rightPrint(y, label);
    rightPrint(y + 1, msg);
}

// -----------------------------
// History buffer: keep last 5 messages
// -----------------------------
struct RenderMsg {
    bool isUser;              // true = You (left), false = friend (right)
    std::string username;     // friend username for right side
    std::string message;
};

static std::deque<RenderMsg> g_lastMsgs;

static void pushMsg(const RenderMsg& m) {
    g_lastMsgs.push_back(m);
    while ((int)g_lastMsgs.size() > MAX_CHAT_MESSAGES) g_lastMsgs.pop_front();
}

static void renderLastMsgs() {
    const ChatLayout L = computeLayout();

    clearRect(L.chatInnerTop, L.chatLeft + 1, L.chatInnerH, L.chatW - 2);

    // Each message uses 2 lines + spacing -> messages "remontent" naturally because
    // we always start rendering from the top of the chat area.
    const int blockH = 2 + BLOCK_SPACING;
    int y = L.chatInnerTop;

    for (const auto& m : g_lastMsgs) {
        if (y + 1 >= L.chatInnerTop + L.chatInnerH) break;

        if (m.isUser) {
            const int x = L.chatInnerLeft + USER_LABEL_INDENT;
            const int maxW = std::max(0, L.chatInnerW);
            printLeftBlock(y, x, maxW, "You", m.message);
        } else {
            const int x = L.chatInnerLeft;
            const int w = std::max(0, L.chatInnerW);
            printRightBlock(y, x, w, m.username.empty() ? "friend" : m.username, m.message);
        }

        y += blockH;
    }

    refresh();
}

static int yOfLastRenderedBlock() {
    const ChatLayout L = computeLayout();
    const int blockH = 2 + BLOCK_SPACING;

    int idx = (int)g_lastMsgs.size() - 1;
    if (idx < 0) return L.chatInnerTop;

    int y = L.chatInnerTop + idx * blockH;
    int maxY = L.chatInnerTop + std::max(0, L.chatInnerH - 2);
    return clampi(y, L.chatInnerTop, maxY);
}

} // namespace

// -----------------------------
// Original API
// -----------------------------
void clearScreen() {
    clear();
    refresh();
}

void ChatView::showChatRoom() {
    clearScreen();
    setupChatRoomLayout();
}

void ChatView::setupChatRoomLayout() {
    PlayerHeader friendSelected = player_->getFriendSelected();

    // Configure terminal settings
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    // Draw scaffold
    drawChatScaffold(friendSelected);

    // Load last messages (max 5) and render
    showHistoryChat(friendSelected);

    // Cursor at input area
    const ChatLayout L = computeLayout();
    move(L.inputInnerY, L.inputInnerX + promptLength);
    refresh();
}

void ChatView::showHistoryChat(const PlayerHeader& friendSelected) {
    ChatModel message;
    server_->handleSocialRequest(SOCIAL_TYPE::GET_MESSAGE, friendSelected);
    std::vector<ChatHeader> chatList = message.getMessages(server_);

    g_lastMsgs.clear();

    // Keep only the last MAX_CHAT_MESSAGES messages
    int startIdx = 0;
    if ((int)chatList.size() > MAX_CHAT_MESSAGES) {
        startIdx = (int)chatList.size() - MAX_CHAT_MESSAGES;
    }

    for (int i = startIdx; i < (int)chatList.size(); ++i) {
        const auto& chat = chatList[i];

        RenderMsg rm{};
        if (chat.idPlayer == player_->getPlayerId()) {
            rm.isUser = true;
            rm.username = "You";
        } else {
            rm.isUser = false;
            rm.username = chat.username ? chat.username : "friend";
        }
        rm.message = chat.message ? chat.message : "";

        pushMsg(rm);
    }

    renderLastMsgs();

    // Keep something coherent for old logic (line is no longer used as "growing")
    const ChatLayout L = computeLayout();
    player_->setLine(L.chatInnerTop);
}

void ChatView::displayUserMessage(int /*line*/, const char* message) {
    // Not used anymore as "incremental printing", but keep it functional.
    pushMsg(RenderMsg{true, "You", message ? message : ""});
    renderLastMsgs();
}

void ChatView::displayFriendMessage(int /*line*/, const char* username, const char* message) {
    pushMsg(RenderMsg{false, username ? username : "friend", message ? message : ""});
    renderLastMsgs();
}

void ChatView::refreshChatView(int /*line*/, const std::string& buffer) {
    // Add to buffer and re-render: gives the "messages move up" effect
    pushMsg(RenderMsg{true, "You", buffer});
    renderLastMsgs();

    // Redraw input prompt cleanly
    clearInputArea();

    const ChatLayout L = computeLayout();
    move(L.inputInnerY, L.inputInnerX + promptLength);
    refresh();
}

void ChatView::clearInputArea() {
    const ChatLayout L = computeLayout();

    // Clear inside input box line
    mvprintw(L.inputInnerY, L.inputLeft + 1, "%*s", L.inputW - 2, "");
    mvprintw(L.inputInnerY, L.inputInnerX, "%s", PROMPT_TEXT);

    move(L.inputInnerY, L.inputInnerX + promptLength);
}

void ChatView::showReceiveChatMessage(int /*line*/, const PlayerHeader& msg,
                                      std::shared_ptr<Player> player_) {
    if (std::strcmp(msg.message, "Message sent") == 0) {
        displayMessageSentConfirmation(yOfLastRenderedBlock(), player_->getLastMessageSent());
    } else {
        pushMsg(RenderMsg{false, msg.username ? msg.username : "friend",
                          msg.message ? msg.message : ""});
        renderLastMsgs();
    }

    // Keep cursor in input
    const ChatLayout L = computeLayout();
    move(L.inputInnerY, L.inputInnerX + promptLength);
    refresh();
}

void ChatView::displayMessageSentConfirmation(int line, const std::string& /*lastMessage*/) {
    const ChatLayout L = computeLayout();

    // Put a subtle status on the same block line (right side of the chat box)
    int y = clampi(line, L.chatInnerTop, L.chatInnerTop + std::max(0, L.chatInnerH - 2));

    std::string status = "sent";
    int innerLeft = L.chatLeft + 1;
    int innerW    = L.chatW - 2;

    int x = innerLeft + innerW - (int)status.size() - 2;
    if (x < innerLeft) x = innerLeft;

    attron(A_DIM);
    mvprintw(y, x, "%s", status.c_str());
    attroff(A_DIM);
}

void ChatView::updateCursorAfterNewMessage(int /*line*/) {
    const ChatLayout L = computeLayout();
    move(L.inputInnerY, L.inputInnerX + promptLength);
    refresh();
}

void ChatView::renderChatPrompt(int pos, char ch) {
    const ChatLayout L = computeLayout();
    int x = L.inputInnerX + promptLength + pos;
    int y = L.inputInnerY;

    int leftLimit  = L.inputLeft + 1;
    int rightLimit = L.inputLeft + L.inputW - 2;
    x = clampi(x, leftLimit, rightLimit);

    move(y, x);
    mvaddch(y, x, ch);
}

void ChatView::handleBackSpace(int pos, char ch) {
    const ChatLayout L = computeLayout();
    int x = L.inputInnerX + promptLength + pos;
    int y = L.inputInnerY;

    int leftLimit  = L.inputLeft + 1;
    int rightLimit = L.inputLeft + L.inputW - 2;
    x = clampi(x, leftLimit, rightLimit);

    mvaddch(y, x, ch);
    move(y, x);
}