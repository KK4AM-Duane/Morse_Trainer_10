#pragma once
#include <Arduino.h>
#include <vector>

// Mode selection using enum class
enum class MorseMode : uint8_t {
  Koch,        // Koch trainer (adaptive CW practice)
  Vector,      // Use vector-based table (dynamic)
  Progmem,     // Use PROGMEM binary tree
  Progtable    // Use PROGMEM direct lookup table
};

// Morse code class definition
class MorseMap {
private:
  char ch;
  String code;
  
public:
  MorseMap(char character = '\0', const String &morseCode = "")
    : ch(character), code(morseCode) {}
  
  char getCharacter() const { return ch; }
  String getCode() const { return code; }
};

// Global variables
extern MorseMode currentMode;
extern std::vector<MorseMap> morseTable;

// Lookup functions
String getMorseCode(char c);
String charToMorse(char c);
String lookupMorse(char c);
String lookupMorseProgTable(char c);
String preprocessProsigns(String message);

// Table management
void initializeVectorTable();
void addMorseEntry(char character, const String &code);

// Helper functions
bool searchSubtree(int index, char target, int level);

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