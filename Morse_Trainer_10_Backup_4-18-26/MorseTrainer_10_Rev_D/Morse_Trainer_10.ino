#include "MorseWebSocket.h"
#include "MorseLookup.h"
#include "MorsePlayback.h"
#include "MorseCommands.h"
#include "MorseWebServer.h"
#include "KochTrainer.h"
#include "WiFiAP.h"
#include "Storage.h"
#include "config.h"
#include "KeyInput.h"
#include "EchoTrainer.h"
#include "CWKeyerMode.h"
#include "PracticePlayer.h"

// --- Pin definitions ---
const unsigned int LED_PIN = 2;
const unsigned int BUZZER_PIN = 25;
const unsigned int LED_GREEN_PIN = 4;
const unsigned int LED_RED_PIN = 17;
const unsigned int KEY_PIN = 32;           // dit paddle / straight key
const unsigned int PADDLE_DAH_PIN = 33;    // dah paddle (iambic)
const unsigned int MODE_SWITCH_PIN = 18;   // HIGH = Trainer, LOW = CW Keyer
const unsigned int TX_KEY_PIN = 23;        // output to external transmitter relay
const int PWM_DUTY_CYCLE = 128;

// --- RNG seed pin (floating ADC for entropy) ---
static const int RAND_PIN = 36;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n======================================");
  Serial.println("   ESP32 Morse Code Trainer");
  Serial.println("======================================\n");

  // Seed the random number generator from a floating ADC pin
  randomSeed(analogRead(RAND_PIN));

  // Initialize hardware pins
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(LED_GREEN_PIN, OUTPUT);
  digitalWrite(LED_GREEN_PIN, LOW);

  pinMode(LED_RED_PIN, OUTPUT);
  digitalWrite(LED_RED_PIN, LOW);

  pinMode(KEY_PIN, INPUT_PULLUP);          // dit paddle / straight key
  pinMode(PADDLE_DAH_PIN, INPUT_PULLUP);   // dah paddle (iambic)
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);  // mode switch: HIGH=Trainer
  pinMode(TX_KEY_PIN, OUTPUT);             // TX keying relay output
  digitalWrite(TX_KEY_PIN, LOW);

  // Configure buzzer PWM using LEDC
  ledcAttach(BUZZER_PIN, 800, 8);  // 800 Hz tone, 8-bit resolution
  ledcWrite(BUZZER_PIN, 0);

  // Initialize Morse code lookup table
  initializeVectorTable();

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

  // Start WiFi Access Point
  initWiFiAP();

  // Start web server (includes WebSocket)
  initWebServer();

  // Show the help menu on startup so the user sees available commands
  showHelp();

  Serial.println("Ready! Type 'help' for commands.\n");
  Serial.print("> ");
}

void loop() {
  // Process serial commands
  readSerialInput();

  // Poll hardware mode switch + run CW keyer passthrough if active
  pollModeSwitch();

  // Update non-blocking Morse playback state machine
  updatePlayback();

  // Poll key input for trainer mode (straight key or iambic echo decode)
  // Skip when in CW Keyer mode — pollModeSwitch handles key input there
  if (!isCWKeyerMode()) {
    pollKeyInput();
  }

  // Update Echo trainer state machine
  updateEchoTrainer();

  // Update Koch trainer state machine (no-op when idle)
  updateKochTrainer();

  // Update Practice player (feeds words to playback engine)
  updatePracticePlayer();

  // Monitor WiFi client connections
  checkWiFiClients();
  blinkLEDForConnection();

  // Handle web server (WebSocket cleanup)
  handleWebServer();
}