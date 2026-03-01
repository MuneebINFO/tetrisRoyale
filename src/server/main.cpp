#include "server.h"

#include <iostream>  

int main() {
    try {
        Server& server = Server::getInstance();
        server.run();

    //si une exception est levé -> affiche un message d'erreur
    } catch (const std::exception& exc) {
        std::cerr << "Error : " << exc.what() << std::endl;
    }
    return 0;
}