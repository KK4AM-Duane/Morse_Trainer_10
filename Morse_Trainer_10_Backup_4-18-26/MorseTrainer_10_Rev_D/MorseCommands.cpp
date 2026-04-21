#include "MorseCommands.h"
#include "MorseLookup.h"
#include "MorsePlayback.h"
#include "MorseWebSocket.h"
#include "Morse_Table.h"
#include "KochTrainer.h"
#include "WiFiAP.h"        // For getStationCount()
#include "Storage.h"       // For NVS persistence
#include "EchoTrainer.h"
#include "KeyInput.h"      // For key type selection
#include "CWKeyerMode.h"   // For operating mode

bool echoCharacters = false;

// Display help
void showHelp() {
  Serial.println("\n=== Commands ===");
  Serial.println("  CHAR:CODE - Add/update morse entry (e.g., 'A:.-')");
  Serial.println("  list      - Show all morse table entries");
  Serial.println("  clear     - Clear the morse table");
  Serial.println("  test      - Test LED and buzzer");
  Serial.println("  mode      - Cycle: Koch -> Vector -> Progmem -> Progtable");
  Serial.println("  koch start- Start Koch training session");
  Serial.println("  koch stop - Stop Koch training session");
  Serial.println("  koch pause- Pause Koch session");
  Serial.println("  koch resume- Resume Koch session");
  Serial.println("  koch stat - Show Koch training statistics");
  Serial.println("  koch reset- Reset all Koch progress to defaults");
  Serial.println("  echo start- Start echo training (key back chars)");
  Serial.println("  echo stop - Stop echo training");
  Serial.println("  echo stat - Show echo training stats");
  Serial.println("  echo koch - Echo uses Koch lesson chars (default)");
  Serial.println("  echo full - Echo uses full character table");
  Serial.println("  echo on   - Enable real-time character echo");
  Serial.println("  echo off  - Disable real-time character echo");
  Serial.println("  key straight- Use straight key (default)");
  Serial.println("  key iambic  - Use iambic paddles (Mode B)");
  Serial.println("  keyer     - Show hardware keyer mode status");
  Serial.println("  buzzer on - Enable ESP32 buzzer during playback");
  Serial.println("  buzzer off- Mute buzzer (phone audio only)");
  Serial.println("  wpm XX    - Set speed (e.g., 'wpm 18')");
  Serial.println("  status    - Show current settings");
  Serial.println("  help      - Show this message");
  Serial.println("  WORD      - Transmit word/phrase in morse");
  Serial.println("              (use space for word gaps)");
  Serial.println("\nDuring Koch training:");
  Serial.println("  Type a single letter to answer");
  Serial.println("  !         - Emergency stop (Arduino Serial Monitor)");
  Serial.println("  Press ESC or Ctrl-C to quit (PuTTY/Tera Term)");
  Serial.println("  'stop' + Enter to quit from any trainer");
  Serial.println("\nHardware mode switch (GPIO18):");
  Serial.println("  HIGH = CW Trainer  |  LOW = CW Keyer (TX on GPIO23)");
}

// List morse table
void listTable() {
  Serial.println("\n=== Morse Table ===");
  if (currentMode == MorseMode::Koch) {
    Serial.println("(Using Koch trainer — Progtable lookup)");
    Serial.printf("Koch lesson: %d  Active chars: %d\n",
                  getKochLesson(), getKochActiveCount());
  } else if (currentMode == MorseMode::Progmem) {
    Serial.println("(Using PROGMEM binary tree - all ITU characters available)");
  } else if (currentMode == MorseMode::Progtable) {
    Serial.println("(Using PROGMEM direct lookup table)");
    Serial.print("Available characters: ");
    Serial.print(MORSE_LOOKUP_LEN);
    Serial.println(" entries");
  } else if (morseTable.empty()) {
    Serial.println("(empty - add characters with CHAR:CODE format)");
  } else {
    Serial.println("(Vector mode - dynamic table)");
    for (const auto &entry : morseTable) {
      Serial.print("  ");
      Serial.print(entry.getCharacter());
      Serial.print(" = ");
      Serial.println(entry.getCode());
    }
  }
  Serial.println();
}

// Show current status
void showStatus() {
  Serial.println("\n=== Current Settings ===");
  Serial.print("Operating mode: ");
  Serial.println(getOperatingModeName());
  if (isCWKeyerMode()) {
    Serial.println("  (Hardware switch LOW — trainer commands disabled)");
    Serial.printf("  Key type: %s  |  TX output: GPIO23\n", getKeyTypeName());
    Serial.print("Speed: ");
    Serial.print(WPM);
    Serial.println(" WPM");
    Serial.println();
    return;
  }
  Serial.print("Mode: ");
  if (currentMode == MorseMode::Koch) {
    Serial.println("Koch Trainer");
    Serial.printf("  Lesson: %d  Active: %d chars\n",
                  getKochLesson(), getKochActiveCount());
    Serial.printf("  Session: %d/%d", getKochSessionCorrect(), getKochSessionTotal());
    if (getKochSessionTotal() > 0) {
      Serial.printf("  (%.0f%%)",
        100.0f * getKochSessionCorrect() / getKochSessionTotal());
    }
    Serial.println();
    Serial.printf("  Training: %s\n", isKochActive() ? "RUNNING" : "stopped");
  } else if (currentMode == MorseMode::Progmem) {
    Serial.println("PROGMEM (Binary Tree)");
  } else if (currentMode == MorseMode::Progtable) {
    Serial.println("PROGTABLE (Direct Lookup)");
  } else {
    Serial.println("Vector (Dynamic)");
  }
  Serial.print("Key type: ");
  Serial.println(getKeyTypeName());
  Serial.print("Echo: ");
  Serial.println(echoCharacters ? "ON" : "OFF");
  Serial.print("Echo training: ");
  if (isEchoActive()) {
    uint16_t eCorr, eTotal;
    getEchoStats(eCorr, eTotal);
    Serial.printf("ACTIVE (%u/%u correct, source: %s)\n",
                  eCorr, eTotal, getEchoCharSourceName());
  } else {
    Serial.printf("INACTIVE (source: %s)\n", getEchoCharSourceName());
  }
  Serial.print("Buzzer: ");
  Serial.println(isBuzzerEnabled() ? "ON" : "OFF");
  Serial.print("Speed: ");
  Serial.print(WPM);
  Serial.println(" WPM");
  Serial.print("Timing: DOT=");
  Serial.print(DOT_MS);
  Serial.print("ms, DASH=");
  Serial.print(DASH_MS);
  Serial.print("ms");
  Serial.println();
  Serial.print("Playback: ");
  Serial.println(playback.state == PlaybackState::Idle ? "Idle" : "Active");
  Serial.print("WiFi AP: Active, Stations: ");
  Serial.println(getStationCount());
  Serial.println();
}

// Show Koch statistics
static void showKochStats() {
  Serial.println("\n=== Koch Training Statistics ===");
  Serial.printf("Lesson: %d / %d\n", getKochLesson(), KOCH_ORDER_LEN);
  Serial.printf("Session: %d correct / %d total",
                getKochSessionCorrect(), getKochSessionTotal());
  if (getKochSessionTotal() > 0) {
    Serial.printf("  (%.1f%%)",
      100.0f * getKochSessionCorrect() / getKochSessionTotal());
  }
  Serial.println();
  Serial.printf("Speed: %d WPM\n", WPM);

  Serial.println("\nPer-character stats:");
  Serial.println("  Char  Prob  Sent  Correct  AvgMs");
  const CharStats* stats = getKochCharStats();
  int activeCount = getKochActiveCount();
  for (int i = 0; i < activeCount; i++) {
    char c = stats[i].character;
    String name;
    if (c == '^')      name = "AR";
    else if (c == '[') name = "BT";
    else if (c == '|') name = "SK";
    else               { name = " "; name += c; }

    Serial.printf("  %s     %3d  %4d    %4d  %5lu\n",
                  name.c_str(),
                  stats[i].probability,
                  stats[i].totalSent,
                  stats[i].totalCorrect,
                  (unsigned long)stats[i].avgReactionMs);
  }
  Serial.println();
}

// Toggle mode - cycles through all four modes
void toggleMode() {
  if (currentMode == MorseMode::Koch && isKochActive()) {
    stopKochSession();
  }

  String modeMessage;
  
  if (currentMode == MorseMode::Koch) {
    currentMode = MorseMode::Vector;
    Serial.println("Switched to Vector mode (dynamic table)");
    initializeVectorTable();
    modeMessage = "VECTOR";
  } else if (currentMode == MorseMode::Vector) {
    currentMode = MorseMode::Progmem;
    Serial.println("Switched to PROGMEM mode (binary tree lookup)");
    modeMessage = "PROGMEM";
  } else if (currentMode == MorseMode::Progmem) {
    currentMode = MorseMode::Progtable;
    Serial.println("Switched to PROGTABLE mode (direct lookup table)");
    modeMessage = "PROGTABLE";
  } else {
    currentMode = MorseMode::Koch;
    Serial.println("Switched to Koch Trainer mode");
    Serial.println("  Type 'koch start' to begin a session.");
    modeMessage = "KOCH";
  }
  
  Serial.println();
  startTransmission(modeMessage);
}

// Set WPM
void setWPM(int newWPM) {
  if (newWPM < 1 || newWPM > 60) {
    Serial.println("ERROR: WPM must be between 1 and 60");
    return;
  }
  WPM = newWPM;
  updateTiming();
  
  if (isStorageReady()) {
    getPrefs().putUInt("wpm", WPM);
  }
  
  Serial.print("Speed set to ");
  Serial.print(WPM);
  Serial.println(" WPM (saved)");
  Serial.println();
}

// Test hardware - sends different message based on mode
void testHardware() {
  Serial.println("\n>>> Testing LED and Buzzer...");
  
  String testMessage;
  if (currentMode == MorseMode::Koch) {
    testMessage = "KOCH";
    Serial.println(">>> Sending 'KOCH' in Koch mode");
  } else if (currentMode == MorseMode::Vector) {
    testMessage = "DYNAMIC TABLE";
    Serial.println(">>> Sending 'DYNAMIC TABLE' in Vector mode");
  } else if (currentMode == MorseMode::Progmem) {
    testMessage = "BINARY TREE";
    Serial.println(">>> Sending 'BINARY TREE' in Progmem mode");
  } else {
    testMessage = "DIRECT LOOKUP TABLE";
    Serial.println(">>> Sending 'DIRECT LOOKUP TABLE' in Progtable mode");
  }
  
  startTransmission(testMessage);
}

// Parse CHAR:CODE format
bool parseCharCode(const String &input) {
  int colonIndex = input.indexOf(':');
  
  if (colonIndex <= 0 || colonIndex >= (int)input.length() - 1) {
    return false;
  }
  
  char character = input[0];
  String code = input.substring(colonIndex + 1);
  code.trim();
  
  for (int i = 0; i < (int)code.length(); ++i) {
    if (code[i] != '.' && code[i] != '-' && code[i] != ' ') {
      Serial.println("ERROR: Morse code must contain only '.', '-', and ' '");
      return false;
    }
  }
  
  if (code.length() == 0) {
    Serial.println("ERROR: Morse code cannot be empty");
    return false;
  }
  
  addMorseEntry(character, code);
  return true;
}

// Process a complete command
void processCommand(String command) {
  command.trim();
  
  if (command.length() == 0) {
    return;
  }

  // --- Commands that always work regardless of operating mode ---

  if (command.equalsIgnoreCase("help")) {
    showHelp();
    return;
  }
  if (command.equalsIgnoreCase("status")) {
    showStatus();
    return;
  }
  if (command.equalsIgnoreCase("keyer")) {
    Serial.printf("Operating mode: %s\n", getOperatingModeName());
    Serial.printf("Key type: %s\n", getKeyTypeName());
    Serial.printf("Speed: %d WPM\n\n", WPM);
    return;
  }
  if (command.startsWith("wpm ")) {
    String wpmStr = command.substring(4);
    wpmStr.trim();
    int newWPM = wpmStr.toInt();
    if (newWPM > 0) {
      setWPM(newWPM);
    } else {
      Serial.println("ERROR: Invalid WPM value\n");
    }
    return;
  }
  if (command.equalsIgnoreCase("wpm")) {
    Serial.print("Current speed: ");
    Serial.print(WPM);
    Serial.println(" WPM\n");
    return;
  }
  if (command.equalsIgnoreCase("key straight")) {
    if (isEchoActive()) {
      Serial.println("Stop echo training first, then change key type.");
    } else {
      setKeyType(KeyType::Straight);
    }
    return;
  }
  if (command.equalsIgnoreCase("key iambic")) {
    if (isEchoActive()) {
      Serial.println("Stop echo training first, then change key type.");
    } else {
      setKeyType(KeyType::Iambic);
    }
    return;
  }
  if (command.equalsIgnoreCase("key")) {
    Serial.printf("Key type: %s\n\n", getKeyTypeName());
    return;
  }
  if (command.startsWith("buzzer ")) {
    String setting = command.substring(7);
    setting.trim();
    if (setting.equalsIgnoreCase("on")) {
      setBuzzerEnabled(true);
      saveBuzzerToNVS();
      wsBroadcastBuzzerState(true);
    } else if (setting.equalsIgnoreCase("off")) {
      setBuzzerEnabled(false);
      saveBuzzerToNVS();
      wsBroadcastBuzzerState(false);
    } else {
      Serial.println("ERROR: Use 'buzzer on' or 'buzzer off'\n");
    }
    return;
  }

  // --- "stop" always works ---
  if (command.equalsIgnoreCase("koch stop") ||
      command.equalsIgnoreCase("stop")) {
    bool stopped = false;
    if (isKochActive()) { stopKochSession(); stopped = true; }
    if (isEchoActive()) { stopEchoTraining(); stopped = true; }
    if (!stopped) {
      Serial.println("No training session is running.");
    }
    return;
  }

  // --- Block trainer-only commands when in CW Keyer mode ---
  if (isCWKeyerMode()) {
    Serial.println("Command unavailable in CW Keyer mode.");
    Serial.println("Flip mode switch to HIGH for Trainer mode.\n");
    return;
  }

  // --- Pause / Resume ---
  if (command.equalsIgnoreCase("koch pause") ||
      command.equalsIgnoreCase("pause")) {
    if (isKochActive() && !isKochPaused()) {
      pauseKochSession();
    } else if (isKochPaused()) {
      Serial.println("Already paused. Type 'koch resume'.");
    } else {
      Serial.println("Koch trainer is not running.");
    }
    return;
  }

  if (command.equalsIgnoreCase("koch resume") ||
      command.equalsIgnoreCase("resume")) {
    if (isKochPaused()) {
      resumeKochSession();
    } else if (isKochActive()) {
      Serial.println("Koch trainer is not paused.");
    } else {
      Serial.println("Koch trainer is not running.");
    }
    return;
  }

  // --- If Koch is active, single-char input is an answer ---
  if (isKochActive() && !isKochPaused() && command.length() == 1) {
    char c = toupper(command[0]);
    if (c >= 32 && c <= 126) {
      kochSubmitAnswer(c);
      return;
    }
  }
  
  Serial.print("Processing: '");
  Serial.print(command);
  Serial.println("'");

  if (command.equalsIgnoreCase("list")) {
    listTable();
  }
  else if (command.equalsIgnoreCase("clear")) {
    morseTable.clear();
    Serial.println("Morse table cleared.\n");
  }
  else if (command.equalsIgnoreCase("test")) {
    testHardware();
  }
  else if (command.equalsIgnoreCase("mode")) {
    toggleMode();
  }
  // --- Koch sub-commands ---
  else if (command.equalsIgnoreCase("koch start")) {
    if (isEchoActive()) {
      Serial.println("Stop echo training first ('echo stop').");
    } else {
      if (currentMode != MorseMode::Koch) {
        currentMode = MorseMode::Koch;
        Serial.println("Switched to Koch mode.");
      }
      startKochSession();
    }
  }
  else if (command.equalsIgnoreCase("koch stat") ||
           command.equalsIgnoreCase("koch stats")) {
    showKochStats();
  }
  else if (command.equalsIgnoreCase("koch reset")) {
    resetKochProgress();
  }
  // --- Echo sub-commands ---
  else if (command.equalsIgnoreCase("echo start")) {
    startEchoTraining();
  }
  else if (command.equalsIgnoreCase("echo stop")) {
    stopEchoTraining();
  }
  else if (command.equalsIgnoreCase("echo stat") ||
           command.equalsIgnoreCase("echo stats")) {
    echoPrintStats();
  }
  else if (command.equalsIgnoreCase("echo koch")) {
    if (isEchoActive()) {
      Serial.println("Stop echo training first, then change source.");
    } else {
      setEchoCharSource(EchoCharSource::Koch);
    }
  }
  else if (command.equalsIgnoreCase("echo full")) {
    if (isEchoActive()) {
      Serial.println("Stop echo training first, then change source.");
    } else {
      setEchoCharSource(EchoCharSource::Full);
    }
  }
  else if (command.startsWith("echo ")) {
    String setting = command.substring(5);
    setting.trim();
    if (setting.equalsIgnoreCase("on")) {
      echoCharacters = true;
      Serial.println("Real-time character echo enabled\n");
    } else if (setting.equalsIgnoreCase("off")) {
      echoCharacters = false;
      Serial.println("Real-time character echo disabled\n");
    } else {
      Serial.println("ERROR: Use 'echo on' or 'echo off'\n");
    }
  }
  else if (command.indexOf(':') != -1) {
    if (!parseCharCode(command)) {
      startTransmission(command);
    }
  }
  else {
    startTransmission(command);
  }
}

// Handle real-time character echo with morse playback
void handleRealtimeEcho(char c) {
  Serial.write(c);
  
  if (c == '\n' || c == '\r') {
    return;
  }
  
  String charStr = String(c);
  startTransmission(charStr, true);
}

// Read serial input line by line
void readSerialInput() {
  static String inputLine = "";
  
  while (Serial.available() > 0) {
    char c = Serial.read();

    // --- ESC (0x1B) or Ctrl-C (0x03) = instant stop from any state ---
    if (c == 0x1B || c == 0x03) {
      if (isKochActive()) {
        Serial.println("\n[ESC] Stopping Koch...");
        stopKochSession();
      }
      if (isEchoActive()) {
        Serial.println("\n[ESC] Stopping Echo...");
        stopEchoTraining();
      }
      inputLine = "";
      continue;
    }

    // --- '!' panic stop — works from Arduino Serial Monitor ---
    if (c == '!' && inputLine.length() == 0 && Serial.available() == 0) {
      bool stopped = false;
      if (isKochActive())  { Serial.println("\n[!] Stopping Koch...");  stopKochSession();  stopped = true; }
      if (isEchoActive())  { Serial.println("\n[!] Stopping Echo...");  stopEchoTraining(); stopped = true; }
      if (!stopped) {
        inputLine += c;
        Serial.write(c);
        continue;
      }
      continue;
    }

    // --- Koch single-character answer shortcut ---
    if (isKochActive() && !isKochPaused() && c >= 32 && c <= 126
        && inputLine.length() == 0 && Serial.available() == 0) {
      Serial.write(c);
      Serial.println();
      kochSubmitAnswer(toupper(c));
      continue;
    }
    
    if (echoCharacters && !isKochActive() && !isCWKeyerMode() &&
        c >= 32 && c <= 126 && playback.state == PlaybackState::Idle) {
      handleRealtimeEcho(c);
      inputLine += c;
      continue;
    }
    
    // Echo typed characters to terminal
    if (playback.state == PlaybackState::Idle || isKochActive()) {
      Serial.write(c);
    }
    
    if (c == '\n' || c == '\r') {
      inputLine.trim();
      if (inputLine.length() > 0) {
        Serial.println();
        processCommand(inputLine);
        inputLine = "";
      }
      continue;
    }
    
    if (c == '\b' || c == 127) {
      if (inputLine.length() > 0) {
        inputLine.remove(inputLine.length() - 1);
      }
      continue;
    }
    
    if (c >= 32 && c <= 126) {
      inputLine += c;
    }
  }
}