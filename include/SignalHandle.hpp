#pragma once

#include <csignal>

extern volatile sig_atomic_t isInterrupted;

void handleSigInt(int sig);
void handleSignals();