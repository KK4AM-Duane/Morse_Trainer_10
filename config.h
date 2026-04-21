#ifndef CONFIG_H
#define CONFIG_H

// config.h — Core data structures

// Koch method character order (most commonly taught sequence)
// Based on the Koch/Ludwig method used by LCWO.net and CW Academy
const char KOCH_ORDER[] = "KMURESNAPTLWI.JZ=FOY,VG5/Q92H38B?47C1D60X";

// Per-character training data
struct CharStats {
    char character;
    uint8_t probability;      // 0-100, higher is more likely to be selected
    uint16_t totalSent;       // times this character has been sent
    uint16_t totalCorrect;    // times correctly identified
    uint32_t avgReactionMs;   // rolling average reaction time in ms
};

// Training session state
struct TrainerState {
    uint8_t kochLesson;       // current Koch lesson (2 = first 2 chars)
    float charSpeed;          // character speed in WPM
    float farnsworthSpeed;    // effective speed in WPM (spacing between chars)
    uint16_t sessionCorrect;  // correct answers this session
    uint16_t sessionTotal;    // total characters this session
    uint8_t recentCorrect;    // correct in last N characters (for speed adaptation)
    uint8_t recentTotal;      // total in last N characters
};

// Morse timing parameters (derived from speed)
struct MorseTiming {
    int ditMs;                // dit duration in milliseconds
    int dahMs;                // dah duration (3 x dit)
    int elementGapMs;         // gap between dits/dahs within a character (1 x dit)
    int charGapMs;            // gap between characters (Farnsworth-adjusted)
    int wordGapMs;            // gap between words (Farnsworth-adjusted)
};

// Morse character queue entry (sent to receiver for scoring)
struct MorseQueueEntry {
    char character;
    unsigned long sentTimeMs; // millis() when character finished sending
};

#endif
