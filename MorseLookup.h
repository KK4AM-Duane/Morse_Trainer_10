#pragma once
#include <Arduino.h>

// Morse code lookup mode
enum class MorseMode : uint8_t {
  Koch,        // Koch trainer (adaptive CW practice)
  Progtable    // Use PROGMEM direct lookup table
};

// Global variables
extern MorseMode currentMode;

// Lookup functions
String getMorseCode(char c);
String lookupMorseProgTable(char c);
String preprocessProsigns(String message);

// Reverse lookup: pattern (".-") -> character (returns '\0' if not found)
char decodeMorsePattern(const String &pattern);

// Morse equivalence: maps characters that share the same Morse pattern
// to a single canonical form so scoring treats them as interchangeable.
//   '=' and '[' (BT)  both -...-
//   '+' and '^' (AR)  both .-.-.
//   '&' and '<' (AS)  both .-...
//   '(' and '$' (KN)  both -.--.
//   '{' (CT) and '*' (KA) both -.-.-
// All other characters are returned unchanged.
char morseCanonical(char c);

// Returns true if two characters produce the same Morse pattern
bool morseEquivalent(char a, char b);