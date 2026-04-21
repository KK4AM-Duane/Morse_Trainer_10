#include "CWKeyerMode.h"        // Manually entered
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
// echoMode=true (no per-char phone handshake) and mirror signal
// state to TX_KEY_PIN in the poller.
static bool keyerMemPlaying = false;

// ── TX helpers ──────────────────────────────────────────────

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

// ── Straight key TX passthrough ─────────────────────────────

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
            }
            else {
                txOff();
            }
        }
    }

    txLastReading = reading;
}

// ── Iambic keyer TX passthrough ─────────────────────────────
// Re-implements the iambic Mode B state machine but mirrors
// signal state to TX_KEY_PIN and broadcasts to WebSocket.

enum class IambicTxState : uint8_t {
    Idle,
    SendingDit,
    SendingDah,
    ElementGap
};

static IambicTxState itState = IambicTxState::Idle;
static unsigned long itElementStartMs = 0;
static unsigned long itLastElementEndMs = 0;
static bool itDitLatch = false;
static bool itDahLatch = false;
static char itLastElement = '\0';

static inline bool itDitPressed() { return digitalRead(KEY_PIN) == LOW; }
static inline bool itDahPressed() { return digitalRead(PADDLE_DAH_PIN) == LOW; }

static void itStartDit() {
    itState = IambicTxState::SendingDit;
    itElementStartMs = millis();
    txOn();
    itDitLatch = false;
    itDahLatch = false;
    itLastElement = '.';
}

static void itStartDah() {
    itState = IambicTxState::SendingDah;
    itElementStartMs = millis();
    txOn();
    itDitLatch = false;
    itDahLatch = false;
    itLastElement = '-';
}

static void itStartGap() {
    txOff();
    itState = IambicTxState::ElementGap;
    itElementStartMs = millis();
}

static void pollIambicKeyerTx() {
    unsigned long now = millis();

    switch (itState) {

    case IambicTxState::Idle: {
        bool dit = itDitPressed();
        bool dah = itDahPressed();
        if (dit && dah) {
            itStartDit();
        }
        else if (dit) {
            itStartDit();
        }
        else if (dah) {
            itStartDah();
        }
        break;
    }

    case IambicTxState::SendingDit: {
        if (itDahPressed()) itDahLatch = true;
        if ((now - itElementStartMs) >= DOT_MS) {
            itStartGap();
        }
        break;
    }

    case IambicTxState::SendingDah: {
        if (itDitPressed()) itDitLatch = true;
        if ((now - itElementStartMs) >= DASH_MS) {
            itStartGap();
        }
        break;
    }

    case IambicTxState::ElementGap: {
        if (itDitPressed()) itDitLatch = true;
        if (itDahPressed()) itDahLatch = true;

        if ((now - itElementStartMs) >= DOT_MS) {
            itLastElementEndMs = millis();

            bool dit = itDitPressed();
            bool dah = itDahPressed();

            if (itDitLatch && itDahLatch) {
                if (itLastElement == '.') itStartDah(); else itStartDit();
            }
            else if (itDitLatch) { itStartDit(); }
            else if (itDahLatch) { itStartDah(); }
            else if (dit && dah) {
                if (itLastElement == '.') itStartDah(); else itStartDit();
            }
            else if (dit) { itStartDit(); }
            else if (dah) { itStartDah(); }
            else {
                itState = IambicTxState::Idle;
            }
        }
        break;
    }
    }
}

// ── Keyer memory playback poller ────────────────────────────
// Mirrors the playback engine's signal state to TX_KEY_PIN.

static bool lastSignalState = false;

static void pollKeyerMemPlayback() {
    if (!keyerMemPlaying) return;

    // Mirror signal state to TX relay (no WS broadcast —
    // phone plays its own audio via single-shot keyer_play)
    bool sig = digitalRead(LED_PIN);
    if (sig != lastSignalState) {
        lastSignalState = sig;
        digitalWrite(TX_KEY_PIN, sig ? HIGH : LOW);
    }

    // Detect when the playback engine finishes
    if (playback.state == PlaybackState::Idle) {
        keyerMemPlaying = false;
        lastSignalState = false;
        digitalWrite(TX_KEY_PIN, LOW);
        txActive = false;
        wsBroadcastPlaybackDone();
        Serial.println(">>> Keyer memory playback complete");
    }
}

// ── Build keyer pattern ─────────────────────────────────────
// Characters separated by ' ', words separated by '|'.

static String buildKeyerPattern(const String& text) {
    String processed = text;
    processed.toUpperCase();
    String pattern;
    pattern.reserve(processed.length() * 6);
    bool needCharGap = false;

    // Temporarily set Progtable mode for getMorseCode lookups
    MorseMode savedMode = currentMode;
    currentMode = MorseMode::Progtable;

    for (int i = 0; i < (int)processed.length(); i++) {
        char c = processed[i];
        if (c == ' ') {
            pattern += '|';
            needCharGap = false;
            continue;
        }
        String morse = getMorseCode(c);
        if (morse.length() == 0) continue;
        if (needCharGap) pattern += ' ';
        pattern += morse;
        needCharGap = true;
    }

    currentMode = savedMode;
    return pattern;
}

// ── Keyer memory playback ───────────────────────────────────

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

    // Build complete morse pattern and send to phone in one message.
    // Phone builds and plays the full audio sequence independently —
    // buildKeyerSeq uses 4-dit char gaps and 8-dit word gaps that
    // exactly match the ESP32 playback engine timing.
    String fullPattern = buildKeyerPattern(keyerMem[slot]);
    wsBroadcastKeyerPlay(fullPattern, DOT_MS);

    // Play on ESP32 with echoMode=true — buzzer plays straight through
    // without waiting for phone ACKs between characters.
    // skipProsigns=true: send letter pairs as individual characters,
    // not as prosigns (e.g. "CQ" sends C then Q, not treated as prosign).
    MorseMode savedMode = currentMode;
    currentMode = MorseMode::Progtable;
    keyerMemPlaying = true;
    lastSignalState = false;
    startTransmission(keyerMem[slot], true, true);
    currentMode = savedMode;
}

bool isKeyerMemoryPlaying() {
    return keyerMemPlaying;
}

void stopKeyerMemoryPlayback() {
    keyerMemPlaying = false;

    if (playback.state != PlaybackState::Idle) {
        signalOff();
        playback.state = PlaybackState::Idle;
    }

    // Clear any pending phone handshake so stale ACKs don't ghost-restart
    extern void clearWordReadyFlag();
    clearWordReadyFlag();

    digitalWrite(TX_KEY_PIN, LOW);
    txActive = false;
    lastSignalState = false;

    // Tell the phone to kill its audio pipeline
    wsBroadcastPlaybackDone();

    Serial.println(">>> Keyer memory playback stopped");
}

// ── Keyer memory NVS persistence ────────────────────────────
// NVS writes are deferred to flushKeyerMemoryNVS() which runs
// in loop().  This avoids a thread-safety crash: the WebSocket
// handler runs in the async_tcp FreeRTOS task while loop() runs
// in loopTask — concurrent Preferences writes corrupt NVS pages.

static bool keyerMemDirty[KEYER_MEM_COUNT] = {};

void loadKeyerMemories() {
    if (!isStorageReady()) return;
    Preferences& prefs = getPrefs();
    for (int i = 0; i < KEYER_MEM_COUNT; i++) {
        char key[12];
        snprintf(key, sizeof(key), "keyMem%d", i);
        keyerMem[i] = prefs.getString(key, "");
        if (keyerMem[i].length() > 0) {
            Serial.printf("  Keyer M%d: \"%s\" (%d chars)\n", i + 1,
                keyerMem[i].substring(0, 40).c_str(),
                (int)keyerMem[i].length());
        }
    }
}

void setKeyerMemory(int slot, const String& text) {
    if (slot < 0 || slot >= KEYER_MEM_COUNT) return;
    String upper = text;
    upper.toUpperCase();
    // Truncate to max length
    if ((int)upper.length() > KEYER_MEM_MAX_LEN) {
        upper = upper.substring(0, KEYER_MEM_MAX_LEN);
    }
    keyerMem[slot] = upper;

    // Mark dirty — actual NVS write happens in flushKeyerMemoryNVS()
    // which is called from loop() where it's safe to use Preferences.
    keyerMemDirty[slot] = true;

    Serial.printf(">>> Keyer M%d queued for save: \"%s\" (%d chars)\n",
        slot + 1, upper.substring(0, 40).c_str(), (int)upper.length());
}

void flushKeyerMemoryNVS() {
    if (!isStorageReady()) return;
    Preferences& prefs = getPrefs();
    for (int i = 0; i < KEYER_MEM_COUNT; i++) {
        if (!keyerMemDirty[i]) continue;
        char key[12];
        snprintf(key, sizeof(key), "keyMem%d", i);
        size_t written = prefs.putString(key, keyerMem[i]);
        if (written > 0) {
            Serial.printf(">>> Keyer M%d saved to NVS (%d bytes)\n", i + 1, (int)written);
        } else {
            Serial.printf(">>> ERROR: Keyer M%d NVS write FAILED\n", i + 1);
        }
        keyerMemDirty[i] = false;
    }
}

String getKeyerMemory(int slot) {
    if (slot < 0 || slot >= KEYER_MEM_COUNT) return "";
    return keyerMem[slot];
}

// ── Mode switch & init ──────────────────────────────────────

OperatingMode getOperatingMode() {
    return opMode;
}

const char* getOperatingModeName() {
    return (opMode == OperatingMode::CWKeyer) ? "CWKeyer" : "Trainer";
}

bool isCWKeyerMode() {
    return opMode == OperatingMode::CWKeyer;
}

void initCWKeyerMode() {
    // Read the hardware switch and set initial mode
    switchState = digitalRead(MODE_SWITCH_PIN);
    lastSwitchReading = switchState;

    if (switchState == LOW) {
        opMode = OperatingMode::CWKeyer;
        Serial.println(">>> Initial mode: CW Keyer");
    }
    else {
        opMode = OperatingMode::Trainer;
        Serial.println(">>> Initial mode: Trainer");
    }

    // Ensure TX relay is safe
    pinMode(TX_KEY_PIN, OUTPUT);
    digitalWrite(TX_KEY_PIN, LOW);
    txActive = false;

    // Load keyer memories from NVS
    loadKeyerMemories();
}

void pollModeSwitch() {
    unsigned long now = millis();

    // ── Debounce the hardware mode switch ──
    int reading = digitalRead(MODE_SWITCH_PIN);

    if (reading != lastSwitchReading) {
        lastSwitchDebounceMs = now;
    }

    if ((now - lastSwitchDebounceMs) > MODE_DEBOUNCE_MS) {
        if (reading != switchState) {
            switchState = reading;

            if (switchState == LOW) {
                // Switch to CW Keyer mode
                opMode = OperatingMode::CWKeyer;
                Serial.println("\n>>> Mode: CW Keyer");

                // Stop any active trainer sessions
                if (isKochActive()) stopKochSession();
                if (isEchoActive()) stopEchoTraining();
                if (isPracticeActive()) stopPractice();

                // Reset iambic TX state
                itState = IambicTxState::Idle;
                itDitLatch = false;
                itDahLatch = false;
                itLastElement = '\0';

                // Reset straight key TX state
                txKeyState = digitalRead(KEY_PIN);
                txLastReading = txKeyState;

                wsBroadcastKeyerMode();
            }
            else {
                // Switch to Trainer mode
                opMode = OperatingMode::Trainer;
                Serial.println("\n>>> Mode: Trainer");

                // Stop keyer memory playback if active
                if (keyerMemPlaying) {
                    stopKeyerMemoryPlayback();
                }

                // Ensure TX relay is off
                digitalWrite(TX_KEY_PIN, LOW);
                txActive = false;
                signalOff();

                wsBroadcastKeyerMode();
            }

            Serial.print("> ");
        }
    }

    lastSwitchReading = reading;

    // ── If in CW Keyer mode, run the passthrough keyer + memory playback ──
    if (opMode == OperatingMode::CWKeyer) {
        // Memory playback takes priority over manual keying
        if (keyerMemPlaying) {
            pollKeyerMemPlayback();
        }
        else {
            if (getKeyType() == KeyType::Iambic) {
                pollIambicKeyerTx();
            }
            else {
                pollStraightKeyTx();
            }
        }
    }
}