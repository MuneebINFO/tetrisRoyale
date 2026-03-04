#include "Signal.h"
#include "Game.h"

// simulateur simple : aucun sigaction ici

Signal::Signal() : sigIntFlag_(0), sigTpstpFlag_(0) {
    // PAS DE sigaction
}

Signal& Signal::getInstance() {
    static Signal instance;
    return instance;
}

bool Signal::getSigIntFlag() const {
    return sigIntFlag_;
}

bool Signal::getSigTstpFlag() const {
    return sigTpstpFlag_;
}

void Signal::signalHandler(int signal) {
    Signal& sig = Signal::getInstance();
    if (signal == SIGINT) {
        sig.sigIntFlag_ = 1;
    } else if (signal == SIGTSTP) {
        sig.sigTpstpFlag_ = 1;
    }
}

// Ajout de simulateurs
void Signal::simulateSIGINT() {
    signalHandler(SIGINT);
}

void Signal::simulateSIGTSTP() {
    signalHandler(SIGTSTP);
}

bool Signal::checkSigTstp(Game& game) {
    if (getSigTstpFlag()) {
        bool newPauseState = !game.getPaused();
        game.setPaused(newPauseState);
        sigTpstpFlag_ = 0;
        return true;
    }
    return false;
}
