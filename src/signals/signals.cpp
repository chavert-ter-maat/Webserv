#include "signals.hpp"



void sigHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    globalSignalReceived = 1;
}

void    initSignals() {
    signal(SIGINT, sigHandler);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
}