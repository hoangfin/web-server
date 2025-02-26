#include "SignalHandle.hpp"

volatile sig_atomic_t isInterrupted = 0;

void handleSigInt(int sig){
	(void)sig;
	isInterrupted = 1;
}

void handleSignals(){
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, handleSigInt);
}
