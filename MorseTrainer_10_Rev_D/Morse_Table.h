#ifndef MORSE_TABLE_H
#define MORSE_TABLE_H

#include <Arduino.h>

// The complete ITU Morse alphabet as a binary tree
// Index 63 is the root. Left child = dit, right child = dah.
const char MORSE_TABLE[] PROGMEM =
"*5*H*4*S***V*3*I***F***U?*_**2*E*&*L\"**R*+."
"****A***P@**W***J'1* *6-B*=*D*/"
"*X***N***C;(!K***Y***T*7*Z**,G***Q***M:8*!***O*9***0*";

const int MORSE_TABLE_LEN = 127;
const int MORSE_TREE_LEVELS = 6;  // Back to 6 for offset calculation
const int MORSE_TREETOP = 63;

// String literals stored in PROGMEM for direct lookup table
const char MORSE_A[] PROGMEM = ".-";
const char MORSE_B[] PROGMEM = "-...";
const char MORSE_C[] PROGMEM = "-.-.";
const char MORSE_D[] PROGMEM = "-..";
const char MORSE_E[] PROGMEM = ".";
const char MORSE_F[] PROGMEM = "..-.";
const char MORSE_G[] PROGMEM = "--.";
const char MORSE_H[] PROGMEM = "....";
const char MORSE_I[] PROGMEM = "..";
const char MORSE_J[] PROGMEM = ".---";
const char MORSE_K[] PROGMEM = "-.-";
const char MORSE_L[] PROGMEM = ".-..";
const char MORSE_M[] PROGMEM = "--";
const char MORSE_N[] PROGMEM = "-.";
const char MORSE_O[] PROGMEM = "---";
const char MORSE_P[] PROGMEM = ".--.";
const char MORSE_Q[] PROGMEM = "--.-";
const char MORSE_R[] PROGMEM = ".-.";
const char MORSE_S[] PROGMEM = "...";
const char MORSE_T[] PROGMEM = "-";
const char MORSE_U[] PROGMEM = "..-";
const char MORSE_V[] PROGMEM = "...-";
const char MORSE_W[] PROGMEM = ".--";
const char MORSE_X[] PROGMEM = "-..-";
const char MORSE_Y[] PROGMEM = "-.--";
const char MORSE_Z[] PROGMEM = "--..";
const char MORSE_0[] PROGMEM = "-----";
const char MORSE_1[] PROGMEM = ".----";
const char MORSE_2[] PROGMEM = "..---";
const char MORSE_3[] PROGMEM = "...--";
const char MORSE_4[] PROGMEM = "....-";
const char MORSE_5[] PROGMEM = ".....";
const char MORSE_6[] PROGMEM = "-....";
const char MORSE_7[] PROGMEM = "--...";
const char MORSE_8[] PROGMEM = "---..";
const char MORSE_9[] PROGMEM = "----.";
const char MORSE_AMPERSAND[] PROGMEM = ".-...";
const char MORSE_APOSTROPHE[] PROGMEM = ".----.";
const char MORSE_AT[] PROGMEM = ".--.-.";
const char MORSE_RPAREN[] PROGMEM = "-.--.-";
const char MORSE_LPAREN[] PROGMEM = "-.--.";
const char MORSE_COLON[] PROGMEM = "---...";
const char MORSE_COMMA[] PROGMEM = "--..--";
const char MORSE_EQUALS[] PROGMEM = "-...-";
const char MORSE_EXCLAIM[] PROGMEM = "-.-.--";
const char MORSE_PERIOD[] PROGMEM = ".-.-.-";
const char MORSE_MINUS[] PROGMEM = "-....-";
// Percent is sent as 0/0 with character gaps (space = char gap)
const char MORSE_PERCENT[] PROGMEM = "----- -..-. -----";
const char MORSE_PLUS[] PROGMEM = ".-.-.";
const char MORSE_QUOTE[] PROGMEM = ".-..-.";
const char MORSE_QUESTION[] PROGMEM = "..--..";
const char MORSE_SLASH[] PROGMEM = "-..-.";
// Prosigns - sent as single character with NO internal character gaps
const char MORSE_AA[] PROGMEM = ".-.-";        // AA (new line)
const char MORSE_AR[] PROGMEM = ".-.-.";       // AR (end of message)
const char MORSE_AS[] PROGMEM = ".-...";       // AS (wait)
const char MORSE_BK[] PROGMEM = "-...-.-";     // BK (break)
const char MORSE_BT[] PROGMEM = "-...-";       // BT (separator)
const char MORSE_CL[] PROGMEM = "-.-..-..";    // CL (closing)
const char MORSE_CT[] PROGMEM = "-.-.-";       // CT (starting signal)
const char MORSE_DO[] PROGMEM = "-..---";      // DO (shift to wabun code)
const char MORSE_HH[] PROGMEM = "........";    // HH (error)
const char MORSE_KA[] PROGMEM = "-.-.-";       // KA (starting signal)
const char MORSE_KN[] PROGMEM = "-.--.";       // KN (invitation to transmit)
const char MORSE_SK[] PROGMEM = "...-.-";      // SK (end of contact)
const char MORSE_SN[] PROGMEM = "...-.";       // SN (understood)
const char MORSE_SOS[] PROGMEM = "...---...";  // SOS (distress)

// Structure for direct lookup table
struct MorseChar {
  char character;
  const char* code;
};

// Direct lookup table in PROGMEM
const MorseChar MORSE_LOOKUP[] PROGMEM = {
    // Letters
    {'A', MORSE_A},    {'B', MORSE_B},    {'C', MORSE_C},    {'D', MORSE_D},
    {'E', MORSE_E},    {'F', MORSE_F},    {'G', MORSE_G},    {'H', MORSE_H},
    {'I', MORSE_I},    {'J', MORSE_J},    {'K', MORSE_K},    {'L', MORSE_L},
    {'M', MORSE_M},    {'N', MORSE_N},    {'O', MORSE_O},    {'P', MORSE_P},
    {'Q', MORSE_Q},    {'R', MORSE_R},    {'S', MORSE_S},    {'T', MORSE_T},
    {'U', MORSE_U},    {'V', MORSE_V},    {'W', MORSE_W},    {'X', MORSE_X},
    {'Y', MORSE_Y},    {'Z', MORSE_Z},
    
    // Numbers
    {'0', MORSE_0},    {'1', MORSE_1},    {'2', MORSE_2},    {'3', MORSE_3},
    {'4', MORSE_4},    {'5', MORSE_5},    {'6', MORSE_6},    {'7', MORSE_7},
    {'8', MORSE_8},    {'9', MORSE_9},
    
    // Punctuation
    {'&', MORSE_AMPERSAND},   {'\'', MORSE_APOSTROPHE},  {'@', MORSE_AT},
    {')', MORSE_RPAREN},      {'(', MORSE_LPAREN},       {':', MORSE_COLON},
    {',', MORSE_COMMA},       {'=', MORSE_EQUALS},       {'!', MORSE_EXCLAIM},
    {'.', MORSE_PERIOD},      {'-', MORSE_MINUS},        {'%', MORSE_PERCENT},
    {'+', MORSE_PLUS},        {'"', MORSE_QUOTE},        {'?', MORSE_QUESTION},
    {'/', MORSE_SLASH},
    
    // Prosigns (sent as single character with no internal character gaps)
    {'~', MORSE_AA},      // AA (new line)
    {'^', MORSE_AR},      // AR (end of message)
    {'<', MORSE_AS},      // AS (wait)
    {'>', MORSE_BK},      // BK (break)
    {'[', MORSE_BT},      // BT (separator)
    {']', MORSE_CL},      // CL (closing)
    {'{', MORSE_CT},      // CT (starting signal)
    {'}', MORSE_DO},      // DO (shift to wabun code)
    {'#', MORSE_HH},      // HH (error)
    {'*', MORSE_KA},      // KA (starting signal)
    {'$', MORSE_KN},      // KN (invitation to transmit)
    {'|', MORSE_SK},      // SK (end of contact)
    {'_', MORSE_SN},      // SN (understood)
    {'\\', MORSE_SOS}     // SOS (distress)
};

const int MORSE_LOOKUP_LEN = sizeof(MORSE_LOOKUP) / sizeof(MORSE_LOOKUP[0]);

#endif