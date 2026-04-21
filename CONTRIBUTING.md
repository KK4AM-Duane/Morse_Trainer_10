## Working Baseline

As of April 18, 2026, the project compiles and runs correctly with:
- Vector mode removed from MorseLookup (Koch/Progmem/Progtable only)
- app.js fully restored with all sections intact (1798 lines)
- `updateEchoUi()` uses correct `elBtnKeyIambic` (not `elBtnKeyerIambic`)
- `activeKochCharacters` used consistently (not `activeKochChars`)
- Keyer overlay has Phone mute button (`cmdKeyerPhoneMuteToggle`) and volume slider (`keyer-vol-slider`)
- `updatePhoneMuteButton()` syncs both trainer and keyer phone mute buttons
- `testTone()` sends via ESP32 round-trip only (no local `playMorseTone` double-play)

## Standards

- ESP32 C++ code targets C++14
- JavaScript in `Data/app.js` uses `'use strict'` and `var` declarations (no ES6 let/const)
- All web assets live in `Data/` folder and are uploaded to LittleFS