#include "MorseLookup.h"
#include "Morse_Table.h"

// Global variables
MorseMode currentMode = MorseMode::Koch;
std::vector<MorseMap> morseTable;

// Initialize vector table with standard Morse code
void initializeVectorTable() {
  morseTable.clear();
  
  // Add letters
  morseTable.push_back(MorseMap('A', ".-"));
  morseTable.push_back(MorseMap('B', "-..."));
  morseTable.push_back(MorseMap('C', "-.-."));
  morseTable.push_back(MorseMap('D', "-.."));
  morseTable.push_back(MorseMap('E', "."));
  morseTable.push_back(MorseMap('F', "..-."));
  morseTable.push_back(MorseMap('G', "--."));
  morseTable.push_back(MorseMap('H', "...."));
  morseTable.push_back(MorseMap('I', ".."));
  morseTable.push_back(MorseMap('J', ".---"));
  morseTable.push_back(MorseMap('K', "-.-"));
  morseTable.push_back(MorseMap('L', ".-.."));
  morseTable.push_back(MorseMap('M', "--"));
  morseTable.push_back(MorseMap('N', "-."));
  morseTable.push_back(MorseMap('O', "---"));
  morseTable.push_back(MorseMap('P', ".--."));
  morseTable.push_back(MorseMap('Q', "--.-"));
  morseTable.push_back(MorseMap('R', ".-."));
  morseTable.push_back(MorseMap('S', "..."));
  morseTable.push_back(MorseMap('T', "-"));
  morseTable.push_back(MorseMap('U', "..-"));
  morseTable.push_back(MorseMap('V', "...-"));
  morseTable.push_back(MorseMap('W', ".--"));
  morseTable.push_back(MorseMap('X', "-..-"));
  morseTable.push_back(MorseMap('Y', "-.--"));
  morseTable.push_back(MorseMap('Z', "--.."));
  
  // Add numbers
  morseTable.push_back(MorseMap('0', "-----"));
  morseTable.push_back(MorseMap('1', ".----"));
  morseTable.push_back(MorseMap('2', "..---"));
  morseTable.push_back(MorseMap('3', "...--"));
  morseTable.push_back(MorseMap('4', "....-"));
  morseTable.push_back(MorseMap('5', "....."));
  morseTable.push_back(MorseMap('6', "-...."));
  morseTable.push_back(MorseMap('7', "--..."));
  morseTable.push_back(MorseMap('8', "---.."));
  morseTable.push_back(MorseMap('9', "----."));
  
  // Add common punctuation
  morseTable.push_back(MorseMap('.', ".-.-.-"));
  morseTable.push_back(MorseMap(',', "--..--"));
  morseTable.push_back(MorseMap('?', "..--.."));
  morseTable.push_back(MorseMap('/', "-..-."));
  morseTable.push_back(MorseMap('-', "-....-"));
  morseTable.push_back(MorseMap('=', "-...-"));
  morseTable.push_back(MorseMap(':', "---..."));
  morseTable.push_back(MorseMap('!', "-.-.--"));
  morseTable.push_back(MorseMap('(', "-.--."));
  morseTable.push_back(MorseMap(')', "-.--.-"));
  morseTable.push_back(MorseMap('&', ".-..."));
  morseTable.push_back(MorseMap('\'', ".----."));
  morseTable.push_back(MorseMap('"', ".-..-."));
  morseTable.push_back(MorseMap('@', ".--.-."));
  morseTable.push_back(MorseMap('+', ".-.-."));
  
  Serial.println("Vector table initialized with standard Morse code");
}

// Helper function to search subtree for character in PROGMEM table
bool searchSubtree(int index, char target, int level) {
  if (index < 0 || index >= MORSE_TABLE_LEN || level > 6) {
    return false;
  }
  
  char nodeChar = pgm_read_byte(&MORSE_TABLE[index]);
  if (nodeChar == target) {
    return true;
  }
  
  // Calculate offset (only valid for levels 0-5)
  if (level >= 6) {
    return false;
  }
  
  // Check left subtree (dit)
  int leftChild = index - (1 << (MORSE_TREE_LEVELS - level - 1));
  if (leftChild >= 0 && searchSubtree(leftChild, target, level + 1)) {
    return true;
  }
  
  // Check right subtree (dah)
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
  
  // Search through the binary tree to find the character (allow 7 levels: 0-6)
  for (int level = 0; level <= 6; level++) {
    char nodeChar = pgm_read_byte(&MORSE_TABLE[index]);
    
    if (nodeChar == c) {
      return morse;
    }
    
    // Can't calculate children at level 6
    if (level >= 6) {
      break;
    }
    
    // Try left child (dit)
    int leftChild = index - (1 << (MORSE_TREE_LEVELS - level - 1));
    if (leftChild >= 0 && leftChild < MORSE_TABLE_LEN) {
      if (searchSubtree(leftChild, c, level + 1)) {
        morse += '.';
        index = leftChild;
        continue;
      }
    }
    
    // Try right child (dah)
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
    // Read character directly from PROGMEM
    char tableChar = pgm_read_byte(&MORSE_LOOKUP[i].character);
    
    if (tableChar == c) {
      // Read the code pointer from PROGMEM
      const char* codePtr = (const char*)pgm_read_ptr(&MORSE_LOOKUP[i].code);
      
      // Read the morse code string from PROGMEM
      char buffer[30];
      strcpy_P(buffer, codePtr);
      return String(buffer);
    }
  }
  return "";
}

// Look up Morse code for a character in the vector
String lookupMorse(char c) {
  c = toupper(c);
  for (const auto &entry : morseTable) {
    if (entry.getCharacter() == c) {
      return entry.getCode();
    }
  }
  return "";
}

// Get morse code based on current mode
// Koch mode uses Progtable internally (it has all chars + prosigns)
String getMorseCode(char c) {
  if (currentMode == MorseMode::Koch) {
    return lookupMorseProgTable(c);
  } else if (currentMode == MorseMode::Progmem) {
    return charToMorse(c);
  } else if (currentMode == MorseMode::Progtable) {
    return lookupMorseProgTable(c);
  } else {
    return lookupMorse(c);
  }
}

// Preprocess message to convert prosign sequences to single characters
String preprocessProsigns(String message) {
  String result = "";
  message.toUpperCase();
  
  for (int i = 0; i < message.length(); i++) {
    bool converted = false;
    
    // Check if we're at a word boundary
    bool atBoundary = (i == 0 || message[i - 1] == ' ');
    
    // Check for 3-character prosigns first
    if (i < message.length() - 2 && atBoundary) {
      String threeChar = message.substring(i, i + 3);
      bool endBoundary = (i + 3 >= message.length() || message[i + 3] == ' ');
      
      if (threeChar == "SOS" && endBoundary) {
        result += '\\';
        i += 2;
        converted = true;
      }
    }
    
    // Check for 2-character prosigns
    if (!converted && i < message.length() - 1 && atBoundary) {
      String twoChar = message.substring(i, i + 2);
      bool endBoundary = (i + 2 >= message.length() || message[i + 2] == ' ');
      
      if (endBoundary) {
        if (twoChar == "AA") { result += '~'; i++; converted = true; }
        else if (twoChar == "AR") { result += '^'; i++; converted = true; }
        else if (twoChar == "AS") { result += '<'; i++; converted = true; }
        else if (twoChar == "BK") { result += '>'; i++; converted = true; }
        else if (twoChar == "BT") { result += '['; i++; converted = true; }
        else if (twoChar == "CL") { result += ']'; i++; converted = true; }
        else if (twoChar == "CT") { result += '{'; i++; converted = true; }
        else if (twoChar == "DO") { result += '}'; i++; converted = true; }
        else if (twoChar == "HH") { result += '#'; i++; converted = true; }
        else if (twoChar == "KA") { result += '*'; i++; converted = true; }
        else if (twoChar == "KN") { result += '$'; i++; converted = true; }
        else if (twoChar == "SK") { result += '|'; i++; converted = true; }
        else if (twoChar == "SN") { result += '_'; i++; converted = true; }
      }
    }
    
    // Regular character if not converted
    if (!converted) {
      result += message[i];
    }
  }
  
  return result;
}

// Add morse entry to table
void addMorseEntry(char character, const String &code) {
  character = toupper(character);
  
  // Check if character already exists and update it
  for (auto &entry : morseTable) {
    if (entry.getCharacter() == character) {
      Serial.print("Updated: ");
      Serial.print(character);
      Serial.print(" = ");
      Serial.println(code);
      entry = MorseMap(character, code);
      return;
    }
  }
  
  // Add new entry
  morseTable.push_back(MorseMap(character, code));
  Serial.print("Added: ");
  Serial.print(character);
  Serial.print(" = ");
  Serial.println(code);
}

// Reverse lookup using PROGMEM direct lookup table
char decodeMorsePattern(const String &pattern) {
  char buffer[30];
  for (int i = 0; i < MORSE_LOOKUP_LEN; i++) {
    char tableChar = pgm_read_byte(&MORSE_LOOKUP[i].character);
    const char* codePtr = (const char*)pgm_read_ptr(&MORSE_LOOKUP[i].code);
    strcpy_P(buffer, codePtr);
    if (pattern.equals(buffer)) {
      return tableChar;
    }
  }
  return '\0';
}

// Morse equivalence: map characters that share the same pattern
// to one canonical form.  Only pairs/groups from the ITU table
// that are genuinely identical on-air are listed here.
char morseCanonical(char c) {
  switch (c) {
    // -...-  BT prosign and = sign
    case '[':  return '=';
    // .-.-.  AR prosign and + sign
    case '^':  return '+';
    // .-...  AS prosign and & sign
    case '<':  return '&';
    // -.--.  KN prosign and ( paren
    case '$':  return '(';
    // -.-.-  CT and KA (both are "start" signals)
    case '{':  return '*';
    default:   return c;
  }
}

// Returns true if two characters are Morse-equivalent
bool morseEquivalent(char a, char b) {
  return morseCanonical(toupper(a)) == morseCanonical(toupper(b));
}