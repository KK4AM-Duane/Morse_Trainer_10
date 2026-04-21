#ifndef MORSE_TABLE_H
#define MORSE_TABLE_H

#include <Arduino.h>

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
const char MORSE_COMMA[] PROGMEM = "--..--";
const char MORSE_EQUAL[] PROGMEM = "-...-";
const char MORSE_PERIOD[] PROGMEM = ".-.-.-";
const char MORSE_QUESTION[] PROGMEM = "..--..";
const char MORSE_SLASH[] PROGMEM = "-..-.";
const char MORSE_HYPHEN[] PROGMEM = "-....-";
const char MORSE_AA[] PROGMEM = ".-.-";
const char MORSE_AR[] PROGMEM = ".-.-.";
const char MORSE_AS[] PROGMEM = ".-...";
const char MORSE_BK[] PROGMEM = "-...-.-";
const char MORSE_BT[] PROGMEM = "-...-";
const char MORSE_CL[] PROGMEM = "-.-..-..";
const char MORSE_CT[] PROGMEM = "-.-.-";
const char MORSE_DO[] PROGMEM = "-..---";
const char MORSE_HH[] PROGMEM = "........";
const char MORSE_KA[] PROGMEM = "-.-.-";
const char MORSE_KN[] PROGMEM = "-.--.";
const char MORSE_SK[] PROGMEM = "...-.-";
const char MORSE_SN[] PROGMEM = "...-.";
const char MORSE_SOS[] PROGMEM = "...---...";

struct MorseLookupEntry {
  char character;
  const char* code;
};

const MorseLookupEntry MORSE_LOOKUP[] PROGMEM = {
  {'A', MORSE_A}, {'B', MORSE_B}, {'C', MORSE_C}, {'D', MORSE_D},
  {'E', MORSE_E}, {'F', MORSE_F}, {'G', MORSE_G}, {'H', MORSE_H},
  {'I', MORSE_I}, {'J', MORSE_J}, {'K', MORSE_K}, {'L', MORSE_L},
  {'M', MORSE_M}, {'N', MORSE_N}, {'O', MORSE_O}, {'P', MORSE_P},
  {'Q', MORSE_Q}, {'R', MORSE_R}, {'S', MORSE_S}, {'T', MORSE_T},
  {'U', MORSE_U}, {'V', MORSE_V}, {'W', MORSE_W}, {'X', MORSE_X},
  {'Y', MORSE_Y}, {'Z', MORSE_Z},
  {'0', MORSE_0}, {'1', MORSE_1}, {'2', MORSE_2}, {'3', MORSE_3},
  {'4', MORSE_4}, {'5', MORSE_5}, {'6', MORSE_6}, {'7', MORSE_7},
  {'8', MORSE_8}, {'9', MORSE_9},
  {',', MORSE_COMMA}, {'=', MORSE_EQUAL}, {'.', MORSE_PERIOD},
  {'?', MORSE_QUESTION}, {'/', MORSE_SLASH}, {'-', MORSE_HYPHEN},
  {'~', MORSE_AA}, {'^', MORSE_AR}, {'<', MORSE_AS}, {'>', MORSE_BK},
  {'[', MORSE_BT}, {']', MORSE_CL}, {'{', MORSE_CT}, {'}', MORSE_DO},
  {'#', MORSE_HH}, {'*', MORSE_KA}, {'$', MORSE_KN}, {'|', MORSE_SK},
  {'_', MORSE_SN}, {'\\', MORSE_SOS}
};

const int MORSE_LOOKUP_LEN = sizeof(MORSE_LOOKUP) / sizeof(MORSE_LOOKUP[0]);

#endif