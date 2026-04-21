#pragma once

#include <Arduino.h>
#include "PracticePlayer.h"    // for isPracticeActive / startPractice / stopPractice

// Hardware pins for mode switch and TX relay (defined in .ino)
extern const unsigned int MODE_SWITCH_PIN;   // GPIO18: HIGH = Trainer, LOW = CW Keyer
extern const unsigned int TX_KEY_PIN;        // GPIO23: output to external transmitter

// System-wide operating mode
enum class OperatingMode : uint8_t {
  Trainer,    // Normal CW trainer operation (echo, Koch, playback)
  CWKeyer     // Passthrough keyer — paddles/key drive TX_KEY_PIN + sidetone
};

// Initialize pins and state (call once from setup, after NVS init)
void initCWKeyerMode();

// Poll the hardware switch and manage mode transitions (call from loop)
void pollModeSwitch();

// Current operating mode
OperatingMode getOperatingMode();
const char* getOperatingModeName();  // "Trainer" or "CWKeyer"

// True when in CW Keyer mode (shorthand for guards)
bool isCWKeyerMode();

// ── Keyer Memory (M1–M6) ──────────────────────────────────────────
static const int KEYER_MEM_COUNT   = 6;
static const int KEYER_MEM_SLOT_SZ = 1024;  // fixed NVS blob size per slot
static const int KEYER_MEM_MAX_LEN = 1023;  // max usable chars (last byte = '\0'

// Load all memories from NVS (called from initCWKeyerMode)
void loadKeyerMemories();

// Save a single memory slot (0-based index, 0–5)
void setKeyerMemory(int slot, const String &text);

// Get memory contents (empty string = not programmed)
String getKeyerMemory(int slot);

// Play a keyer memory out through the TX output (keyer mode only).
// Uses the playback engine with echoMode=true to skip phone audio.
// Drives TX_KEY_PIN during playback.
void playKeyerMemory(int slot);

// True if a keyer memory playback is in progress
bool isKeyerMemoryPlaying();

// Stop keyer memory playback
void stopKeyerMemoryPlayback();