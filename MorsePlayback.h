#ifndef MORSEPLAYBACK_H
#define MORSEPLAYBACK_H

#include <Arduino.h>

// External GPIO pin declarations (defined in .ino)
extern const unsigned int LED_PIN;
extern const unsigned int BUZZER_PIN;
extern const unsigned int LED_GREEN_PIN;
extern const unsigned int LED_RED_PIN;
extern const int PWM_DUTY_CYCLE;

// Non-blocking playback state machine
enum class PlaybackState : uint8_t {
  Idle,
  WaitPhoneReady,  // Sent char pattern to phone, waiting for ACK
  SignalOn,
  ElementGap,      // Gap between dits/dahs
  InternalCharGap, // Gap within compound codes (like % = 0/0)
  CharGap,
  WordGap
};

struct PlaybackContext {
  PlaybackState state = PlaybackState::Idle;
  String message = "";
  String currentMorseCode = "";
  int messageIndex = 0;
  int codeIndex = 0;
  unsigned long stateStartTime = 0;
  unsigned long stateDuration = 0;
  bool echoMode = false;
};

// Global playback context
extern PlaybackContext playback;

// Timing variables
extern unsigned int WPM;
extern unsigned int DOT_MS;
extern unsigned int DASH_MS;
extern unsigned int ELEMENT_GAP_MS;
extern unsigned int CHAR_GAP_MS;
extern unsigned int WORD_GAP_MS;

// Max time (ms) to wait for phone "word_ready" ACK before starting buzzer
static const unsigned long WORD_READY_TIMEOUT_MS = 400;

// Hardware control functions
void signalOn();
void signalOff();
void updateTiming();

// Buzzer mute control (LED still blinks during playback)
void setBuzzerEnabled(bool enabled);
bool isBuzzerEnabled();

// Buzzer state persistence
void saveBuzzerToNVS();
bool loadBuzzerFromNVS();

// Playback functions
void updatePlayback();
void startTransmission(const String &message, bool echo = false, bool skipProsigns = false);

// Phone handshake for freeform playback — called by WebSocket on "word_ready"
void wordPhoneReady();

// Clear pending phone handshake (used by force-stop)
void clearWordReadyFlag();

#endif