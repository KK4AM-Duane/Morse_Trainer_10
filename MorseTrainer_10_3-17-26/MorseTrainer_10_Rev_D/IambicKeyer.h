#pragma once
#include <Arduino.h>

// Hardware — dit paddle shares GPIO with straight key; dah is separate
extern const unsigned int KEY_PIN;          // GPIO32 = dit paddle / straight key
extern const unsigned int PADDLE_DAH_PIN;   // GPIO33 = dah paddle

// Iambic Mode B state machine
// Call initIambicKeyer() once from setup.
// Call pollIambicKeyer() every loop() iteration.
// When a complete character is decoded it calls echoSubmitDecodedChar().

void initIambicKeyer();

// Non-blocking poller — reads paddles, generates timed dits/dahs,
// accumulates pattern, decodes on character gap.
void pollIambicKeyer();

// Returns true if the keyer is currently mid-element (sending a dit or dah)
bool isIambicBusy();