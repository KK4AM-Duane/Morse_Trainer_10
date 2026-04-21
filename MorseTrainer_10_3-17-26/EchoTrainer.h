#pragma once
#include <Arduino.h>

// Character source for echo training prompts
enum class EchoCharSource : uint8_t {
  Koch,      // Current Koch lesson characters only
  Full       // All characters from PROGTABLE (letters, numbers, punctuation)
};

// Lifecycle
void initEchoTrainer();      // call from setup
void updateEchoTrainer();    // call each loop
void startEchoTraining();
void stopEchoTraining();
void echoSubmitDecodedChar(char c); // called when user keyed a character

// Character source selection (persisted to NVS)
void setEchoCharSource(EchoCharSource src);
EchoCharSource getEchoCharSource();
const char* getEchoCharSourceName();   // "Koch" or "Full"

// Status
bool isEchoActive();
void echoPrintStats();
void getEchoStats(uint16_t &outCorrect, uint16_t &outTotal);
uint16_t getEchoCorrect();
uint16_t getEchoTotal();