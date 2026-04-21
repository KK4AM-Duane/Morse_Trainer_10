#ifndef KOCHTRAINER_H
#define KOCHTRAINER_H

#include <Arduino.h>
#include "config.h"

// ---------- Constants ----------
static const int   KOCH_MAX_CHARS       = 50;   // 44 base + 6 prosigns (AR BT SK BK AS KN)
static const int   EVAL_WINDOW          = 10;   // evaluate every N characters
static const float ADVANCE_THRESHOLD    = 0.90f;
static const float SPEED_UP_THRESHOLD   = 0.90f;
static const float SPEED_DOWN_THRESHOLD = 0.70f;
static const int   REACTION_FAST_MS     = 3000;  // "fast" reaction threshold
static const int   MIN_WPM_KOCH         = 18;
static const int   MAX_WPM_KOCH         = 50;
static const uint8_t PROB_INITIAL       = 10;
static const uint8_t PROB_MIN           = 2;
static const uint8_t PROB_MAX           = 50;
static const uint8_t PROB_NEW_CHAR      = 20;
static const uint8_t PROB_WRONG_DELTA   = 5;
static const uint8_t PROB_RIGHT_DELTA   = 1;
static const int     SPEED_UP_DELTA     = 2;
static const int     SPEED_DOWN_DELTA   = 3;
static const uint8_t LOST_TIMEOUT_THRESHOLD = 3;   // consecutive timeouts before Lost
static const unsigned long LOST_PAUSE_MS    = 2000; // recovery pause in Lost state
static const uint16_t CHECKPOINT_INTERVAL   = 50;   // periodic NVS save every N chars

// Max time (ms) to wait for phone "ready" ACK before starting anyway.
// If no phone is connected, the timeout ensures the buzzer still plays.
static const unsigned long READY_TIMEOUT_MS = 400;

// Consecutive correct answers at max lesson before grid sorts alphabetically
static const uint16_t SORT_GRID_STREAK = 10;

// ---------- Koch character order ----------
// Standard 44 characters + prosigns: AR(^), BT([), SK(|), BK(>), AS(<), KN($)
// Prosign characters use the same mapping as preprocessProsigns()
extern const char KOCH_ORDER_FULL[];
extern const int  KOCH_ORDER_LEN;

// ---------- Training state (non-blocking) ----------
enum class KochState : uint8_t {
  Idle,             // Koch mode not active
  SelectAndPlay,    // Pick a character, start playback
  WaitReady,        // Challenge sent to phone, waiting for "ready" ACK
  WaitPlayback,     // Morse engine is playing the character
  WaitResponse,     // Waiting for user answer (serial or WebSocket)
  ShowResult,       // Brief pause after showing result before next char
  Paused,           // Session frozen — resume returns to SelectAndPlay
  Lost              // User fell behind — auto-slow and brief recovery pause
};

// ---------- Public API ----------

// Lifecycle
void initKochTrainer();     // Call once from setup()
void updateKochTrainer();   // Call every loop() iteration
void startKochSession();    // Enter Koch training mode
void stopKochSession();     // Leave Koch training mode
void pauseKochSession();    // Freeze the session
void resumeKochSession();   // Continue from where we left off
bool isKochActive();        // True if a session is running (includes Paused/Lost)
bool isKochPaused();        // True only when Paused

// Scoring — called from serial input handler AND WebSocket handler
void kochSubmitAnswer(char answer);

// Handshake — called by WebSocket when phone sends {"type":"koch_ready"}
void kochPhoneReady();

// Replay — re-play the current challenge character on the buzzer.
// Called by WebSocket when phone sends {"type":"koch_replay"}.
void kochReplayChallenge();

// Accessors for status display
uint8_t          getKochLesson();
uint16_t         getKochSessionCorrect();
uint16_t         getKochSessionTotal();
const CharStats* getKochCharStats();
int              getKochActiveCount();

// True when user has completed all lessons and sustained a streak of
// SORT_GRID_STREAK correct answers — phone should show grid alphabetically.
bool isKochGridSorted();

// Character selection (exposed for testing)
char selectKochCharacter();

// Reset all Koch progress (lesson, stats, WPM) to defaults and clear NVS
void resetKochProgress();

// Persistent storage
void saveKochToNVS(bool force = false); // Save lesson + charStats + WPM
void loadKochFromNVS();                 // Load from NVS (falls back to defaults)

#endif