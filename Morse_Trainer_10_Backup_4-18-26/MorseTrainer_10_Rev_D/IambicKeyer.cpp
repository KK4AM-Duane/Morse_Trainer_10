#include "IambicKeyer.h"
#include "MorsePlayback.h"   // signalOn/Off, DOT_MS, DASH_MS, CHAR_GAP_MS
#include "MorseLookup.h"     // decodeMorsePattern
#include "EchoTrainer.h"     // echoSubmitDecodedChar

// ============================================================
// Iambic Mode B keyer
//
// Paddle inputs are active-low (pressed = LOW).
//   KEY_PIN       (GPIO32) = dit paddle  (shared with straight key)
//   PADDLE_DAH_PIN(GPIO33) = dah paddle
//
// The keyer produces correctly-timed dits and dahs with proper
// element gaps.  Mode B means: if both paddles are squeezed, the
// keyer alternates dit-dah and finishes the CURRENT element after
// both are released before stopping.
//
// After a character gap (3 × DOT_MS of idle), the accumulated
// pattern is decoded and forwarded to echoSubmitDecodedChar().
// ============================================================

static const uint8_t MAX_PATTERN_LEN = 8;

// --- Keyer state machine ---
enum class IambicState : uint8_t {
  Idle,         // No element playing, waiting for paddle press
  SendingDit,   // Dit tone is ON
  SendingDah,   // Dah tone is ON
  ElementGap    // Inter-element silence (1 × DOT)
};

static IambicState iState = IambicState::Idle;
static unsigned long elementStartMs = 0;    // when current element/gap started
static unsigned long lastElementEndMs = 0;  // when last element gap finished

// Pattern being accumulated for current character
static String iPattern;

// "Opposite" latch for Mode B: if the other paddle was pressed during
// the current element, queue it so it plays after the gap.
static bool ditLatch = false;
static bool dahLatch = false;

// Last element sent — used for Mode B alternation
static char lastElement = '\0';   // '.' or '-'

// --- Helpers ---

static inline bool ditPressed() { return digitalRead(KEY_PIN) == LOW; }
static inline bool dahPressed() { return digitalRead(PADDLE_DAH_PIN) == LOW; }

// Start a dit element
static void startDit() {
  iState = IambicState::SendingDit;
  elementStartMs = millis();
  signalOn();
  ditLatch = false;
  dahLatch = false;
  lastElement = '.';
}

// Start a dah element
static void startDah() {
  iState = IambicState::SendingDah;
  elementStartMs = millis();
  signalOn();
  ditLatch = false;
  dahLatch = false;
  lastElement = '-';
}

// Begin the inter-element gap after a dit or dah
static void startGap(char element) {
  signalOff();
  if (iPattern.length() < MAX_PATTERN_LEN) {
    iPattern += element;
  }
  iState = IambicState::ElementGap;
  elementStartMs = millis();
}

// --- Public API ---

void initIambicKeyer() {
  iState = IambicState::Idle;
  elementStartMs = 0;
  lastElementEndMs = millis();
  iPattern = "";
  ditLatch = false;
  dahLatch = false;
  lastElement = '\0';
}

bool isIambicBusy() {
  return iState != IambicState::Idle;
}

void pollIambicKeyer() {
  unsigned long now = millis();

  switch (iState) {

    // ---- IDLE: waiting for a paddle press or decoding on char gap ----
    case IambicState::Idle: {
      bool dit = ditPressed();
      bool dah = dahPressed();

      if (dit && dah) {
        // Squeeze — start with dit (Mode B convention)
        startDit();
      } else if (dit) {
        startDit();
      } else if (dah) {
        startDah();
      } else {
        // Neither paddle pressed — check for character gap
        if (iPattern.length() > 0 && (now - lastElementEndMs) > CHAR_GAP_MS) {
          char decoded = decodeMorsePattern(iPattern);
          if (decoded != '\0') {
            Serial.printf("Iambic decoded: %c from %s\n", decoded, iPattern.c_str());
            echoSubmitDecodedChar(decoded);
          } else {
            Serial.printf("Iambic decoded: ? from %s (not found)\n", iPattern.c_str());
          }
          iPattern = "";
        }
      }
      break;
    }

    // ---- SENDING DIT: tone ON for 1 × DOT ----
    case IambicState::SendingDit: {
      // Sample the opposite paddle during this element (Mode B latch)
      if (dahPressed()) dahLatch = true;

      if ((now - elementStartMs) >= DOT_MS) {
        startGap('.');
      }
      break;
    }

    // ---- SENDING DAH: tone ON for 3 × DOT ----
    case IambicState::SendingDah: {
      // Sample the opposite paddle during this element (Mode B latch)
      if (ditPressed()) ditLatch = true;

      if ((now - elementStartMs) >= DASH_MS) {
        startGap('-');
      }
      break;
    }

    // ---- ELEMENT GAP: silence for 1 × DOT between dits/dahs ----
    case IambicState::ElementGap: {
      // Continue sampling latches during the gap
      if (ditPressed()) ditLatch = true;
      if (dahPressed()) dahLatch = true;

      if ((now - elementStartMs) >= DOT_MS) {
        lastElementEndMs = millis();

        // Decide what to do next.
        // Mode B priority: latched opposite > currently held > stop.
        bool dit = ditPressed();
        bool dah = dahPressed();

        if (ditLatch && dahLatch) {
          // Both latched — alternate from the last element
          if (lastElement == '.') startDah(); else startDit();
        }
        else if (ditLatch) {
          startDit();
        }
        else if (dahLatch) {
          startDah();
        }
        else if (dit && dah) {
          // Squeeze still held — alternate
          if (lastElement == '.') startDah(); else startDit();
        }
        else if (dit) {
          startDit();
        }
        else if (dah) {
          startDah();
        }
        else {
          // Nothing pressed and nothing latched — go idle
          iState = IambicState::Idle;
        }
      }
      break;
    }
  }
}