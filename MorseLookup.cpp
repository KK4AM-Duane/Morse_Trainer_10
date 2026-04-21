#include "MorseLookup.h"
#include "Morse_Table.h"

// Global variables
MorseMode currentMode = MorseMode::Koch;

// Helper function to search subtree for character in PROGMEM table
bool searchSubtree(int index, char target, int level) {
  if (index < 0 || index >= MORSE_TABLE_LEN || level > 6) {
    return false;
  }
  
  char nodeChar = pgm_read_byte(&MORSE_TABLE[index]);
  if (nodeChar == target) {
    return true;
  }
  
  if (level >= 6) {
    return false;
  }
  
  int leftChild = index - (1 << (MORSE_TREE_LEVELS - level - 1));
  if (leftChild >= 0 && searchSubtree(leftChild, target, level + 1)) {
    return true;
  }
  
  int rightChild = index + (1 << (MORSE_TREE_LEVELS - level - 1));
  if (rightChild < MORSE_TABLE_LEN && searchSubtree(rightChild, target, level + 1)) {
    return true;
  }
  
  return false;
}

// Convert character to morse code string using MORSE_TABLE binary tree in PROGMEM
String charToMorse(char c) {
  c = toupper(c);
  String morse = "";
  int index = MORSE_TREETOP;
  
  for (int level = 0; level <= 6; level++) {
    char nodeChar = pgm_read_byte(&MORSE_TABLE[index]);
    
    if (nodeChar == c) {
      return morse;
    }
    
    if (level >= 6) {
      break;
    }
    
    int leftChild = index - (1 << (MORSE_TREE_LEVELS - level - 1));
    if (leftChild >= 0 && leftChild < MORSE_TABLE_LEN) {
      if (searchSubtree(leftChild, c, level + 1)) {
        morse += '.';
        index = leftChild;
        continue;
      }
    }
    
    int rightChild = index + (1 << (MORSE_TREE_LEVELS - level - 1));
    if (rightChild >= 0 && rightChild < MORSE_TABLE_LEN) {
      if (searchSubtree(rightChild, c, level + 1)) {
        morse += '-';
        index = rightChild;
        continue;
      }
    }
    
    break;
  }
  
  return morse;
}

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
String getMorseCode(char c) {
  if (currentMode == MorseMode::Koch || currentMode == MorseMode::Progtable) {
    return lookupMorseProgTable(c);
  } else {
    return charToMorse(c);
  }
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
    // '{' (CT) and '*' (KA) both map to -.-.
    case '*': return '{';
    default:  return c;
  }
}

// Returns true if two characters produce the same Morse pattern
bool morseEquivalent(char a, char b) {
  if (a == b) return true;
  return morseCanonical(a) == morseCanonical(b);
}