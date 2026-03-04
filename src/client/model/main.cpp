#include <cstdlib>

#include "Tetris.h"

int main(int argc, char* argv[]) {
    Tetris tetris;
    tetris.init();
    tetris.run();
    
    return EXIT_SUCCESS;
}
