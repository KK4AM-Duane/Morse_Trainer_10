#pragma once
#include <Arduino.h>

// Practice text playback — plays text one character at a time using the
// existing non-blocking MorsePlayback engine, with word-at-a-time pacing.
//
// Sources: serial inline, LittleFS /practice.txt, or WebSocket push.

// Lifecycle
void initPracticePlayer();      // call from setup (after LittleFS mount)
void updatePracticePlayer();    // call from loop

// Start practice from a string (serial inline or WebSocket)
void startPractice(const String &text);

// Start practice from LittleFS file /practice.txt
bool startPracticeFromFile();

// Stop practice playback
void stopPractice();

// Status
bool isPracticeActive();
void practicePrintStats();

// Stats accessors for WebSocket
uint16_t getPracticeWordsSent();
uint16_t getPracticeTotalWords();
const String& getPracticeCurrentWord();