#include "CWKeyerMode.h"
#include "MorsePlayback.h"    // signalOn/Off, DOT_MS, DASH_MS, CHAR_GAP_MS
#include "MorseLookup.h"      // MorseMode, currentMode, getMorseCode
#include "KeyInput.h"         // getKeyType, KEY_PIN, PADDLE_DAH_PIN
#include "KochTrainer.h"      // isKochActive, stopKochSession
#include "EchoTrainer.h"      // isEchoActive, stopEchoTraining
#include "MorseWebSocket.h"   // broadcast mode change
#include "Storage.h"          // NVS persistence

// ============================================================
// Hardware mode switch on GPIO18 (INPUT_PULLUP)
//   HIGH (or floating with pull-up) = Trainer mode
//   LOW  (switch closed to GND)     = CW Keyer mode
//
// TX relay output on GPIO23
//   HIGH = key down (relay closed, transmitter keys)
//   LOW  = key up
//
// In CW Keyer mode the iambic / straight key state machine
// still generates sidetone (signalOn/Off), but additionally
// mirrors key-down to TX_KEY_PIN.
// ============================================================

static OperatingMode opMode = OperatingMode::Trainer;

// Debounce for the hardware switch
static const unsigned long MODE_DEBOUNCE_MS = 50;
static int lastSwitchReading = HIGH;
static int switchState = HIGH;
static unsigned long lastSwitchDebounceMs = 0;

// --- CW Keyer passthrough state machine ---
static const unsigned long KEY_DEBOUNCE_MS = 5;
static int txLastReading = HIGH;
static int txKeyState = HIGH;
static unsigned long txDebounceMs = 0;

// Track whether TX signal is currently asserted
static bool txActive = false;

// --- Keyer memory slots (M1–M6) ---
static String keyerMem[KEYER_MEM_COUNT];

// Keyer memory playback: we re-use the global playback engine with
// echoMode=true (skips phone audio handshake) and mirror signal
// state to TX_KEY_PIN in the poller.
static bool keyerMemPlaying = false;

static void txOn() {
  if (!txActive) {
    txActive = true;
    digitalWrite(TX_KEY_PIN, HIGH);
    signalOn();   // sidetone + LED for operator feedback
  }
}

static void txOff() {
  if (txActive) {
    txActive = false;
    digitalWrite(TX_KEY_PIN, LOW);
    signalOff();
  }
}

// --- Straight key passthrough (debounced GPIO32 ? GPIO23) ---
static void pollStraightKeyTx() {
  int reading = digitalRead(KEY_PIN);
  if (reading != txLastReading) {
    txDebounceMs = millis();
  }
  if ((millis() - txDebounceMs) > KEY_DEBOUNCE_MS) {
    if (reading != txKeyState) {
      txKeyState = reading;
      if (txKeyState == LOW) {
        txOn();
      } else {
        txOff();
      }
    }
  }
  txLastReading = reading;
}

// --- Iambic keyer passthrough (full Mode B ? GPIO23) ---
static void pollIambicKeyerTx() {
  extern void pollIambicKeyer();
  pollIambicKeyer();

  // Mirror: if LED is on ? TX relay on, else off
  bool ledOn = (digitalRead(LED_PIN) == HIGH);
  if (ledOn && !txActive) {
    txActive = true;
    digitalWrite(TX_KEY_PIN, HIGH);
  } else if (!ledOn && txActive) {
    txActive = false;
    digitalWrite(TX_KEY_PIN, LOW);
  }
}

// --- Keyer memory playback: mirror playback signal to TX ---
static void pollKeyerMemPlayback() {
  if (!keyerMemPlaying) return;

  // Mirror LED_PIN state ? TX_KEY_PIN (playback engine drives LED)
  bool ledOn = (digitalRead(LED_PIN) == HIGH);
  if (ledOn && !txActive) {
    txActive = true;
    digitalWrite(TX_KEY_PIN, HIGH);
  } else if (!ledOn && txActive) {
    txActive = false;
    digitalWrite(TX_KEY_PIN, LOW);
  }

  // Check if playback finished
  if (playback.state == PlaybackState::Idle) {
    keyerMemPlaying = false;
    txActive = false;
    digitalWrite(TX_KEY_PIN, LOW);
  }
}

// --- Transition helpers ---

static void enterCWKeyerMode() {
  // Stop any active training sessions
  if (isKochActive())  stopKochSession();
  if (isEchoActive())  stopEchoTraining();

  // Kill any active buzzer/LED signal from trainer playback
  signalOff();

  // Ensure TX output starts LOW
  digitalWrite(TX_KEY_PIN, LOW);
  txActive = false;
  keyerMemPlaying = false;

  // Reset straight-key passthrough state
  txLastReading = digitalRead(KEY_PIN);
  txKeyState = txLastReading;
  txDebounceMs = millis();

  opMode = OperatingMode::CWKeyer;
  Serial.println("\n>>> CW Keyer mode ACTIVE (TX on GPIO23)");
  wsBroadcastKeyerMode();
}

static void enterTrainerMode() {
  // Stop any keyer memory playback
  if (keyerMemPlaying) {
    keyerMemPlaying = false;
    if (playback.state != PlaybackState::Idle) {
      signalOff();
      playback.state = PlaybackState::Idle;
    }
  }

  // Ensure TX output is off
  digitalWrite(TX_KEY_PIN, LOW);
  txActive = false;
  signalOff();

  opMode = OperatingMode::Trainer;
  Serial.println("\n>>> Trainer mode ACTIVE");
  wsBroadcastKeyerMode();
}

// --- Public API ---

void initCWKeyerMode() {
  // Start in whatever state the switch is in
  switchState = digitalRead(MODE_SWITCH_PIN);
  lastSwitchReading = switchState;
  lastSwitchDebounceMs = millis();

  // TX output starts low
  digitalWrite(TX_KEY_PIN, LOW);
  txActive = false;
  keyerMemPlaying = false;

  if (switchState == LOW) {
    opMode = OperatingMode::CWKeyer;
    Serial.println("Mode switch: CW Keyer");
  } else {
    opMode = OperatingMode::Trainer;
    Serial.println("Mode switch: Trainer");
  }

  // Load keyer memories from NVS
  loadKeyerMemories();
}

void pollModeSwitch() {
  int reading = digitalRead(MODE_SWITCH_PIN);

  if (reading != lastSwitchReading) {
    lastSwitchDebounceMs = millis();
  }

  if ((millis() - lastSwitchDebounceMs) > MODE_DEBOUNCE_MS) {
    if (reading != switchState) {
      switchState = reading;
      if (switchState == LOW) {
        enterCWKeyerMode();
      } else {
        enterTrainerMode();
      }
    }
  }

  lastSwitchReading = reading;

  // If in CW Keyer mode, run the passthrough keyer + memory playback
  if (opMode == OperatingMode::CWKeyer) {
    // Memory playback takes priority over manual keying
    if (keyerMemPlaying) {
      pollKeyerMemPlayback();
    } else {
      if (getKeyType() == KeyType::Iambic) {
        pollIambicKeyerTx();
      } else {
        pollStraightKeyTx();
      }
    }
  }
}

OperatingMode getOperatingMode() {
  return opMode;
}

const char* getOperatingModeName() {
  return (opMode == OperatingMode::CWKeyer) ? "CWKeyer" : "Trainer";
}

bool isCWKeyerMode() {
  return opMode == OperatingMode::CWKeyer;
}

// ?? Keyer Memory ???????????????????????????????????????????????????

void loadKeyerMemories() {
  if (!isStorageReady()) return;
  Preferences& p = getPrefs();
  for (int i = 0; i < KEYER_MEM_COUNT; i++) {
    String key = "kmem" + String(i);
    keyerMem[i] = p.getString(key.c_str(), "");
    if (keyerMem[i].length() > 0) {
      Serial.printf("  Keyer M%d: \"%s\" (%d chars)\n",
                    i + 1, keyerMem[i].c_str(), (int)keyerMem[i].length());
    }
  }
}

void setKeyerMemory(int slot, const String &text) {
  if (slot < 0 || slot >= KEYER_MEM_COUNT) return;
  String trimmed = text;
  trimmed.trim();
  if ((int)trimmed.length() > KEYER_MEM_MAX_LEN) {
    trimmed = trimmed.substring(0, KEYER_MEM_MAX_LEN);
  }
  trimmed.toUpperCase();
  keyerMem[slot] = trimmed;

  if (isStorageReady()) {
    String key = "kmem" + String(slot);
    getPrefs().putString(key.c_str(), keyerMem[slot]);
  }
  Serial.printf(">>> Keyer M%d saved: \"%s\" (%d chars)\n",
                slot + 1, keyerMem[slot].c_str(), (int)keyerMem[slot].length());
}

String getKeyerMemory(int slot) {
  if (slot < 0 || slot >= KEYER_MEM_COUNT) return "";
  return keyerMem[slot];
}

void playKeyerMemory(int slot) {
  if (slot < 0 || slot >= KEYER_MEM_COUNT) return;
  if (keyerMem[slot].length() == 0) {
    Serial.printf(">>> Keyer M%d is empty\n", slot + 1);
    return;
  }
  if (!isCWKeyerMode()) {
    Serial.println(">>> Keyer memory playback only in CW Keyer mode");
    return;
  }
  if (playback.state != PlaybackState::Idle) {
    Serial.println(">>> Playback busy — stop first");
    return;
  }

  Serial.printf(">>> Keyer M%d playing: \"%s\"\n", slot + 1, keyerMem[slot].c_str());

  // Use Progtable for full character support
  MorseMode savedMode = currentMode;
  currentMode = MorseMode::Progtable;

  // Start transmission in echoMode=true to skip phone audio handshake.
  // The playback engine drives signalOn/Off which we mirror to TX_KEY_PIN
  // in pollKeyerMemPlayback().
  keyerMemPlaying = true;
  startTransmission(keyerMem[slot], true);

  currentMode = savedMode;
}

bool isKeyerMemoryPlaying() {
  return keyerMemPlaying;
}

void stopKeyerMemoryPlayback() {
  if (!keyerMemPlaying) return;
  keyerMemPlaying = false;
  if (playback.state != PlaybackState::Idle) {
    signalOff();
    playback.state = PlaybackState::Idle;
  }
  digitalWrite(TX_KEY_PIN, LOW);
  txActive = false;
  Serial.println(">>> Keyer memory playback stopped");
}