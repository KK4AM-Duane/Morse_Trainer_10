#ifndef MORSECOMMANDS_H
#define MORSECOMMANDS_H

#include <Arduino.h>

// Echo mode
extern bool echoCharacters;

// Command processing functions
void processCommand(String command);
void readSerialInput();

// Command handlers
void showHelp();
void listTable();
void showStatus();
void toggleMode();
void setWPM(int newWPM);
void testHardware();
bool parseCharCode(const String &input);
void handleRealtimeEcho(char c);

#endif