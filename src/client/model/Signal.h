#ifndef __SIGNAL_H
#define __SIGNAL_H

typedef int sig_atomic_t;

#define SIGINT 2
#define SIGTSTP 20

class Game;

class Signal {
   private:
    Signal();
    ~Signal() = default;
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    static void signalHandler(int signal);

    static Signal* instance_;
    volatile sig_atomic_t sigIntFlag_;
    volatile sig_atomic_t sigTpstpFlag_;

   public:
    static Signal& getInstance();
    [[nodiscard]] bool getSigIntFlag() const;
    [[nodiscard]] bool getSigTstpFlag() const;
    [[nodiscard]] bool checkSigTstp(Game& game);

    void simulateSIGINT();   // ajoutés pour contourner l'absence de sigaction
    void simulateSIGTSTP();
};

#endif
