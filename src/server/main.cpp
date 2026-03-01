#include "server.h"

#include <iostream>  

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));
    try {
        Server& server = Server::getInstance();
        server.run();

    } catch (const std::exception& exc) {
        std::cerr << "Error : " << exc.what() << std::endl;
    }
    return 0;
}