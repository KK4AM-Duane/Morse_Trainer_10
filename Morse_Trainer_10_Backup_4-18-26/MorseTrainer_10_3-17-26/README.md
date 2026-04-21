Here's the improved `README.md` file, incorporating the new content while maintaining the existing structure and information: 3/9/2026
-Dan Martin KD0SMP
-Duane Erby KK4AM
-Esp-32 Morse Code Trainer and Iambic Keyer


# ESP32 CW Morse Code Trainer — Operation Manual

## Table of Contents
1. [Overview](#overview)
2. [Hardware](#hardware)
3. [Getting Started](#getting-started)
4. [Phone Interface](#phone-interface)
5. [Serial Monitor Interface](#serial-monitor-interface)
6. [Training Modes](#training-modes)
   - [Koch Method Training](#koch-method-training)
   - [Echo Training (Straight Key)](#echo-training-straight-key)
7. [Freeform Morse Playback](#freeform-morse-playback)
8. [Speed Control](#speed-control)
9. [Buzzer Control](#buzzer-control)
10. [Morse Lookup Modes](#morse-lookup-modes)
11. [Prosigns](#prosigns)
12. [Persistent Storage](#persistent-storage)
13. [Serial Command Reference](#serial-command-reference)
14. [Troubleshooting](#troubleshooting)

---

## Overview

The ESP32 CW Morse Code Trainer is a self-contained device that teaches Morse code using the **Koch method**. It transmits characters via a piezo buzzer, provides a phone-based web interface over Wi-Fi, and accepts answers either through the phone touchscreen or a physical **straight key** wired to the ESP32.

Key features:
- Koch method adaptive training with per-character statistics
- Straight-key echo training mode
- Phone web interface (WebSocket-based, no internet required)
- Synchronized audio on both buzzer and phone speaker simultaneously
- Real-time character echo mode for keyboard practice
- Freeform text-to-Morse playback
- All progress saved to non-volatile storage (survives power cycles)

---

## Hardware

| Component | GPIO Pin | Notes |
|---|---|---|
| Status LED | 2 | Blinks during playback |
| Buzzer (PWM) | 25 | 800 Hz, 8-bit LEDC |
| Green LED | 4 | Correct answer feedback |
| Red LED | 17 | Wrong answer feedback |
| Straight Key | 32 | Active-low; internal + external 10k, pull-up to 3.3V |
| RNG seed (ADC) | 36 | Floating input-only pin for entropy |

**Buzzer wiring:** The buzzer is driven by LEDC PWM at 800 Hz. Duty cycle is fixed at 128/255 (~50%). Do not connect the buzzer directly to a GPIO without a current-limiting transistor or driver circuit.

**Straight key wiring:** Wire the key between GPIO 32 and GND. The pin uses an internal pull-up; an additional external 10k resistor from GPIO 32 to 3.3V is recommended for noise immunity. Key closed = LOW = dit or dah.

---

## Getting Started

1. Power on the ESP32. The serial monitor (115200 baud) will display the boot banner and the full command help menu.
2. On your phone or tablet, connect to Wi-Fi:
   - **SSID:** `ESP32_Morse_Trainer`
   - **Password:** `morse1234`
3. Open a browser and navigate to `http://192.168.4.1`
4. The phone interface will connect automatically and display **Online**.
5. The trainer restores your last saved lesson number, speed, and per-character statistics from memory.

---

## Phone Interface

The phone interface is a full-screen web app served from the ESP32's internal flash (LittleFS). No internet connection or app installation is required.

### Controls

| Button | Function |
|---|---|
|   Start | Begin a Koch training session |
|   Pause /   Resume | Freeze or continue the current session |
|   Stop | End the current session |
|   Replay | Replay the last character sent |
| Reset | Reset all Koch progress to defaults (requires confirmation) |
|    Buzzer /    Muted | Toggle the ESP32 buzzer on or off |
|   / + (speed) | Decrease or increase WPM by 1 |

### Character Grid

During a Koch session, the character grid shows all characters active in the current lesson. Tap the button matching what you heard. Buttons flash **green** for correct, **red** for incorrect.

### Score Bar

Displays the current session's correct count, total count, accuracy percentage, and streak count (??).

### Display Area

Shows the current character (`?` while listening, the letter after answering), its Morse pattern (dots and dashes), and a result message.

### Audio

The phone plays the Morse tone through its own speaker **in sync with the ESP32 buzzer** using Web Audio API. The handshake sequence is:

1. ESP32 sends `word_char` / `koch_challenge` with the pattern and timing
2. Phone pre-builds the audio sequence and replies `word_ready` / `koch_ready`
3. ESP32 sends `word_go` / `koch_go` — both buzzer and phone speaker start simultaneously

> **Note:** Mobile browsers require a user gesture (tap) before audio can play. The Start button and Replay button both unlock audio. If you hear nothing, tap Replay once.

---

## Serial Monitor Interface

Connect at **115200 baud**. The command help menu prints automatically on every boot.

### Terminal Recommendations

The Arduino IDE Serial Monitor works for all line-based commands. For the best experience — especially during Koch training where single-character answers are sent immediately — use a terminal emulator that sends raw keystrokes:

| Tool | Platform | Advantage |
|---|---|---|
| **PuTTY** | Windows | ESC and Ctrl-C work as emergency stop |
| **Tera Term** | Windows | Full VT100, ESC works |
| **CoolTerm** | Mac/Windows | Clean interface, ESC works |
| Arduino Serial Monitor | All | Works for all commands; use `!` or `stop` to abort training |

### Emergency Stop

| Key / Command | Works in |
|---|---|
| `ESC` (0x1B) | PuTTY, Tera Term, CoolTerm |
| `Ctrl-C` (0x03) | PuTTY, Tera Term, CoolTerm |
| `!` (then Send) | Arduino Serial Monitor |
| `stop` (then Enter) | All serial tools |

All four methods stop both Koch and Echo training immediately.

---

## Training Modes

### Koch Method Training

The Koch method introduces characters one at a time. You only advance when you can copy the current character set with   90% accuracy. Speed adapts automatically.

#### Starting a session

- Serial: `koch start`
- Phone: tap **  Start**

The trainer switches to Koch mode automatically if needed.

#### How it works

1. The trainer plays a character via buzzer and phone speaker simultaneously.
2. After playback completes, answer by:
   - **Phone:** Tapping the matching button in the character grid
   - **Serial (terminal):** Typing the character (sent immediately, no Enter needed)
   - **Serial (Serial Monitor):** Typing the character then pressing Send/Enter
3. The result is shown on screen and serial. Green/red LEDs flash briefly.
4. After a short pause, the next character plays.

#### Adaptive behaviour

| Situation | Automatic action |
|---|---|
|   90% correct in last 10 chars | Lesson advances (new character added) |
|   90% correct, reaction time < 3s | Speed increases by 2 WPM |
| < 70% correct in last 10 chars | Speed decreases by 3 WPM |
| 3 consecutive timeouts (10s each) | "Lost" state: speed reduced, 2s recovery pause |

#### WPM limits during Koch

- Minimum: **18 WPM**
- Maximum: **50 WPM**

#### Pausing a session

- Serial: `koch pause` or `pause`
- Phone: tap **  Pause**

Resume with `koch resume` / `resume` or the **  Resume** button.

#### Stopping a session

- Serial: `koch stop`, `stop`, `!`, ESC, or Ctrl-C
- Phone: tap **  Stop**

Session statistics are saved to NVS automatically every 50 characters and on session stop.

#### Viewing statistics

- Serial: `koch stat`
- Phone: scroll to the Stats panel

Statistics include per-character accuracy, selection probability, total sent, total correct, and average reaction time.

#### Resetting progress

- Serial: `koch reset`
- Phone: tap **Reset** (confirmation required)

Clears all NVS data: lesson returns to 2, speed to 20 WPM, all character statistics cleared.

---

### Echo Training (Straight Key)

Echo training lets you practice sending Morse with a physical straight key. The trainer plays a character then waits for you to key it back.

#### Requirements

A straight key must be wired to **GPIO 32** (see [Hardware](#hardware)).

#### Starting echo training


echo start


Koch training must be stopped first. Any active playback must finish first.

#### How it works

1. The trainer plays a character from your current Koch lesson set via buzzer and phone.
2. The serial monitor displays `Key it back >`.
3. Key the character back using your straight key.
4. The key decoder classifies each press as a dit (press < 2× DOT duration) or dah (press   2× DOT duration).
5. After a gap of 3× DOT duration (character gap), the pattern is decoded and compared to the expected character.
6. Result is shown on serial and sent to the phone.
7. After 400 ms, the next character plays.

#### Timing reference for dit/dah at common speeds

| WPM | DOT (ms) | DAH (ms) | Char gap (ms) | Dit/dah threshold (ms) |
|---|---|---|---|---|
| 18 | 67 | 200 | 200 | 133 |
| 20 | 60 | 180 | 180 | 120 |
| 25 | 48 | 144 | 144 | 96 |

> **Tip:** Start at a lower WPM (`wpm 15`) when learning to send. The sidetone plays through the buzzer (and phone speaker if muted is off) while the key is held down.

#### Timeout

If no key input is received within **15 seconds**, the attempt is scored as incorrect and the next character plays automatically.

#### Stopping echo training


echo stop


Or use `stop`, `!`, ESC, or Ctrl-C.

#### Viewing echo statistics


echo stat


Displays total characters attempted and number correct for the current session. Echo stats are session-only (not saved to NVS).

---

## Freeform Morse Playback

Any text typed at the serial monitor (or sent from the phone) that is not a recognised command is transmitted as Morse code.


HELLO WORLD


- Spaces between words produce a 7× DOT word gap.
- Characters not found in the active table are skipped with a notice.
- Playback uses the same phone/buzzer synchronisation as Koch training.

You can also send prosigns by spelling them out (see [Prosigns](#prosigns)).

---

## Speed Control


wpm 20            set speed to 20 WPM
wpm               display current speed


Speed can also be changed from the phone using the **-** and **+** buttons.

- Valid range: **1 – 60 WPM** (general playback)
- Koch training enforces **18 – 50 WPM** internally
- Speed is saved to NVS immediately on change

**Timing formula (PARIS standard):**

| Parameter | Formula |
|---|---|
| DOT | 1200 ÷ WPM ms |
| DAH | 3 × DOT ms |
| Element gap | 1 × DOT ms |
| Character gap | 3 × DOT ms |
| Word gap | 7 × DOT ms |

---

## Buzzer Control

The ESP32 piezo buzzer and the phone speaker are independent. You can mute the buzzer and use phone audio only, or run both together.


buzzer on         enable buzzer
buzzer off        mute buzzer (phone audio still plays)


Phone button: **   Buzzer** / **   Muted**

The buzzer state is saved to NVS and restored on boot. The LED continues to flash during playback regardless of buzzer mute state. The sidetone during straight-key input also honours the buzzer mute setting.

---

## Morse Lookup Modes

The trainer supports four character lookup backends. Cycle through them with the `mode` command. The mode does not affect Koch training (which always uses Progtable).

| Mode | Command | Description |
|---|---|---|
| **Koch** | `mode` | Adaptive trainer; uses Progtable lookup internally |
| **Vector** | `mode` | Dynamic RAM table; add entries with `CHAR:CODE` |
| **Progmem** | `mode` | Read-only binary tree; full ITU character set |
| **Progtable** | `mode` | Read-only direct lookup table; fastest |

### Adding custom entries (Vector mode only)


A:.-              add or update character A with code .-
?:..--..        ? add question mark


Code must contain only `.` `-` and space. View the table with `list`. Clear it with `clear`.

---

## Prosigns

Prosigns are multi-letter sequences sent as a single unbroken Morse character. Type the two or three letter name at the serial prompt; the trainer converts them automatically.

| Type | Sent as | Meaning |
|---|---|---|
| `AA` | `~` | New line |
| `AR` | `^` | End of message |
| `AS` | `<` | Wait |
| `BK` | `>` | Break |
| `BT` | `[` | Separator / new paragraph |
| `CL` | `]` | Going off air |
| `CT` | `{` | Start signal (also KA) |
| `DO` | `}` | Shift to Wabun |
| `HH` | `#` | Error / correction |
| `KA` | `*` | Start signal |
| `KN` | `$` | Invite specific station |
| `SK` | `\|` | End of contact |
| `SN` | `_` | Understood |
| `SOS` | `\` | Distress |

You can also type the special character directly if your terminal supports it.

---

## Persistent Storage

The following values are saved to ESP32 non-volatile storage (NVS) and restored automatically on every boot:

| Item | Saved when |
|---|---|
| Current WPM | Every `wpm` command |
| Buzzer on/off state | Every `buzzer` command |
| Koch lesson number | Every 50 characters + on session stop |
| Koch per-character statistics | Every 50 characters + on session stop |

To erase all saved data: `koch reset` (or phone Reset button).

---

## Serial Command Reference

### General commands

| Command | Description |
|---|---|
| `help` | Display the command reference |
| `status` | Show current mode, speed, session stats, Wi-Fi info |
| `list` | Show the active Morse table |
| `clear` | Clear the Vector mode table |
| `test` | Send a test phrase via buzzer |
| `mode` | Cycle lookup mode: Koch , Vector , Progmem , Progtable |
| `wpm XX` | Set speed (1–60 WPM) |
| `wpm` | Display current speed |
| `buzzer on` | Enable buzzer |
| `buzzer off` | Mute buzzer |
| `echo on` | Enable real-time keyboard echo (types ? plays Morse) |
| `echo off` | Disable real-time keyboard echo |
| `CHAR:CODE` | Add/update a character in Vector mode (e.g. `A:.-`) |
| `WORD` | Transmit any word or phrase in Morse |

### Koch commands

| Command | Description |
|---|---|
| `koch start` | Start a Koch training session |
| `koch stop` | Stop the current session |
| `koch pause` | Pause the session |
| `koch resume` | Resume a paused session |
| `koch stat` | Show per-character statistics |
| `koch reset` | Reset all progress to defaults |

### Echo training commands

| Command | Description |
|---|---|
| `echo start` | Start straight-key echo training |
| `echo stop` | Stop echo training |
| `echo stat` | Show session statistics |

### Emergency stop

| Input | Works in |
|---|---|
| `stop` + Enter | All serial tools |
| `!` + Send | Arduino Serial Monitor |
| `ESC` | PuTTY, Tera Term, CoolTerm |
| `Ctrl-C` | PuTTY, Tera Term, CoolTerm |

---

## Troubleshooting

### Phone shows , Offline

- Confirm your phone is connected to Wi-Fi SSID `ESP32_Morse_Trainer`.
- Reload the browser page (`http://192.168.4.1`).
- Check the serial monitor for JavaScript errors (a parse error in `app.js` will prevent the WebSocket from connecting).

### No audio on phone

- Tap any button (Start or Replay) to unlock the Web Audio API — browsers require a user gesture before playing audio.
- Confirm the phone is not in silent/Do Not Disturb mode.

### Buzzer silent but phone plays

- Type `buzzer on` at the serial monitor or tap the **Muted** button on the phone.

### Straight key not responding

- Confirm the key is wired between GPIO 32 and GND.
- At rest (key open) GPIO 32 should read HIGH. Check with `status` , Playback shows key activity in serial output during key presses.
- Verify the serial monitor shows `KEY TIME (Xms) -> .` or `KEY TIME (Xms) -> -` when you press the key.

### Characters decoded as `?` on the straight key

- The pattern is not in the lookup table. Ensure the lookup mode includes the character (Progtable or Progmem cover the full ITU set).
- If your timing is off, try a lower WPM: `wpm 15`. The dit/dah threshold is 2× DOT duration.

### Koch training stuck / not advancing

- Check `koch stat` — accuracy must reach , 90% over 10 consecutive characters to advance the lesson.
- If you are consistently below 70% the trainer will reduce speed automatically.

### All progress lost after power cycle

- NVS saves every 50 characters. Progress from a very short session may not have been checkpointed. Use `koch stop` to force a save before powering down.

### Serial output shows `?` / diamond characters mid-transmission

- This indicates concurrent Serial writes from the async WebSocket task and the main loop. Ensure you are running the latest firmware which suppresses debug logging for `word_ready` and `koch_ready` handshake messages.
