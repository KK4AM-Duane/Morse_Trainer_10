#include "KochTrainer.h"
#include "MorsePlayback.h"
#include "MorseLookup.h"
#include "MorseCommands.h"
#include "MorseWebSocket.h"
#include "Storage.h"

// ── Koch character order ───────────────────────────────────────────
// Base 44 + prosigns AR(^) BT([) SK(|) BK(>) AS(<) KN($) — total 50
const char KOCH_ORDER_FULL[] = "KMURESNAPTLWI.JZ=FOY,VG5/Q92H38B?47C1D60X^[|><$";
const int  KOCH_ORDER_LEN    = sizeof(KOCH_ORDER_FULL) - 1;  // exclude null

// ── Per-character statistics ───────────────────────────────────────
static CharStats charStats[KOCH_MAX_CHARS];

// ── Trainer state ──────────────────────────────────────────────────
static TrainerState trainer;
static KochState    kochState = KochState::Idle;

// The character currently being tested
static char          pendingChar       = '\0';
static unsigned long charSentAtMs      = 0;     // millis() when playback finished
static unsigned long resultShownAtMs   = 0;     // millis() when result was displayed
static const unsigned long RESULT_PAUSE_MS = 800; // pause between rounds

// LED feedback flash state (driven during ShowResult)
static bool lastAnswerCorrect = false;

// Pause / Lost state
static uint8_t       consecutiveTimeouts = 0;
static unsigned long lostEnteredAtMs     = 0;

// Periodic checkpoint counter (counts since last NVS save)
static uint16_t charsSinceLastSave = 0;
static bool     nvsDirty           = false;
static unsigned long lastNvsSaveMs = 0;
static const unsigned long NVS_SAVE_MIN_INTERVAL_MS = 5000; // min spacing between NVS writes

// ── WaitReady handshake state ──────────────────────────────────────
static unsigned long readySentAtMs   = 0;     // millis() when challenge was broadcast
static volatile bool phoneReadyFlag  = false;  // set true by kochPhoneReady()

// ── Alphabetical grid sort tracking ────────────────────────────────
// When the user has unlocked ALL Koch characters (max lesson) and gets
// SORT_GRID_STREAK consecutive correct answers, the phone grid switches
// from Koch order to alphabetical order for easier button location.
static uint16_t maxLessonStreak    = 0;    // consecutive correct at max lesson
static bool     gridSortedFlag     = false; // latched true once streak is reached
static bool     gridSortNotified   = false; // true once we've told the phone

// ── Helpers ────────────────────────────────────────────────────────

// Return a friendly display name for Koch prosign placeholder characters
static String kochProsignName(char c) {
  switch (c) {
    case '^': return "AR";
    case '[': return "BT";
    case '|': return "SK";
    case '>': return "BK";
    case '<': return "AS";
    case '$': return "KN";
    default:  return String(c);
  }
}

static void notifyKoch(const String &text) {
  Serial.println(text);
  wsBroadcastKochEvent(text);
}

// Start buzzer + serial output for the pending character.
// Called when the phone ACKs "ready" or the timeout expires.
static void beginPlayback() {
  // Print the first element to serial (this was the missing piece
  // that caused the race condition — the first dit/dah was never printed)
  char element = playback.currentMorseCode[0];
  Serial.print("? (");
  Serial.print(element);

  // Broadcast "go" so the phone starts its pre-prepared audio
  wsBroadcastKochGo();

  // Kick off the buzzer on the first element
  signalOn();
  playback.state          = PlaybackState::SignalOn;
  playback.stateStartTime = millis();
  playback.stateDuration  = (element == '.') ? DOT_MS : DASH_MS;

  kochState = KochState::WaitPlayback;
}

// ── initKochTrainer ────────────────────────────────────────────────
void initKochTrainer() {
  // Set safe defaults first
  trainer.kochLesson      = 2;
  trainer.sessionCorrect  = 0;
  trainer.sessionTotal    = 0;
  trainer.recentCorrect   = 0;
  trainer.recentTotal     = 0;

  for (int i = 0; i < KOCH_ORDER_LEN && i < KOCH_MAX_CHARS; i++) {
    charStats[i].character     = KOCH_ORDER_FULL[i];
    charStats[i].probability   = PROB_INITIAL;
    charStats[i].totalSent     = 0;
    charStats[i].totalCorrect  = 0;
    charStats[i].avgReactionMs = 0;
  }

  // Overwrite with NVS data if available
  loadKochFromNVS();

  kochState = KochState::Idle;
  lastNvsSaveMs = millis();
  // (rest unchanged)
}

// ── Character selection (weighted probability) ─────────────────────
char selectKochCharacter() {
  int activeCount = trainer.kochLesson;
  if (activeCount > KOCH_ORDER_LEN) activeCount = KOCH_ORDER_LEN;
  if (activeCount < 2) activeCount = 2;

  // Sum weights
  long totalWeight = 0;
  for (int i = 0; i < activeCount; i++) {
    totalWeight += charStats[i].probability;
  }
  if (totalWeight <= 0) totalWeight = activeCount;  // safety

  long target     = random(0, totalWeight);
  long cumulative = 0;
  for (int i = 0; i < activeCount; i++) {
    cumulative += charStats[i].probability;
    if (cumulative > target) {
      return charStats[i].character;
    }
  }
  return charStats[activeCount - 1].character;  // fallback
}

// ── Find CharStats index for a character ───────────────────────────
static int findCharIndex(char c) {
  c = toupper(c);
  int activeCount = trainer.kochLesson;
  if (activeCount > KOCH_ORDER_LEN) activeCount = KOCH_ORDER_LEN;
  for (int i = 0; i < activeCount; i++) {
    if (charStats[i].character == c) return i;
  }
  return -1;
}

// ── Koch progression check ─────────────────────────────────────────
static void checkKochProgression() {
  if (trainer.recentTotal < EVAL_WINDOW) return;

  float accuracy = (float)trainer.recentCorrect / (float)trainer.recentTotal;

  // --- Koch lesson advancement ---
  if (accuracy >= ADVANCE_THRESHOLD && trainer.kochLesson < KOCH_ORDER_LEN) {
    trainer.kochLesson++;
    int newIdx = trainer.kochLesson - 1;
    charStats[newIdx].probability = PROB_NEW_CHAR;

    String msg = ">>> New character unlocked: ";
    char nc = KOCH_ORDER_FULL[newIdx];
    // Show friendly name for prosigns
    String display = kochProsignName(nc);
    if (display.length() > 1) {
      msg += display + " (" + String(nc) + ")";
    } else {
      msg += nc;
    }
    msg += "  [Lesson " + String(trainer.kochLesson) + "]";
    notifyKoch(msg);

    // A new char was added — reset the sort streak since grid just changed
    maxLessonStreak  = 0;
    gridSortedFlag   = false;
    gridSortNotified = false;

    // Push updated character list to all connected clients
    wsBroadcastKochChars();
    nvsDirty = true;
  }

  // --- Speed adaptation ---
  uint32_t avgReaction = 0;
  int activeCount = trainer.kochLesson;
  if (activeCount > KOCH_ORDER_LEN) activeCount = KOCH_ORDER_LEN;
  int countWithData = 0;
  for (int i = 0; i < activeCount; i++) {
    if (charStats[i].totalSent > 0) {
      avgReaction += charStats[i].avgReactionMs;
      countWithData++;
    }
  }
  if (countWithData > 0) avgReaction /= countWithData;

  if (accuracy >= SPEED_UP_THRESHOLD && (int)avgReaction < REACTION_FAST_MS) {
    int newWPM = WPM + SPEED_UP_DELTA;
    if (newWPM > MAX_WPM_KOCH) newWPM = MAX_WPM_KOCH;
    if (newWPM != (int)WPM) {
      setWPM(newWPM);
      wsBroadcastSpeedChange(WPM);
      notifyKoch(">>> Speed UP to " + String(WPM) + " WPM");
      nvsDirty = true;
    }
  } else if (accuracy < SPEED_DOWN_THRESHOLD) {
    int newWPM = WPM - SPEED_DOWN_DELTA;
    if (newWPM < MIN_WPM_KOCH) newWPM = MIN_WPM_KOCH;
    if (newWPM != (int)WPM) {
      setWPM(newWPM);
      wsBroadcastSpeedChange(WPM);
      notifyKoch(">>> Speed DOWN to " + String(WPM) + " WPM");
      nvsDirty = true;
    }
  }

  // Reset the evaluation window
  trainer.recentCorrect = 0;
  trainer.recentTotal   = 0;

  // Persist after any lesson/speed change
  saveKochToNVS(true);
}

// ── Score an answer ────────────────────────────────────────────────
static void scoreAnswer(char expected, char received, unsigned long reactionMs) {
  expected = toupper(expected);
  received = toupper(received);
  bool correct = morseEquivalent(expected, received);

  int idx = findCharIndex(expected);
  if (idx >= 0) {
    charStats[idx].totalSent++;
    if (correct) {
      charStats[idx].totalCorrect++;
      int p = charStats[idx].probability - PROB_RIGHT_DELTA;
      charStats[idx].probability = (p < PROB_MIN) ? PROB_MIN : (uint8_t)p;
    } else {
      int p = charStats[idx].probability + PROB_WRONG_DELTA;
      charStats[idx].probability = (p > PROB_MAX) ? PROB_MAX : (uint8_t)p;
    }
    // Rolling average reaction time
    if (charStats[idx].totalSent <= 1) {
      charStats[idx].avgReactionMs = reactionMs;
    } else {
      charStats[idx].avgReactionMs =
        (charStats[idx].avgReactionMs * 3 + reactionMs) / 4;
    }
  }

  // Session totals
  trainer.sessionTotal++;
  if (correct) trainer.sessionCorrect++;

  // Sliding evaluation window
  trainer.recentTotal++;
  if (correct) trainer.recentCorrect++;

  // Record for LED flash in ShowResult state
  lastAnswerCorrect = correct;

  // ── Track streak at max lesson for alphabetical grid sort ──
  if (trainer.kochLesson >= KOCH_ORDER_LEN) {
    if (correct) {
      maxLessonStreak++;
      if (!gridSortedFlag && maxLessonStreak >= SORT_GRID_STREAK) {
        gridSortedFlag = true;
        gridSortNotified = false;   // will notify on next result broadcast
        notifyKoch(">>> All characters mastered! Grid switching to alphabetical order.");
      }
    } else {
      maxLessonStreak = 0;
      // If the grid was sorted but the user makes an error, keep it sorted.
      // Once earned, the alphabetical layout persists for the session.
    }
  }

  // Build result string
  String result;
  if (correct) {
    result = "✓ Correct: " + String(expected) + "  (" + String(reactionMs) + "ms)";
  } else {
    result = "✗ Wrong: sent " + String(expected) + ", got " + String(received) +
             "  (" + String(reactionMs) + "ms)";
  }
  result += "  [" + String(trainer.sessionCorrect) + "/" +
            String(trainer.sessionTotal) + "]";

  Serial.println(result);

  // Broadcast result to phone (includes sort_grid flag)
  wsBroadcastKochResult(expected, received, correct, reactionMs,
                        trainer.sessionCorrect, trainer.sessionTotal,
                        trainer.kochLesson);

  // Periodic checkpoint — mark dirty and attempt debounced save
  charsSinceLastSave++;
  nvsDirty = true;
  saveKochToNVS(); // debounced; will skip if too soon

  // Check progression every EVAL_WINDOW characters
  if (trainer.recentTotal >= EVAL_WINDOW) {
    checkKochProgression();
  }
}

// ── Start / Stop ───────────────────────────────────────────────────
void startKochSession() {
  if (kochState != KochState::Idle) {
    Serial.println("Koch session already running. Type 'koch stop' to end it.");
    return;
  }

  // Reset session counters (keep charStats & lesson across sessions)
  trainer.sessionCorrect = 0;
  trainer.sessionTotal   = 0;
  trainer.recentCorrect  = 0;
  trainer.recentTotal    = 0;
  charsSinceLastSave     = 0;
  maxLessonStreak        = 0;
  gridSortedFlag         = false;
  gridSortNotified       = false;

  // Ensure a sane starting speed
  if (WPM < (unsigned int)MIN_WPM_KOCH) {
    setWPM(MIN_WPM_KOCH);
  }

  kochState = KochState::SelectAndPlay;

  Serial.println("\n======================================");
  Serial.println("   Koch Trainer — Session Started");
  Serial.printf("   Lesson %d  (%d chars)  %d WPM\n",
                trainer.kochLesson, trainer.kochLesson, WPM);
  Serial.println("   Type the character you hear.");
  Serial.println("   'koch stop' to end session.");
  Serial.println("======================================\n");

  wsBroadcastKochSession(true, trainer.kochLesson, WPM);

  // Push the active character list so the phone builds its grid
  wsBroadcastKochChars();
}

void stopKochSession() {
  if (kochState == KochState::Idle) return;

  // Abort any in-progress playback
  if (playback.state != PlaybackState::Idle) {
    signalOff();
    playback.state = PlaybackState::Idle;
  }

  kochState   = KochState::Idle;
  pendingChar = '\0';

  Serial.println("\n======================================");
  Serial.println("   Koch Trainer — Session Ended");
  Serial.printf("   Score: %d / %d", trainer.sessionCorrect, trainer.sessionTotal);
  if (trainer.sessionTotal > 0) {
    Serial.printf("  (%.0f%%)", 100.0f * trainer.sessionCorrect / trainer.sessionTotal);
  }
  Serial.println();
  Serial.printf("   Lesson: %d  WPM: %d\n", trainer.kochLesson, WPM);
  Serial.println("======================================\n");
  Serial.print("> ");

  // Persist progress
  saveKochToNVS(true);

  wsBroadcastKochSession(false, trainer.kochLesson, WPM);
}

void pauseKochSession() {
  if (kochState == KochState::Idle || kochState == KochState::Paused) return;

  // Abort any in-progress playback
  if (playback.state != PlaybackState::Idle) {
    signalOff();
    playback.state = PlaybackState::Idle;
  }

  // Ensure feedback LEDs are off
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);

  pendingChar = '\0';
  kochState = KochState::Paused;

  Serial.println("\n>>> Koch Trainer — PAUSED");
  Serial.print("> ");

  // Persist progress on pause (user may power off while paused)
  saveKochToNVS(true);

  wsBroadcastKochPause(true);
}

void resumeKochSession() {
  if (kochState != KochState::Paused) return;

  kochState = KochState::SelectAndPlay;

  Serial.println("\n>>> Koch Trainer — RESUMED");

  wsBroadcastKochPause(false);
}


bool isKochActive() {
  return kochState != KochState::Idle;
}

bool isKochPaused() {
  return kochState == KochState::Paused;
}

bool isKochGridSorted() {
  return gridSortedFlag;
}

// ── Phone handshake callback ───────────────────────────────────────
void kochPhoneReady() {
  phoneReadyFlag = true;
}

// ── Replay current challenge on the buzzer ─────────────────────────
void kochReplayChallenge() {
  // Only allow replay while waiting for the user's answer
  if (kochState != KochState::WaitResponse) return;
  if (pendingChar == '\0') return;

  // Abort any in-progress playback (shouldn't be any, but be safe)
  if (playback.state != PlaybackState::Idle) {
    signalOff();
    playback.state = PlaybackState::Idle;
  }

  // Re-prepare the playback context for the same character
  String charStr(pendingChar);

  MorseMode savedMode = currentMode;
  currentMode = MorseMode::Progtable;

  playback.message          = charStr;
  playback.messageIndex     = 0;
  playback.codeIndex        = 0;
  playback.echoMode         = true;
  playback.currentMorseCode = getMorseCode(pendingChar);

  currentMode = savedMode;

  if (playback.currentMorseCode.length() == 0) return;

  // Re-enter the WaitReady handshake so the phone and buzzer sync up
  phoneReadyFlag = false;
  wsBroadcastKochChallenge(trainer.kochLesson, trainer.sessionTotal + 1,
                            playback.currentMorseCode, DOT_MS);
  readySentAtMs = millis();
  kochState = KochState::WaitReady;

  Serial.println("[Replay]");
}

// ── Submit answer (called from serial or WebSocket) ────────────────
void kochSubmitAnswer(char answer) {
  if (kochState != KochState::WaitResponse) return;
  if (pendingChar == '\0') return;

  unsigned long reactionMs = millis() - charSentAtMs;

  // Track consecutive timeouts for Lost state detection
  if (answer == '?') {
    consecutiveTimeouts++;
  } else {
    consecutiveTimeouts = 0;
  }

  scoreAnswer(pendingChar, answer, reactionMs);

  // Check if user is lost (N consecutive timeouts)
  if (consecutiveTimeouts >= LOST_TIMEOUT_THRESHOLD) {
    consecutiveTimeouts = 0;

    // Slow down
    int newWPM = (int)WPM - SPEED_DOWN_DELTA;
    if (newWPM < MIN_WPM_KOCH) newWPM = MIN_WPM_KOCH;
    if (newWPM != (int)WPM) {
      setWPM(newWPM);
      wsBroadcastSpeedChange(WPM);
    }

    notifyKoch(">>> Slowing down — " + String(LOST_TIMEOUT_THRESHOLD) +
               " consecutive timeouts  (" + String(WPM) + " WPM)");

    lostEnteredAtMs = millis();
    kochState = KochState::Lost;
    return;
  }

  resultShownAtMs = millis();
  kochState = KochState::ShowResult;
}

// ── Non-blocking update (call from loop()) ─────────────────────────
void updateKochTrainer() {
  if (kochState == KochState::Idle) return;

  switch (kochState) {

    case KochState::SelectAndPlay: {
      if (playback.state != PlaybackState::Idle) return;  // wait for engine

      pendingChar = selectKochCharacter();
      String charStr(pendingChar);

      // Use Progtable for Koch (it has the prosigns)
      MorseMode savedMode = currentMode;
      currentMode = MorseMode::Progtable;

      // Prepare playback context — do NOT start the buzzer yet
      playback.message          = charStr;
      playback.messageIndex     = 0;
      playback.codeIndex        = 0;
      playback.echoMode         = true;   // suppress normal "Done" prompt
      playback.currentMorseCode = getMorseCode(pendingChar);

      currentMode = savedMode;  // restore

      if (playback.currentMorseCode.length() == 0) {
        Serial.printf("[Koch: '%c' has no morse code — skipping]\n", pendingChar);
        kochState = KochState::SelectAndPlay;
        return;
      }

      // Send challenge to phone so it can prepare its Web Audio pipeline.
      // The buzzer does NOT start yet — we wait for the phone to ACK "ready"
      // (or timeout) so all outputs fire simultaneously.
      phoneReadyFlag = false;
      wsBroadcastKochChallenge(trainer.kochLesson, trainer.sessionTotal + 1,
                                playback.currentMorseCode, DOT_MS);
      readySentAtMs = millis();
      kochState = KochState::WaitReady;
      break;
    }

    case KochState::WaitReady: {
      // Phone has ACK'd — start immediately
      if (phoneReadyFlag) {
        phoneReadyFlag = false;
        beginPlayback();
        break;
      }
      // Timeout — no phone connected or phone is slow; start anyway
      if (millis() - readySentAtMs >= READY_TIMEOUT_MS) {
        beginPlayback();
      }
      break;
    }

    case KochState::WaitPlayback:
      if (playback.state == PlaybackState::Idle) {
        // Playback finished — record the timestamp and wait for answer
        charSentAtMs = millis();
        Serial.print(" ");
        kochState = KochState::WaitResponse;
      }
      break;

    case KochState::WaitResponse:
      // Timeout after 10 seconds — treat as wrong
      if (millis() - charSentAtMs > 10000) {
        Serial.println("(timeout)");
        kochSubmitAnswer('?');  // score as wrong
      }
      break;

    case KochState::ShowResult: {
      unsigned long elapsed = millis() - resultShownAtMs;

      // ── LED feedback flash ──
      // Correct: green LED single 200 ms blink
      // Wrong:   red LED three rapid 100 ms blinks
      if (lastAnswerCorrect) {
        digitalWrite(LED_GREEN_PIN, elapsed < 200 ? HIGH : LOW);
        digitalWrite(LED_RED_PIN, LOW);
      } else {
        unsigned long cycle = elapsed % 200;   // 100 ms on + 100 ms off
        bool on = (cycle < 100) && (elapsed < 600);
        digitalWrite(LED_RED_PIN, on ? HIGH : LOW);
        digitalWrite(LED_GREEN_PIN, LOW);
      }

      if (elapsed >= RESULT_PAUSE_MS) {
        digitalWrite(LED_GREEN_PIN, LOW);
        digitalWrite(LED_RED_PIN, LOW);
        kochState = KochState::SelectAndPlay;
      }
      break;
    }

    case KochState::Paused:
      // Nothing to do — waiting for resumeKochSession()
      break;

    case KochState::Lost: {
      // Recovery pause with alternating LED blink
      unsigned long elapsed = millis() - lostEnteredAtMs;
      unsigned long cycle = elapsed % 400;
      digitalWrite(LED_GREEN_PIN, cycle < 200 ? HIGH : LOW);
      digitalWrite(LED_RED_PIN,   cycle >= 200 ? HIGH : LOW);

      if (elapsed >= LOST_PAUSE_MS) {
        digitalWrite(LED_GREEN_PIN, LOW);
        digitalWrite(LED_RED_PIN, LOW);
        kochState = KochState::SelectAndPlay;
      }
      break;
    }

    default:
      break;
  }
}

// ── Accessors ──────────────────────────────────────────────────────
uint8_t          getKochLesson()         { return trainer.kochLesson; }
uint16_t         getKochSessionCorrect() { return trainer.sessionCorrect; }
uint16_t         getKochSessionTotal()   { return trainer.sessionTotal; }
const CharStats* getKochCharStats()      { return charStats; }
int              getKochActiveCount()    { return min((int)trainer.kochLesson, KOCH_ORDER_LEN); }

// ── NVS persistence ────────────────────────────────────────────────
void saveKochToNVS(bool force) {
  if (!isStorageReady()) return;

  unsigned long now = millis();
  bool checkpointReady = (charsSinceLastSave >= CHECKPOINT_INTERVAL);
  bool timeReady = (now - lastNvsSaveMs) >= NVS_SAVE_MIN_INTERVAL_MS;

  if (!force) {
    if (!nvsDirty) return;
    if (!checkpointReady && !timeReady) return;
  }

  Preferences& p = getPrefs();
  p.putUChar("kochLesson", trainer.kochLesson);
  p.putUInt("wpm", WPM);
  p.putBytes("charStats", charStats,
             sizeof(CharStats) * min(KOCH_ORDER_LEN, (int)KOCH_MAX_CHARS));

  lastNvsSaveMs = now;
  charsSinceLastSave = 0;
  nvsDirty = false;
  Serial.printf(">>> NVS saved: Lesson %d, %d WPM\n", trainer.kochLesson, WPM);
}

void loadKochFromNVS() {
  if (!isStorageReady()) return;

  Preferences& p = getPrefs();

  trainer.kochLesson = p.getUChar("kochLesson", 2);
  if (trainer.kochLesson < 2) trainer.kochLesson = 2;
  if (trainer.kochLesson > KOCH_ORDER_LEN) trainer.kochLesson = KOCH_ORDER_LEN;

  unsigned int savedWPM = p.getUInt("wpm", 20);
  if (savedWPM >= 1 && savedWPM <= 60) {
    WPM = savedWPM;
  }

  size_t expected = sizeof(CharStats) * min(KOCH_ORDER_LEN, (int)KOCH_MAX_CHARS);
  size_t loaded   = p.getBytes("charStats", charStats, expected);

  if (loaded != expected) {
    // Blob missing or wrong size — re-init defaults (already set by caller)
    Serial.println("  NVS: no saved charStats — using defaults");
  } else {
    Serial.printf("  NVS loaded: Lesson %d, %d WPM, %d char stats\n",
                  trainer.kochLesson, WPM, KOCH_ORDER_LEN);
  }
}

// ── Reset all Koch progress ────────────────────────────────────────
void resetKochProgress() {
  // Stop any active session first
  if (isKochActive()) {
    stopKochSession();
  }

  // Clear all NVS keys in the cwtrainer namespace
  if (isStorageReady()) {
    Preferences& p = getPrefs();
    p.clear();
    Serial.println(">>> NVS cleared");
  }

  // Reinitialize to defaults (same as initKochTrainer but explicit)
  trainer.kochLesson      = 2;
  trainer.sessionCorrect  = 0;
  trainer.sessionTotal    = 0;
  trainer.recentCorrect   = 0;
  trainer.recentTotal     = 0;

  for (int i = 0; i < KOCH_ORDER_LEN && i < KOCH_MAX_CHARS; i++) {
    charStats[i].character     = KOCH_ORDER_FULL[i];
    charStats[i].probability   = PROB_INITIAL;
    charStats[i].totalSent     = 0;
    charStats[i].totalCorrect  = 0;
    charStats[i].avgReactionMs = 0;
  }

  charsSinceLastSave = 0;

  // Reset WPM to default
  setWPM(20);

  // Re-save defaults to NVS (so loadKochFromNVS sees clean state)
  nvsDirty = true;
  charsSinceLastSave = CHECKPOINT_INTERVAL; // force immediate write
  saveKochToNVS(true);

  // Restore buzzer to default (on) since prefs.clear() wiped it
  setBuzzerEnabled(true);
  saveBuzzerToNVS();

  Serial.println("\n======================================");
  Serial.println("   Koch Progress — RESET");
  Serial.println("   Lesson: 2, WPM: 20, Stats cleared");
  Serial.println("======================================\n");
  Serial.print("> ");

  // Notify all connected clients
  wsBroadcastStatus();
  wsBroadcastKochChars();
  wsBroadcastSpeedChange(WPM);
  wsBroadcastBuzzerState(true);
}