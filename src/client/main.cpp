#include <ncurses.h>

#include <csignal>
#include <cstdlib>
#include <ctime>

#include "Game.h"
#include "Player.h"
#include "Signal.h"
#include "Tetris.h"
Signal* Signal::instance_ = nullptr;

int main() {
    Signal::getInstance();
    Tetris tetris;
    tetris.run();
    endwin();
    return EXIT_SUCCESS;
}
