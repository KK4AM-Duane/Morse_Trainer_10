#include "MorseLookup.h"
#include "Morse_Table.h"

// Global variables
MorseMode currentMode = MorseMode::Koch;

// Look up Morse code using PROGMEM direct lookup table
String lookupMorseProgTable(char c) {
  c = toupper(c);
  
  for (int i = 0; i < MORSE_LOOKUP_LEN; i++) {
    char tableChar = pgm_read_byte(&MORSE_LOOKUP[i].character);
    
    if (tableChar == c) {
      const char* codePtr = (const char*)pgm_read_ptr(&MORSE_LOOKUP[i].code);
      char buffer[30];
      strcpy_P(buffer, codePtr);
      return String(buffer);
    }
  }
  return "";
}

// Get morse code based on current mode
// (Always uses Progtable — mode distinction kept for Koch trainer logic)
String getMorseCode(char c) {
  return lookupMorseProgTable(c);
}

// Preprocess message to convert prosign sequences to single characters
String preprocessProsigns(String message) {
  String result = "";
  message.toUpperCase();
  
  for (int i = 0; i < (int)message.length(); i++) {
    bool converted = false;
    
    // Check for 3-character prosigns first
    if (i + 2 < (int)message.length()) {
      String tri = message.substring(i, i + 3);
      if (tri == "SOS") { result += '\\'; i += 2; converted = true; }
    }
    
    // Check for 2-character prosigns
    if (!converted && i + 1 < (int)message.length()) {
      String di = message.substring(i, i + 2);
      if      (di == "AA") { result += '~'; i += 1; converted = true; }
      else if (di == "AR") { result += '^'; i += 1; converted = true; }
      else if (di == "AS") { result += '<'; i += 1; converted = true; }
      else if (di == "BK") { result += '>'; i += 1; converted = true; }
      else if (di == "BT") { result += '['; i += 1; converted = true; }
      else if (di == "CL") { result += ']'; i += 1; converted = true; }
      else if (di == "CT") { result += '{'; i += 1; converted = true; }
      else if (di == "DO") { result += '}'; i += 1; converted = true; }
      else if (di == "HH") { result += '#'; i += 1; converted = true; }
      else if (di == "KA") { result += '*'; i += 1; converted = true; }
      else if (di == "KN") { result += '$'; i += 1; converted = true; }
      else if (di == "SK") { result += '|'; i += 1; converted = true; }
      else if (di == "SN") { result += '_'; i += 1; converted = true; }
    }
    
    if (!converted) {
      result += message[i];
    }
  }
  
  return result;
}

// Reverse lookup: morse pattern -> character
char decodeMorsePattern(const String &pattern) {
  for (int i = 0; i < MORSE_LOOKUP_LEN; i++) {
    const char* codePtr = (const char*)pgm_read_ptr(&MORSE_LOOKUP[i].code);
    char buffer[30];
    strcpy_P(buffer, codePtr);
    if (pattern == buffer) {
      return pgm_read_byte(&MORSE_LOOKUP[i].character);
    }
  }
  return '\0';
}

// Morse equivalence: maps characters that share the same Morse pattern
// to a single canonical form so scoring treats them as interchangeable.
char morseCanonical(char c) {
  switch (c) {
    // '=' and '[' (BT) both map to -...-
    case '[': return '=';
    // '+' and '^' (AR) both map to .-.-.
    case '^': return '+';
    // '&' and '<' (AS) both map to .-...
    case '<': return '&';
    // '(' and '$' (KN) both map to -.--.
    case '$': return '(';
    // '{' (CT) and '*' (KA) both map to -.-.-
    case '*': return '{';
    default:  return c;
  }
}

// Returns true if two characters produce the same Morse pattern
bool morseEquivalent(char a, char b) {
  if (a == b) return true;
  return morseCanonical(a) == morseCanonical(b);
}