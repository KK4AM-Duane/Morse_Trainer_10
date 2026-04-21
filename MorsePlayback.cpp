#include "MorsePlayback.h"
#include "MorseLookup.h"
#include "config.h"
#include "MorseWebSocket.h"
#include "Storage.h"

// Note: LED_PIN, BUZZER_PIN, PWM_DUTY_CYCLE are now extern in header, defined in .ino
// Global playback context
PlaybackContext playback;

// Buzzer mute state — LED still flashes during playback even when muted
static bool buzzerEnabled = true;

// Phone handshake state for freeform (WORD) playback
static volatile bool wordReadyFlag = false;
static unsigned long wordReadySentMs = 0;

// Timing variables
unsigned int WPM = 20;
unsigned int DOT_MS = 1200 / WPM;
unsigned int DASH_MS = 3 * DOT_MS;
unsigned int ELEMENT_GAP_MS = DOT_MS;
unsigned int CHAR_GAP_MS = 3 * DOT_MS;
unsigned int WORD_GAP_MS = 7 * DOT_MS;

// Function to update timing based on WPM
void updateTiming() {
    DOT_MS = 1200 / WPM;
    DASH_MS = 3 * DOT_MS;
    ELEMENT_GAP_MS = DOT_MS;
    CHAR_GAP_MS = 3 * DOT_MS;
    WORD_GAP_MS = 7 * DOT_MS;
}

// Function to turn on LED and buzzer
void signalOn() {
    digitalWrite(LED_PIN, HIGH);
    if (buzzerEnabled) {
        ledcWrite(BUZZER_PIN, PWM_DUTY_CYCLE);
    }
}

// Function to turn off LED and buzzer
void signalOff() {
    digitalWrite(LED_PIN, LOW);
    ledcWrite(BUZZER_PIN, 0);
}

// Buzzer mute control
void setBuzzerEnabled(bool enabled) {
    buzzerEnabled = enabled;
    if (!enabled) {
        ledcWrite(BUZZER_PIN, 0);   // silence immediately
    }
    Serial.printf("Buzzer %s\n", enabled ? "ON" : "OFF");
}

bool isBuzzerEnabled() {
    return buzzerEnabled;
}

void saveBuzzerToNVS() {
    if (!isStorageReady()) return;
    getPrefs().putBool("buzzerOn", buzzerEnabled);
}

bool loadBuzzerFromNVS() {
    if (!isStorageReady()) return true;   // default on
    return getPrefs().getBool("buzzerOn", true);
}

// Phone handshake callback — set by WebSocket when "word_ready" arrives
void wordPhoneReady() {
    wordReadyFlag = true;
}

// Return a friendly display name for prosign symbols, or the char itself
static String prosignDisplayName(char c) {
    switch (c) {
    case '~': return "AA";
    case '^': return "AR";
    case '<': return "AS";
    case '>': return "BK";
    case '[': return "BT";
    case ']': return "CL";
    case '{': return "CT";
    case '}': return "DO";
    case '#': return "HH";
    case '*': return "KA";
    case '$': return "KN";
    case '|': return "SK";
    case '_': return "SN";
    case '\\': return "SOS";
    default:  return String(c);
    }
}

// Begin buzzer playback of the current character's first element.
// Called after phone ACKs "word_ready" or timeout expires.
static void beginCharPlayback() {
    char element = playback.currentMorseCode[0];
    playback.codeIndex = 0;

    // Print character header and first element to serial
    char c = playback.message[playback.messageIndex];
    Serial.print("(");
    Serial.print(prosignDisplayName(c));
    Serial.print(":");
    Serial.print(element);

    // Tell the phone to start playing now
    wsBroadcastWordGo();

    // Start the buzzer
    signalOn();
    playback.state = PlaybackState::SignalOn;
    playback.stateStartTime = millis();
    playback.stateDuration = (element == '.') ? DOT_MS : DASH_MS;
}

// Send the current character's pattern to the phone and enter WaitPhoneReady.
// If echoMode is true (Koch), skip the handshake — Koch has its own.
static void sendCharToPhoneAndWait() {
    if (playback.echoMode) {
        // Koch mode — no freeform handshake; start immediately
        beginCharPlayback();
        return;
    }

    char c = playback.message[playback.messageIndex];

    // Send pattern + timing to phone so it can pre-build audio
    wsBroadcastWordChar(c, playback.currentMorseCode, DOT_MS);

    wordReadyFlag = false;
    wordReadySentMs = millis();
    playback.state = PlaybackState::WaitPhoneReady;
}

// Non-blocking playback update - called from loop()
void updatePlayback() {
    if (playback.state == PlaybackState::Idle) {
        return;
    }

    // Feed the watchdog to prevent resets during long transmissions
    yield();

    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - playback.stateStartTime;

    // ── WaitPhoneReady: check for ACK or timeout ──
    if (playback.state == PlaybackState::WaitPhoneReady) {
        if (wordReadyFlag) {
            wordReadyFlag = false;
            beginCharPlayback();
        }
        else if (currentTime - wordReadySentMs >= WORD_READY_TIMEOUT_MS) {
            // Timeout — no phone or phone is slow; start buzzer anyway
            beginCharPlayback();
        }
        return;
    }

    // Safety check: if we've been in a state too long, reset
    if (elapsed > 10000) {  // 10 second timeout
        Serial.println("\n>>> ERROR: Playback timeout, resetting...\n");
        signalOff();
        playback.state = PlaybackState::Idle;
        wsBroadcastPlaybackDone();
        Serial.print("> ");
        return;
    }

    // Check if current state duration has elapsed
    if (elapsed < playback.stateDuration) {
        return;
    }

    // State machine
    switch (playback.state) {
    case PlaybackState::SignalOn:
        // Turn off signal and move to element gap
        signalOff();
        playback.state = PlaybackState::ElementGap;
        playback.stateStartTime = currentTime;
        playback.stateDuration = ELEMENT_GAP_MS;
        break;

    case PlaybackState::ElementGap:
        // Move to next element in morse code
        playback.codeIndex++;

        // Safety check
        if (playback.codeIndex > 100) {  // No morse code should be this long
            Serial.println("\n>>> ERROR: Code index overflow\n");
            playback.state = PlaybackState::Idle;
            signalOff();
            wsBroadcastPlaybackDone();
            Serial.print("> ");
            break;
        }

        if (playback.codeIndex >= (int)playback.currentMorseCode.length()) {
            // Finished current character — broadcast to WebSocket clients
            char completedChar = playback.message[playback.messageIndex];
            wsBroadcastCharSent(completedChar, playback.currentMorseCode);

            Serial.print(") ");
            playback.messageIndex++;

            // Check if message is complete
            if (playback.messageIndex >= (int)playback.message.length()) {
                // Message complete
                Serial.println("\n>>> Done.\n");
                playback.state = PlaybackState::Idle;
                wsBroadcastPlaybackDone();
                if (!playback.echoMode) {
                    Serial.print("> ");
                }
            }
            else {
                // Check next character
                char nextChar = playback.message[playback.messageIndex];

                if (nextChar == ' ') {
                    // Word gap
                    Serial.print(" | ");
                    playback.state = PlaybackState::WordGap;
                    playback.stateStartTime = currentTime;
                    playback.stateDuration = WORD_GAP_MS;
                    playback.messageIndex++;
                }
                else {
                    // Character gap
                    playback.state = PlaybackState::CharGap;
                    playback.stateStartTime = currentTime;
                    playback.stateDuration = CHAR_GAP_MS;
                }
            }
        }
        else {
            // Play next element (spaces in patterns should never exist)
            char element = playback.currentMorseCode[playback.codeIndex];
            Serial.print(element);
            signalOn();
            playback.state = PlaybackState::SignalOn;
            playback.stateStartTime = currentTime;
            playback.stateDuration = (element == '.') ? DOT_MS : DASH_MS;
        }
        break;

    case PlaybackState::CharGap:
        // Start next character
        {
            // Safety check
            if (playback.messageIndex >= (int)playback.message.length()) {
                Serial.println("\n>>> Done.\n");
                playback.state = PlaybackState::Idle;
                wsBroadcastPlaybackDone();
                if (!playback.echoMode) {
                    Serial.print("> ");
                }
                break;
            }

            char c = playback.message[playback.messageIndex];
            playback.currentMorseCode = getMorseCode(c);

            if (playback.currentMorseCode.length() == 0) {
                Serial.print("\n[");
                Serial.print(prosignDisplayName(c));
                Serial.println(" not in table - skipping]");
                playback.messageIndex++;

                if (playback.messageIndex >= (int)playback.message.length()) {
                    Serial.println("\n>>> Done.\n");
                    playback.state = PlaybackState::Idle;
                    wsBroadcastPlaybackDone();
                    if (!playback.echoMode) {
                        Serial.print("> ");
                    }
                }
                break;
            }

            // Send pattern to phone and wait for ACK before starting buzzer
            sendCharToPhoneAndWait();
        }
        break;

    case PlaybackState::WordGap:
        // After word gap, start next character
        playback.state = PlaybackState::CharGap;
        playback.stateStartTime = currentTime;
        playback.stateDuration = 0;
        break;

    default:
        playback.state = PlaybackState::Idle;
        signalOff();
        wsBroadcastPlaybackDone();
        break;
    }
}

// Start a new transmission
void startTransmission(const String& message, bool echo, bool skipProsigns) {
    if (message.length() == 0) return;

    // Preprocess prosigns BEFORE playback — converts "KN" → '$', "AR" → '^', etc.
    // When skipProsigns is true (memory/practice playback), send letters individually.
    if (skipProsigns) {
        playback.message = message;
        playback.message.toUpperCase();
    }
    else {
        playback.message = preprocessProsigns(message);
    }
    playback.messageIndex = 0;
    playback.codeIndex = 0;
    playback.echoMode = echo;

    // Broadcast playback start to all WebSocket clients (show original message)
    wsBroadcastPlaybackStart(message, WPM);

    // Skip any leading unknown characters (treat as spaces)
    while (playback.messageIndex < (int)playback.message.length()) {
        char c = playback.message[playback.messageIndex];

        // Skip spaces
        if (c == ' ') {
            playback.messageIndex++;
            continue;
        }

        playback.currentMorseCode = getMorseCode(c);

        if (playback.currentMorseCode.length() == 0) {
            Serial.print("[");
            Serial.print(prosignDisplayName(c));
            Serial.println(" not in table - skipping]");
            playback.messageIndex++;
            continue;  // Skip unknown character
        }

        // Found valid character — start playback
        sendCharToPhoneAndWait();
        return;
    }

    // If we get here, entire message was invalid characters
    Serial.println(">>> No valid morse characters in message");
    playback.state = PlaybackState::Idle;
    wsBroadcastPlaybackDone();
}

// Clear the phone handshake flag — called when playback is force-stopped
// to prevent stale ACKs from ghost-restarting the next transmission.
void clearWordReadyFlag() {
    wordReadyFlag = false;
}