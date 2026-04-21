#ifndef MORSECOMMANDS_H
#define MORSECOMMANDS_H

#include <Arduino.h>

// Command processing functions
void processCommand(String command);
void readSerialInput();

// Command handlers
void showHelp();
void showStatus();
void toggleMode();
void setWPM(int newWPM);

#endif