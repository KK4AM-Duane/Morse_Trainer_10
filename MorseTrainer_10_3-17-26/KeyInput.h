#pragma once
#include <Arduino.h>
#include "MorsePlayback.h"   // for signalOn/signalOff and DOT_MS

// Hardware
extern const unsigned int KEY_PIN;
extern const unsigned int PADDLE_DAH_PIN;

// Key input type — selects which decoder pollKeyInput() uses
enum class KeyType : uint8_t {
  Straight,   // Straight key on KEY_PIN (default)
  Iambic      // Iambic paddles on KEY_PIN (dit) + PADDLE_DAH_PIN (dah)
};

// Initialize internal state (call once in setup)
void initKeyInput();

// Non-blocking poller (call from loop) — dispatches to straight or iambic
void pollKeyInput();

// Key type selection (persisted to NVS)
void setKeyType(KeyType kt);
KeyType getKeyType();
const char* getKeyTypeName();   // "Straight" or "Iambic"