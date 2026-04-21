#ifndef CONFIG_H
#define CONFIG_H

// config.h – Core data structures

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
    uint16_t sessionCorrect;  // correct answers this session
    uint16_t sessionTotal;    // total characters this session
    uint8_t recentCorrect;    // correct in last N characters (for speed adaptation)
    uint8_t recentTotal;      // total in last N characters
};

#endif
