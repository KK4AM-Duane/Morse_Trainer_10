#include "MorseWebSocket.h"
#include "MorseLookup.h"
#include "MorsePlayback.h"
#include "MorseCommands.h"
#include "MorseWebServer.h"
#include "KochTrainer.h"
#include "WiFiAPMode.h"
#include "WiFiStation.h"
#include "Storage.h"
#include "config.h"
#include "KeyInput.h"
#include "EchoTrainer.h"
#include "CWKeyerMode.h"
#include "PracticePlayer.h"
#include <ESPmDNS.h>

// --- Pin definitions ---
const unsigned int LED_PIN = 2;
const unsigned int BUZZER_PIN = 25;
const unsigned int LED_GREEN_PIN = 4;
const unsigned int LED_RED_PIN = 17;
const unsigned int KEY_PIN = 32;           // dit paddle / straight key
const unsigned int PADDLE_DAH_PIN = 33;    // dah paddle (iambic)
const unsigned int MODE_SWITCH_PIN = 18;   // HIGH = Trainer, LOW = CW Keyer
const unsigned int WIFI_MODE_PIN = 19;     // HIGH = AP mode,  LOW = STA mode (phone hotspot)
const unsigned int TX_KEY_PIN = 23;        // output to external transmitter relay
const int PWM_DUTY_CYCLE = 128;

// --- IP Blink Feature (STA mode only) ---
const unsigned int IP_BLINK_SWITCH_PIN = 15;  // Switch to ground to trigger IP/NC blink
const unsigned int EXTERNAL_LED_PIN = 16;     // External LED for IP address display

// --- RNG seed pin (floating ADC for entropy) ---
static const int RAND_PIN = 36;

// --- mDNS hostname (http://cwtrainer.local) ---
static const char* MDNS_HOSTNAME = "cwtrainer";

// --- IP blink timing (10 WPM: 1 dit = 120ms) ---
static const int IP_DIT_MS    = 120;
static const int IP_DAH_MS    = 3 * IP_DIT_MS;
static const int IP_ELEM_GAP  = IP_DIT_MS;
static const int IP_CHAR_GAP  = 3 * IP_DIT_MS;
static const int IP_WORD_GAP  = 7 * IP_DIT_MS;

// --- GPIO23 post-boot watchdog (non-blocking) ---
static const unsigned long TX_WATCHDOG_MS = 2000;  // monitor for 2 seconds after boot
static unsigned long txWatchdogStartMs = 0;
static bool txWatchdogActive = false;

// --- IP blink helpers ---
static bool ipSwitchPressed() { return digitalRead(IP_BLINK_SWITCH_PIN) == LOW; }

static void ipLedsOn()  { digitalWrite(LED_PIN, HIGH); digitalWrite(EXTERNAL_LED_PIN, HIGH); }
static void ipLedsOff() { digitalWrite(LED_PIN, LOW);  digitalWrite(EXTERNAL_LED_PIN, LOW); }

void setup() {
  // ── CRITICAL: TX relay output must be LOW before anything else ──
  // GPIO23 has an internal pull-up at reset. Drive it LOW immediately
  // to prevent a relay glitch during the boot delay.
  pinMode(TX_KEY_PIN, OUTPUT);
  digitalWrite(TX_KEY_PIN, LOW);

  // ── Key inputs must be ready before CW keyer mode reads them ──
  pinMode(KEY_PIN, INPUT_PULLUP);          // dit paddle / straight key
  pinMode(PADDLE_DAH_PIN, INPUT_PULLUP);   // dah paddle (iambic)
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);  // mode switch: HIGH=Trainer
  pinMode(WIFI_MODE_PIN, INPUT_PULLUP);    // WiFi switch: HIGH=AP, LOW=STA

  Serial.begin(115200);
  delay(1000);

  Serial.println("\n======================================");
  Serial.println("   ESP32 Morse Code Trainer");
  Serial.println("======================================\n");

  // Seed the random number generator from a floating ADC pin
  randomSeed(analogRead(RAND_PIN));

  // Initialize remaining hardware pins
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(LED_GREEN_PIN, OUTPUT);
  digitalWrite(LED_GREEN_PIN, LOW);

  pinMode(LED_RED_PIN, OUTPUT);
  digitalWrite(LED_RED_PIN, LOW);

  // Initialize IP blink feature pins
  pinMode(IP_BLINK_SWITCH_PIN, INPUT_PULLUP);
  pinMode(EXTERNAL_LED_PIN, OUTPUT);
  digitalWrite(EXTERNAL_LED_PIN, LOW);

  // Configure buzzer PWM using LEDC
  ledcAttach(BUZZER_PIN, 700, 8);  // 700 Hz tone, 8-bit resolution
  ledcWrite(BUZZER_PIN, 0);

  // Initialize NVS storage (must be before anything that loads state)
  initStorage();

  // Now safe to load saved settings from NVS
  initKeyInput();              // loads key type (Straight/Iambic) from NVS
  initEchoTrainer();           // loads echo char source (Koch/Full) from NVS
  initCWKeyerMode();           // reads mode switch, sets initial operating mode

  // Initialize Koch trainer (loads lesson + charStats + WPM from NVS)
  initKochTrainer();

  // Update timing from loaded WPM
  updateTiming();

  // Restore buzzer mute state from NVS
  setBuzzerEnabled(loadBuzzerFromNVS());

  // Initialize practice player (no NVS state — just zeroes)
  initPracticePlayer();

  // -- WiFi mode selection (read hardware switch before starting WiFi) --
  if (digitalRead(WIFI_MODE_PIN) == LOW) {
    setWiFiOpMode(WiFiOpMode::STA);
    Serial.println("WiFi switch: STA (phone hotspot)");
    initWiFiSTA();
  } else {
    setWiFiOpMode(WiFiOpMode::AP);
    Serial.println("WiFi switch: AP (ESP32 network)");
    initWiFiAP();
  }

  // -- mDNS: advertise http://cwtrainer.local on either interface --
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("✓ mDNS responder started: http://%s.local\n", MDNS_HOSTNAME);
  } else {
    Serial.println("✗ mDNS failed to start");
  }

  // Start web server (includes WebSocket) — works on either WiFi interface
  initWebServer();

  // Show the help menu on startup so the user sees available commands
  showHelp();

  // ── Final safety: guarantee TX relay is LOW after all init ──
  // LittleFS, WiFi, and mDNS init can disturb GPIO23 (VSPI MOSI).
  pinMode(TX_KEY_PIN, OUTPUT);
  digitalWrite(TX_KEY_PIN, LOW);

  // ── Start non-blocking GPIO23 watchdog (CW Keyer mode only) ──
  // Async init (WiFi, mDNS, LittleFS callbacks) may set GPIO23 HIGH
  // after setup() returns. Monitor it in loop() for 2 seconds.
  if (digitalRead(MODE_SWITCH_PIN) == LOW) {
    txWatchdogActive = true;
    txWatchdogStartMs = millis();
  }

  Serial.println("Ready! Type 'help' for commands.\n");
  Serial.print("> ");
}

void loop() {
  // ── GPIO23 post-boot watchdog (non-blocking, runs first) ──
  if (txWatchdogActive) {
    if (digitalRead(TX_KEY_PIN) == HIGH) {
      pinMode(TX_KEY_PIN, OUTPUT);
      digitalWrite(TX_KEY_PIN, LOW);
      txWatchdogActive = false;
      Serial.println(">>> GPIO23 glitch caught — forced LOW");
    } else if (millis() - txWatchdogStartMs >= TX_WATCHDOG_MS) {
      txWatchdogActive = false;
      Serial.println(">>> GPIO23 watchdog complete — pin stable");
    }
  }

  // Process serial commands
  readSerialInput();

  // Poll hardware mode switch + run CW keyer passthrough if active
  pollModeSwitch();

  // Flush any pending keyer memory NVS writes (safe here in loopTask)
  flushKeyerMemoryNVS();

  // ── In CW Keyer mode, skip trainer subsystems entirely ──
  // The keyer needs the tightest possible loop for clean sidetone.
  // Only run the WebServer cleanup on a throttled interval.
  if (isCWKeyerMode()) {
    // Keyer memory playback needs the playback engine
    updatePlayback();

    // Throttle WebServer cleanup to every 500ms instead of every loop
    static unsigned long lastWsCleanup = 0;
    unsigned long now = millis();
    if (now - lastWsCleanup > 500) {
      lastWsCleanup = now;
      handleWebServer();
    }
    return;
  }


  // Update non-blocking Morse playback state machine
  updatePlayback();

  // Poll key input for trainer mode (straight key or iambic echo decode)
  pollKeyInput();

  // Update Echo trainer state machine
  updateEchoTrainer();

  // Update Koch trainer state machine (no-op when idle)
  updateKochTrainer();

  // Update Practice player (feeds words to playback engine)
  updatePracticePlayer();

  // Monitor WiFi connections (mode-specific)
  if (getWiFiOpMode() == WiFiOpMode::AP) {
    checkWiFiClients();
    blinkLEDForConnection();
  } else {
    checkWiFiSTA();
    checkIPBlinkSwitch();
  }

  // Handle web server (WebSocket cleanup)
  handleWebServer();
}

// ═══════════════════════════════════════════════════════════════════
// IP Address Blinking Functions (STA mode only)
// Uses lookupMorseProgTable() from MorseLookup — no duplicate tables.
// NOTE: Blocking while switch is held. Trainer state machines freeze.
// ═══════════════════════════════════════════════════════════════════

// Blink a dit/dah pattern string on both LEDs (onboard + external).
static void blinkMorsePattern(const char* pattern) {
  for (int i = 0; pattern[i] != '\0'; i++) {
    if (!ipSwitchPressed()) { ipLedsOff(); return; }
    ipLedsOn();
    delay(pattern[i] == '.' ? IP_DIT_MS : IP_DAH_MS);
    ipLedsOff();
    delay(IP_ELEM_GAP);
  }
}

// Blink a single character using the existing Morse lookup table.
static void blinkChar(char c) {
  String pattern = lookupMorseProgTable(c);
  if (pattern.length() > 0) {
    blinkMorsePattern(pattern.c_str());
  }
}

// Blink the STA IP address in Morse code at 10 WPM.
static void blinkIPAddress() {
  String ip = getSTAIPAddress();
  for (unsigned int i = 0; i < ip.length(); i++) {
    if (!ipSwitchPressed()) return;
    char c = ip[i];
    if (c == '.') {
      blinkChar('.');
      delay(IP_WORD_GAP);   // word-space after dot (octet separator)
    } else if (isdigit(c)) {
      blinkChar(c);
      delay(IP_CHAR_GAP);
    }
  }
}

// Blink "NC" (Not Connected) in Morse code at 10 WPM.
static void blinkNC() {
  if (!ipSwitchPressed()) return;
  blinkChar('N');
  delay(IP_CHAR_GAP);
  if (!ipSwitchPressed()) return;
  blinkChar('C');
  delay(IP_WORD_GAP);
}

// Poll the IP blink switch. Blinks IP or "NC" while switch is held.
void checkIPBlinkSwitch() {
  if (!ipSwitchPressed()) return;
  delay(50);  // debounce
  if (!ipSwitchPressed()) return;

  while (ipSwitchPressed()) {
    if (isSTAReady()) {
      blinkIPAddress();
    } else {
      blinkNC();
    }
    if (!ipSwitchPressed()) break;
    delay(1000);  // pause between repetitions
  }

  Serial.println("[IP Blink Stopped]\n");
  Serial.print("> ");
}