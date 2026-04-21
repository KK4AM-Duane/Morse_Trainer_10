#ifndef MORSEWEBSOCKET_H
#define MORSEWEBSOCKET_H

// These includes trigger Visual Micro library discovery
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>

#include <Arduino.h>

// Initialize WebSocket and attach to server
void initWebSocket(AsyncWebServer &server);

// Call from loop() to clean up disconnected clients
void cleanupWebSocketClients();

// --- Broadcast helpers (call from anywhere to push data to all clients) ---

// Notify all clients that a character is being transmitted
void wsBroadcastCharSent(char ch, const String &morse);

// Notify all clients that playback started
void wsBroadcastPlaybackStart(const String &message, unsigned int wpm);

// Notify all clients that playback finished
void wsBroadcastPlaybackDone();

// Notify all clients of a speed change
void wsBroadcastSpeedChange(unsigned int wpm);

// Notify all clients with full status
void wsBroadcastStatus();

// --- Freeform (WORD) playback broadcasts ---

// Send a character's morse pattern to the phone before playing it.
// Phone pre-builds audio, ACKs "word_ready", then plays on "word_go".
void wsBroadcastWordChar(char ch, const String &pattern, unsigned int ditMs);

// "Go" signal — phone and buzzer start playing simultaneously.
void wsBroadcastWordGo();

// --- Koch trainer broadcasts ---

// A new challenge character is being played — includes morse pattern
// and dit duration so the phone can play the tone via Web Audio.
void wsBroadcastKochChallenge(uint8_t lesson, uint16_t questionNum,
                               const String &pattern, unsigned int ditMs);

// "Go" signal — phone and buzzer should start playing simultaneously.
// Sent after the phone ACKs "ready" or the timeout expires.
void wsBroadcastKochGo();

// Result of the student's answer
void wsBroadcastKochResult(char expected, char received, bool correct,
                           unsigned long reactionMs,
                           uint16_t sessionCorrect, uint16_t sessionTotal,
                           uint8_t lesson);

// Koch session started / stopped
void wsBroadcastKochSession(bool started, uint8_t lesson, unsigned int wpm);

// Generic Koch event notification (lesson advance, speed change, etc.)
void wsBroadcastKochEvent(const String &text);

// Push the active Koch character list to all clients (call on lesson change)
void wsBroadcastKochChars();

// Notify all clients of buzzer mute state change
void wsBroadcastBuzzerState(bool enabled);

// Notify all clients of Koch pause/resume
void wsBroadcastKochPause(bool paused);

// Echo training broadcasts
void wsBroadcastEchoSession(bool started, uint16_t correct, uint16_t total);
void wsBroadcastEchoPrompt(char expected, const String &pattern, unsigned int ditMs);
void wsBroadcastEchoResult(char expected, char received, bool correct,
                           uint16_t correctCount, uint16_t totalCount);

// Echo configuration broadcast (char source changed)
void wsBroadcastEchoConfig();

// Key type configuration broadcast (straight/iambic changed)
void wsBroadcastKeyConfig();

// Operating mode broadcast (Trainer/CWKeyer hardware switch change)
void wsBroadcastKeyerMode();

// Practice player broadcasts
void wsBroadcastPracticeSession(bool started, uint16_t wordsSent, uint16_t totalWords);
void wsBroadcastPracticeWord(const String &word, uint16_t wordNum, uint16_t totalWords);

// Keyer memory broadcasts — send all 6 memory slots to clients
void wsBroadcastKeyerMemories();

// Real-time keyer TX state — lets the phone play sidetone for manual keying
void wsBroadcastKeyerTx(bool on);

// Send complete keyer memory pattern to phone (single-shot, no per-word handshake)
void wsBroadcastKeyerPlay(const String &pattern, unsigned int ditMs);


#endif