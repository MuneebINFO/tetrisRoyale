#ifndef __CONSTANT_H
#define __CONSTANT_H

#include <cstring>
#include <string>
#include <vector>

constexpr const int PORT = 8080;
constexpr const int MAX_CLIENTS = 9;
constexpr const int MIN_PLAYERS = 2;
constexpr const int WIDTH = 10, HEIGHT = 20;
constexpr const int NUMBER_OF_COLORS = 7;
constexpr const int PLAYERS_NUMBERS[] = {1, 2, 3, 4, 5, 6, 7, 8};
constexpr const int SLEEP_TIME = 1000000;

constexpr const char* PROMPT_TEXT = "> ";
constexpr const int promptLength = 2;
constexpr const int MAX_PLAYERS_NUMBERS = 8;
constexpr const int MODE_NUMBERS = 4;
constexpr const char* GAME_MODE[] = {"Endless", "Dual", "Classic",
                                     "Tetris Royal"};
enum class GAME_MODE { ENDLESS, DUAL, CLASSIC, TETRIS_ROYAL };

constexpr const int BUFFER_SIZE = 1024;
constexpr const int MAX_LENGTH_MESSAGES = 256;
constexpr const int MAX_NAME_LENGTH = 50;

const std::vector<std::vector<std::vector<int>>> SHAPES = {
    {{0, 1, 1}, {1, 1, 0}},  // Z
    {{1, 0, 0}, {1, 1, 1}},  // L
    {{1, 1}, {1, 1}},        // O
    {{1, 1, 0}, {0, 1, 1}},  // S
    {{1, 1, 1, 1}},          // I
    {{0, 0, 1}, {1, 1, 1}},  // J
    {{0, 1, 0}, {1, 1, 1}},  // T
};

enum class MENU_STATE {
    GAME,
    LOBBY,
    MAIN,
    GAME_INVITATION,
    INVITATION,
    PROFILE,
    RANKING,
    CREATING,
    CHATROOM,
};
enum class ACCOUNT_STATE { LOGIN, SIGNUP, ACCOUNT, LOGOUT };

// BASIC COMMAND
constexpr const int SPACE = 32;
constexpr const int ESCAPE = 27;
constexpr const int ENTER = 10;
constexpr const int BACKSPACE = 127;

#endif
