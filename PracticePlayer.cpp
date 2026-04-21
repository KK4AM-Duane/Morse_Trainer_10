#include "PracticePlayer.h"
#include "MorsePlayback.h"
#include "MorseWebSocket.h"
#include "MorseLookup.h"
#include "KochTrainer.h"
#include "EchoTrainer.h"
#include "CWKeyerMode.h"
#include <LittleFS.h>

// ============================================================
// Practice Player
//
// Feeds text to startTransmission() one word at a time.
// Always uses Progtable lookup for character resolution,
// regardless of the global currentMode setting.
// Waits for the playback engine to go Idle after each word,
// then inserts a proper ITU word gap (7 × DOT) before the next.
// ============================================================

enum class PracticeState : uint8_t {
  Idle,
  SendingFirstWord,  // first word has no leading gap
  WaitingPlayback,   // waiting for inter-word gap to elapse
  InterWordDelay     // ITU 7-dot word gap between words
};

static PracticeState pState = PracticeState::Idle;
static String practiceText;         // full text to play
static int    textPos = 0;          // current parse position in practiceText
static String currentWord;          // word currently being played
static uint16_t wordsSent = 0;
static uint16_t totalWords = 0;
static unsigned long nextWordAt = 0;

// Count total words in a string (space-separated)
static uint16_t countWords(const String &text) {
  uint16_t count = 0;
  bool inWord = false;
  for (int i = 0; i < (int)text.length(); i++) {
    if (text[i] == ' ' || text[i] == '\n' || text[i] == '\r' || text[i] == '\t') {
      inWord = false;
    } else {
      if (!inWord) count++;
      inWord = true;
    }
  }
  return count;
}

// Extract the next word from practiceText starting at textPos.
// Skips leading whitespace.  Returns empty string when done.
static String nextWord() {
  while (textPos < (int)practiceText.length()) {
    char c = practiceText[textPos];
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
      textPos++;
    } else {
      break;
    }
  }

  if (textPos >= (int)practiceText.length()) {
    return "";
  }

  int start = textPos;
  while (textPos < (int)practiceText.length()) {
    char c = practiceText[textPos];
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
      break;
    }
    textPos++;
  }

  return practiceText.substring(start, textPos);
}

// Send the next word to the playback engine using Progtable lookup,
// regardless of whatever mode the rest of the trainer is using.
// Returns true if a word was sent, false if no words remain.
static bool sendNextWord() {
  currentWord = nextWord();
  if (currentWord.length() == 0) {
    return false;
  }
  wordsSent++;
  Serial.printf("[%u/%u] ", wordsSent, totalWords);
  wsBroadcastPracticeWord(currentWord, wordsSent, totalWords);

  // Force Progtable for practice playback so all standard characters
  // resolve correctly, then restore whatever mode was active before.
  // skipProsigns=true: send letter pairs as individual characters,
  // not as prosigns (e.g. "AS" sends A then S, not the AS prosign).
  MorseMode savedMode = currentMode;
  currentMode = MorseMode::Progtable;
  startTransmission(currentWord, false, true);
  currentMode = savedMode;

  return true;
}

// --- Public API ---

void initPracticePlayer() {
  pState = PracticeState::Idle;
  practiceText = "";
  textPos = 0;
  currentWord = "";
  wordsSent = 0;
  totalWords = 0;
}

void startPractice(const String &text) {
  if (text.length() == 0) {
    Serial.println("Practice: no text provided.\n");
    return;
  }
  if (isCWKeyerMode()) {
    Serial.println("Practice unavailable in CW Keyer mode.\n");
    return;
  }
  if (isKochActive()) {
    Serial.println("Stop Koch training first ('koch stop').\n");
    return;
  }
  if (isEchoActive()) {
    Serial.println("Stop echo training first ('echo stop').\n");
    return;
  }
  if (playback.state != PlaybackState::Idle) {
    Serial.println("Wait for current playback to finish.\n");
    return;
  }

  practiceText = text;
  practiceText.toUpperCase();
  textPos = 0;
  wordsSent = 0;
  totalWords = countWords(practiceText);
  currentWord = "";
  nextWordAt = 0;

  Serial.printf("Practice: START (%u words at %u WPM)\n", totalWords, WPM);
  wsBroadcastPracticeSession(true, wordsSent, totalWords);

  // Send the first word immediately — no leading word gap
  if (!sendNextWord()) {
    Serial.println("Practice: no words found.\n");
    pState = PracticeState::Idle;
    return;
  }
  pState = PracticeState::SendingFirstWord;
}

bool startPracticeFromFile() {
  if (!LittleFS.exists("/practice.txt")) {
    Serial.println("Practice: /practice.txt not found in LittleFS.\n");
    return false;
  }
  File f = LittleFS.open("/practice.txt", "r");
  if (!f) {
    Serial.println("Practice: failed to open /practice.txt.\n");
    return false;
  }
  String text = f.readString();
  f.close();
  text.trim();
  if (text.length() == 0) {
    Serial.println("Practice: /practice.txt is empty.\n");
    return false;
  }
  Serial.printf("Practice: loaded %d bytes from /practice.txt\n", (int)text.length());
  startPractice(text);
  return true;
}

void stopPractice() {
  if (pState == PracticeState::Idle) return;
  pState = PracticeState::Idle;
  practiceText = "";
  currentWord = "";
  Serial.println("Practice: STOP");
  wsBroadcastPracticeSession(false, wordsSent, totalWords);
}

bool isPracticeActive() {
  return pState != PracticeState::Idle;
}

void practicePrintStats() {
  Serial.printf("Practice: %u/%u words sent at %u WPM\n",
                wordsSent, totalWords, WPM);
}

uint16_t getPracticeWordsSent()  { return wordsSent; }
uint16_t getPracticeTotalWords() { return totalWords; }
const String& getPracticeCurrentWord() { return currentWord; }

void updatePracticePlayer() {
  if (pState == PracticeState::Idle) return;

  unsigned long now = millis();

  // First word is already playing — just wait for it to finish
  if (pState == PracticeState::SendingFirstWord) {
    if (playback.state != PlaybackState::Idle) return;
    // First word done — schedule ITU word gap before next word
    nextWordAt = now + WORD_GAP_MS;
    pState = PracticeState::WaitingPlayback;
    return;
  }

  if (pState == PracticeState::WaitingPlayback) {
    // Wait for the inter-word gap to elapse
    if (now < nextWordAt) return;

    // Send next word
    if (!sendNextWord()) {
      // All done
      Serial.printf("\nPractice: COMPLETE (%u words)\n\n", wordsSent);
      pState = PracticeState::Idle;
      practiceText = "";
      wsBroadcastPracticeSession(false, wordsSent, totalWords);
      return;
    }
    pState = PracticeState::InterWordDelay;
    return;
  }

  if (pState == PracticeState::InterWordDelay) {
    // Wait for playback to finish, then schedule ITU word gap (7 × DOT)
    if (playback.state != PlaybackState::Idle) return;
    nextWordAt = now + WORD_GAP_MS;
    pState = PracticeState::WaitingPlayback;
  }
}