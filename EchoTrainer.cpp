#include "EchoTrainer.h"
#include "KochTrainer.h"      // for KOCH_ORDER_FULL / LEN
#include "MorsePlayback.h"    // startTransmission
#include "MorseWebSocket.h"
#include "MorseLookup.h"
#include "Morse_Table.h"      // for MORSE_LOOKUP / MORSE_LOOKUP_LEN
#include "Storage.h"          // for NVS persistence

enum class EchoState : uint8_t {
  Idle,
  WaitingPrompt,   // waiting to send next prompt (delay)
  PlayingPrompt,   // prompt is being sent (playback not necessarily blocking)
  WaitingKey       // waiting for user to key back
};

static EchoState echoState = EchoState::Idle;
static EchoCharSource charSource = EchoCharSource::Koch;
static char expectedChar = '\0';
static unsigned long nextPromptAt = 0;
static uint16_t total = 0;
static uint16_t correct = 0;

static const unsigned long ECHO_TIMEOUT_MS = 15000;
static unsigned long waitKeyStartMs = 0;

// Number of entries in PROGTABLE that are simple printable characters
// (letters A-Z + digits 0-9 + common punctuation).
// We skip prosign entries (indices 42+) because they use special symbols
// that can't be keyed back on a straight key.
static const int FULL_CHAR_COUNT = 42;   // 26 letters + 10 digits + 6 punctuation (,=.?/-)

// Pick a character from Koch order up to current lesson
static char pickCharKoch() {
  int count = getKochActiveCount();
  if (count < 2) count = 2;
  int idx = random(0, count);
  return KOCH_ORDER_FULL[idx];
}

// Pick a character from the full PROGTABLE (letters, numbers, common punctuation)
static char pickCharFull() {
  int idx = random(0, FULL_CHAR_COUNT);
  return (char)pgm_read_byte(&MORSE_LOOKUP[idx].character);
}

// Dispatch to the active character source
static char pickChar() {
  if (charSource == EchoCharSource::Full) {
    return pickCharFull();
  }
  return pickCharKoch();
}

uint16_t getEchoCorrect() { return correct; }
uint16_t getEchoTotal()   { return total; }

static void sendPrompt() {
  expectedChar = pickChar();
  String msg; msg += expectedChar;
  String pattern = getMorseCode(expectedChar);
  wsBroadcastEchoPrompt(expectedChar, pattern, DOT_MS);
  startTransmission(msg);
  echoState = EchoState::PlayingPrompt;
}

void initEchoTrainer() {
  echoState = EchoState::Idle;
  expectedChar = '\0';
  total = correct = 0;
  nextPromptAt = 0;

  // Restore character source from NVS
  if (isStorageReady()) {
    uint8_t saved = getPrefs().getUChar("echoSrc", 0);
    charSource = (saved == 1) ? EchoCharSource::Full : EchoCharSource::Koch;
  }
}

void setEchoCharSource(EchoCharSource src) {
  charSource = src;
  if (isStorageReady()) {
    getPrefs().putUChar("echoSrc", (src == EchoCharSource::Full) ? 1 : 0);
  }
  Serial.printf("Echo char source: %s (saved)\n", getEchoCharSourceName());
  wsBroadcastEchoConfig();
}

EchoCharSource getEchoCharSource() {
  return charSource;
}

const char* getEchoCharSourceName() {
  return (charSource == EchoCharSource::Full) ? "Full" : "Koch";
}

void startEchoTraining() {
  if (echoState != EchoState::Idle) return;
  if (isKochActive()) {
    Serial.println("Stop Koch training first ('koch stop').");
    return;
  }
  if (playback.state != PlaybackState::Idle) {
    Serial.println("Wait for current playback to finish.");
    return;
  }

  // Auto-enable buzzer so the user can hear the prompt and their key echo
  if (!isBuzzerEnabled()) {
    setBuzzerEnabled(true);
    saveBuzzerToNVS();
    wsBroadcastBuzzerState(true);
    Serial.println("Buzzer auto-enabled for echo training.");
  }

  total = correct = 0;
  expectedChar = '\0';
  nextPromptAt = millis();
  echoState = EchoState::WaitingPrompt;
  wsBroadcastEchoSession(true, correct, total);
  Serial.printf("Echo training: START (source: %s)\n", getEchoCharSourceName());
}

void stopEchoTraining() {
  if (echoState == EchoState::Idle) return;
  echoState = EchoState::Idle;
  expectedChar = '\0';
  wsBroadcastEchoSession(false, correct, total);
  Serial.println("Echo training: STOP");
}

bool isEchoActive() {
  return echoState != EchoState::Idle;
}

void echoPrintStats() {
  Serial.printf("Echo stats: %u/%u correct (source: %s)\n",
                correct, total, getEchoCharSourceName());
}

void getEchoStats(uint16_t &outCorrect, uint16_t &outTotal) {
  outCorrect = correct;
  outTotal   = total;
}

void updateEchoTrainer() {
  if (echoState == EchoState::Idle) return;

  unsigned long now = millis();

  if (echoState == EchoState::WaitingPrompt) {
    if (now >= nextPromptAt) {
      sendPrompt();
    }
    return;
  }

  if (echoState == EchoState::PlayingPrompt) {
    if (playback.state != PlaybackState::Idle) {
      return;
    }
    waitKeyStartMs = millis();
    echoState = EchoState::WaitingKey;
    Serial.print("Key it back > ");
    return;
  }

  // WaitingKey: check for timeout
  if (echoState == EchoState::WaitingKey) {
    if (millis() - waitKeyStartMs > ECHO_TIMEOUT_MS) {
      Serial.printf("(timeout - expected %c)\n", expectedChar);
      total++;
      wsBroadcastEchoResult(expectedChar, '?', false, correct, total);
      nextPromptAt = millis() + 400;
      echoState = EchoState::WaitingPrompt;
    }
  }
}

void echoSubmitDecodedChar(char c) {
  if (echoState != EchoState::WaitingKey) return;
  total++;
  bool ok = morseEquivalent(c, expectedChar);
  if (ok) {
    correct++;
    Serial.printf("Echo: OK (%c)\n", c);
  } else {
    Serial.printf("Echo: MISS (got %c expected %c)\n", c, expectedChar);
  }
  wsBroadcastEchoResult(expectedChar, c, ok, correct, total);
  nextPromptAt = millis() + 400;
  echoState = EchoState::WaitingPrompt;
}