#include "KeyInput.h"
#include "IambicKeyer.h"
#include "MorseLookup.h"
#include "EchoTrainer.h"
#include "Storage.h"
#include "MorseWebSocket.h"    // for wsBroadcastKeyConfig

// ---------- Shared state ----------
static KeyType activeKeyType = KeyType::Straight;

// ---------- Straight-key state (unchanged logic) ----------
static const unsigned long DEBOUNCE_MS = 5;
static const uint8_t MAX_PATTERN_LEN = 8;

static int lastReading = HIGH;
static int keyState    = HIGH;
static unsigned long lastDebounceTime = 0;
static unsigned long keyDownTime = 0;
static unsigned long lastKeyUpTime = 0;
static String currentPattern;

static void pollStraightKey() {
  int reading = digitalRead(KEY_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    if (reading != keyState) {
      keyState = reading;

      if (keyState == LOW) {  // key closed (active-low)
        keyDownTime = millis();
        signalOn();
      } else {                // key opened
        unsigned long pressMs = millis() - keyDownTime;
        signalOff();
        lastKeyUpTime = millis();

        char symbol = (pressMs < (DOT_MS * 2)) ? '.' : '-';
        if (currentPattern.length() < MAX_PATTERN_LEN) {
          currentPattern += symbol;
        }

        Serial.printf("KEY TIME (%lums) -> %c\n", pressMs, symbol);
      }
    }
  }

  // Character gap detection: idle and have a pattern, gap > 3 * DOT
  if (keyState == HIGH && currentPattern.length() > 0) {
    unsigned long idle = millis() - lastKeyUpTime;
    if (idle > CHAR_GAP_MS) {
      char decoded = decodeMorsePattern(currentPattern);
      if (decoded != '\0') {
        Serial.printf("Decoded: %c from %s\n", decoded, currentPattern.c_str());
        echoSubmitDecodedChar(decoded);
      } else {
        Serial.printf("Decoded: ? from %s (not found)\n", currentPattern.c_str());
      }
      currentPattern = "";
    }
  }

  lastReading = reading;
}

// ---------- Public API ----------

void initKeyInput() {
  // Straight key defaults
  keyState = lastReading = digitalRead(KEY_PIN);
  lastDebounceTime = millis();
  keyDownTime = 0;
  lastKeyUpTime = millis();
  currentPattern = "";

  // Iambic keyer init
  initIambicKeyer();

  // Restore key type from NVS
  if (isStorageReady()) {
    uint8_t saved = getPrefs().getUChar("keyType", 0);
    activeKeyType = (saved == 1) ? KeyType::Iambic : KeyType::Straight;
  }
}

void pollKeyInput() {
  if (activeKeyType == KeyType::Iambic) {
    pollIambicKeyer();
  } else {
    pollStraightKey();
  }
}

void setKeyType(KeyType kt) {
  activeKeyType = kt;
  if (isStorageReady()) {
    getPrefs().putUChar("keyType", (kt == KeyType::Iambic) ? 1 : 0);
  }
  // Reset both decoders so no stale state carries over
  keyState = lastReading = digitalRead(KEY_PIN);
  currentPattern = "";
  signalOff();
  initIambicKeyer();
  Serial.printf("Key type: %s (saved)\n", getKeyTypeName());
  wsBroadcastKeyConfig();
}

KeyType getKeyType() {
  return activeKeyType;
}

const char* getKeyTypeName() {
  return (activeKeyType == KeyType::Iambic) ? "Iambic" : "Straight";
}