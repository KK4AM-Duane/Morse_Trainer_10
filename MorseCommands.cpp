#include "MorseCommands.h"
#include "MorseLookup.h"
#include "MorsePlayback.h"
#include "MorseWebSocket.h"
#include "Morse_Table.h"
#include "KochTrainer.h"
#include "WiFiAPMode.h"      // For getStationCount(), getWiFiOpMode()
#include "WiFiStation.h"   // For STA credential commands
#include "Storage.h"       // For NVS persistence
#include "EchoTrainer.h"
#include "CWKeyerMode.h"       // isKeyerMemoryPlaying
#include "KeyInput.h"      // For KeyType, setKeyType(), getKeyTypeName()

// Display help
void showHelp() {
  Serial.println("\n=== Commands ===");
  Serial.println("  koch start  - Start Koch training session");
  Serial.println("  koch stop   - Stop Koch training session");
  Serial.println("  koch pause  - Pause Koch session");
  Serial.println("  koch resume - Resume Koch session");
  Serial.println("  koch stat   - Show Koch training statistics");
  Serial.println("  koch reset  - Reset all Koch progress to defaults");
  Serial.println("  echo start  - Start straight-key echo training");
  Serial.println("  echo stop   - Stop straight-key echo training");
  Serial.println("  echo stat   - Show echo training stats");
  Serial.println("  echo koch   - Set echo char source to Koch set");
  Serial.println("  echo full   - Set echo char source to full set");
  Serial.println("  stop        - Stop any active training session");
  Serial.println("  pause       - Pause Koch session (shortcut)");
  Serial.println("  resume      - Resume Koch session (shortcut)");
  Serial.println("  key         - Show current key type");
  Serial.println("  key straight- Set straight key mode");
  Serial.println("  key iambic  - Set iambic paddle mode");
  Serial.println("  keyer       - Show keyer status");
  Serial.println("  buzzer on   - Enable ESP32 buzzer during playback");
  Serial.println("  buzzer off  - Mute buzzer (phone audio only)");
  Serial.println("  wpm XX      - Set speed (e.g., 'wpm 18')");
  Serial.println("  wpm         - Show current speed");
  Serial.println("  mode        - Cycle: Koch → Progtable");
  Serial.println("  wifi show   - Show WiFi connection info");
  Serial.println("  wifi reset  - Clear saved WiFi credentials");
  Serial.println("  status      - Show current settings");
  Serial.println("  help        - Show this message");
  Serial.println("  practice start <text> - Play text word-by-word");
  Serial.println("  practice file  - Play from /practice.txt");
  Serial.println("  practice stop  - Stop practice playback");
  Serial.println("\nDuring Koch training:");
  Serial.println("  Type a single letter to answer");
  Serial.println("  Press ESC or type 'stop' + Enter to quit");
  Serial.println("\nProsigns (type as shown, sent as single character):");
  Serial.println("  AA = new line      AR = end message    AS = wait");
  Serial.println("  BK = break         BT = separator      CL = closing");
  Serial.println("  CT = start signal  DO = shift          HH = error");
  Serial.println("  KA = start signal  KN = invite TX      SK = end contact");
  Serial.println("  SN = understood    SOS = distress");
  Serial.println("\nOr use special chars: ~ ^ < > [ ] { } # * $ | _ \\");
  Serial.println();
}

// Show current status
void showStatus() {
  Serial.println("\n=== Current Settings ===");
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
  } else {
    Serial.println("Progtable (Direct Lookup)");
  }
  Serial.print("Echo training: ");
  if (isEchoActive()) {
    uint16_t eCorr, eTotal;
    getEchoStats(eCorr, eTotal);
    Serial.printf("ACTIVE (%u/%u correct)\n", eCorr, eTotal);
  } else {
    Serial.println("INACTIVE");
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
  if (getWiFiOpMode() == WiFiOpMode::AP) {
    Serial.print("WiFi AP: Active, Stations: ");
    Serial.println(getStationCount());
  }
  Serial.printf("WiFi mode: %s\n", getWiFiOpModeName());
  if (getWiFiOpMode() == WiFiOpMode::STA) {
    Serial.printf("  STA SSID: %s\n", getSTASSID().c_str());
    Serial.printf("  STA IP:   %s\n", isSTAReady() ? getSTAIPAddress().c_str() : "(not connected)");
  }
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
    // Friendly prosign names
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

// Toggle mode - simplified to Koch ↔ Progtable only
void toggleMode() {
  // Stop Koch session if leaving Koch mode
  if (currentMode == MorseMode::Koch && isKochActive()) {
    stopKochSession();
  }

  if (currentMode == MorseMode::Koch) {
    currentMode = MorseMode::Progtable;
    Serial.println("Switched to Progtable mode (freeform text)\n");
    startTransmission("PROGTABLE");
  } else {
    currentMode = MorseMode::Koch;
    Serial.println("Switched to Koch Trainer mode");
    Serial.println("  Type 'koch start' to begin a session.\n");
    startTransmission("KOCH");
  }
}

// Set WPM
void setWPM(int newWPM) {
  if (newWPM < 1 || newWPM > 60) {
    Serial.println("ERROR: WPM must be between 1 and 60");
    return;
  }
  // Block speed changes during keyer memory playback — both ESP32 buzzer
  // and phone are locked to the ditMs captured at start time.
  if (isKeyerMemoryPlaying()) {
    Serial.println(">>> Speed change blocked during keyer playback");
    return;
  }
  WPM = newWPM;
  updateTiming();
  
  // Save to NVS so it persists across reboot
  if (isStorageReady()) {
    getPrefs().putUInt("wpm", WPM);
  }
  
  Serial.print("Speed set to ");
  Serial.print(WPM);
  Serial.println(" WPM (saved)");
  Serial.println();
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
      if (isEchoActive()) {
        Serial.println("Buzzer locked ON during echo training.\n");
      } else {
        setBuzzerEnabled(false);
        saveBuzzerToNVS();
        wsBroadcastBuzzerState(false);
      }
    } else {
      Serial.println("ERROR: Use 'buzzer on' or 'buzzer off'\n");
    }
    return;
  }

  // --- WiFi commands (work in any mode) ---
  if (command.equalsIgnoreCase("wifi show")) {
    Serial.println("\n=== WiFi Info ===");
    Serial.printf("  WiFi mode: %s\n", getWiFiOpModeName());
    if (getWiFiOpMode() == WiFiOpMode::STA) {
      Serial.printf("  SSID:      \"%s\"\n", getSTASSID().c_str());
      if (isSTAReady()) {
        Serial.printf("  Connected: YES — IP %s  RSSI %d dBm\n",
                      getSTAIPAddress().c_str(), getSTARSSI());
      } else {
        Serial.println("  Connected: NO (retrying in background)");
      }
    } else {
      Serial.println("  (AP mode active)");
    }
    Serial.println();
    return;
  }
  if (command.equalsIgnoreCase("wifi reset")) {
    resetSTACredentials();
    return;
  }

  // --- "stop" always works ---
  if (command.equalsIgnoreCase("koch stop") ||
      command.equalsIgnoreCase("stop")) {
    bool stopped = false;
    if (isKochActive()) { stopKochSession(); stopped = true; }
    if (isEchoActive()) { stopEchoTraining(); stopped = true; }
    if (isPracticeActive()) { stopPractice(); stopped = true; }
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

  if (command.equalsIgnoreCase("mode")) {
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
  // --- Practice sub-commands ---
  else if (command.equalsIgnoreCase("practice start")) {
    String text = command.substring(15);  // text after "practice start "
    if (text.length() > 0) {
      startPractice(text);
    } else {
      Serial.println("Usage: practice start <text>");
    }
  }
  else if (command.equalsIgnoreCase("practice file")) {
    startPracticeFromFile();
  }
  else if (command.equalsIgnoreCase("practice stop")) {
    stopPractice();
  }
  else {
    Serial.println("Unknown command. Type 'help' for options.\n");
  }
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
      if (isPracticeActive()) {
        Serial.println("\n[ESC] Stopping Practice...");
        stopPractice();
      }
      inputLine = "";
      continue;
    }

    // --- '!' panic stop — works from Arduino Serial Monitor ---
    if (c == '!' && inputLine.length() == 0 && Serial.available() == 0) {
      bool stopped = false;
      if (isKochActive())  { Serial.println("\n[!] Stopping Koch...");  stopKochSession();  stopped = true; }
      if (isEchoActive())  { Serial.println("\n[!] Stopping Echo...");  stopEchoTraining(); stopped = true; }
      if (isPracticeActive()) { Serial.println("\n[!] Stopping Practice..."); stopPractice(); stopped = true; }
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