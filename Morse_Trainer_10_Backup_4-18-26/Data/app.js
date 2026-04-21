// ================================================================
//  CW Trainer — Phone Interface
//  Connects to ESP32 WebSocket at /ws
// ================================================================

'use strict';

// ── DOM refs ────────────────────────────────────────────────
var elConnStatus   = null;
var elHeaderLesson = null;
var elHeaderWpm    = null;
var elHeaderWifi   = null;
var elDisplayChar  = null;
var elDisplayMorse = null;
var elDisplayMsg   = null;
var elScoreCorrect = null;
var elScoreTotal   = null;
var elScorePct     = null;
var elScoreStreak  = null;
var elScoreReact   = null;
var elCharGrid     = null;
var elBtnStart     = null;
var elBtnPause     = null;
var elBtnStop      = null;
var elBtnReplay    = null;
var elBtnReset     = null;
var elBtnBuzzer    = null;
var elSpeedDisplay = null;
var elStatsBody    = null;
var elLogBody      = null;
var elEchoStatus   = null;
var elEchoPrompt   = null;
var elEchoResult   = null;
var elBtnEchoKoch  = null;
var elBtnEchoFull  = null;
var elBtnEchoStop  = null;
// Echo key type buttons (inside Echo Training panel)
var elBtnKeyStraight = null;
var elBtnKeyIambic   = null;
// Practice player
var elPracticeText   = null;
var elBtnPracSend    = null;
var elBtnPracFile    = null;
var elBtnPracStop    = null;
var elPracticeStatus = null;
var elPracticeProgress = null;
// File editor
var elFileEditorText     = null;
var elFileEditorStatus   = null;
// Keyer overlay
var elKeyerOverlay      = null;
var elKeyerStatus       = null;
var elKeyerBuzzer       = null;
var elKeyerStop         = null;
var elKeyerSpeedDisp    = null;
var elKeyerProgramArea  = null;
var elKeyerProgramLabel = null;
var elKeyerProgramText  = null;
var elKeyerMemBtns      = [];
var elBtnKeyerProgram   = null;
// Keyer key type buttons (inside Keyer overlay)
var elBtnKeyerStraight  = null;
var elBtnKeyerIambic    = null;

function cacheDom() {
  elConnStatus = document.getElementById('conn-status');
  elHeaderLesson = document.getElementById('header-lesson');
  elHeaderWpm = document.getElementById('header-wpm');
  elHeaderWifi = document.getElementById('header-wifi');
  elDisplayChar  = document.getElementById('display-char');
  elDisplayMorse = document.getElementById('display-morse');
  elDisplayMsg   = document.getElementById('display-msg');
  elScoreCorrect = document.getElementById('score-correct');
  elScoreTotal   = document.getElementById('score-total');
  elScorePct     = document.getElementById('score-pct');
  elScoreStreak  = document.getElementById('score-streak');
  elScoreReact   = document.getElementById('score-reaction');
  elCharGrid     = document.getElementById('char-grid');
  elBtnStart     = document.getElementById('btn-start');
  elBtnPause     = document.getElementById('btn-pause');
  elBtnStop      = document.getElementById('btn-stop');
  elBtnReplay    = document.getElementById('btn-replay');
  elBtnReset     = document.getElementById('btn-reset');
  elBtnBuzzer    = document.getElementById('btn-buzzer');
  elSpeedDisplay = document.getElementById('speed-display');
  elStatsBody    = document.getElementById('stats-body');
  elLogBody      = document.getElementById('log-body');
  elEchoStatus   = document.getElementById('echo-status');
  elEchoPrompt   = document.getElementById('echo-prompt');
  elEchoResult   = document.getElementById('echo-result');
  elBtnEchoKoch  = document.getElementById('btn-echo-koch');
  elBtnEchoFull  = document.getElementById('btn-echo-full');
  elBtnEchoStop  = document.getElementById('btn-echo-stop');
  // Echo key type buttons
  elBtnKeyStraight = document.getElementById('btn-key-straight');
  elBtnKeyIambic   = document.getElementById('btn-key-iambic');
  // Practice player
  elPracticeText     = document.getElementById('practice-text');
  elBtnPracSend      = document.getElementById('btn-practice-send');
  elBtnPracFile      = document.getElementById('btn-practice-file');
  elBtnPracStop      = document.getElementById('btn-practice-stop');
  elPracticeStatus   = document.getElementById('practice-status');
  elPracticeProgress = document.getElementById('practice-progress');
  // File editor
  elFileEditorText     = document.getElementById('file-editor-text');
  elFileEditorStatus   = document.getElementById('file-editor-status');
  // Keyer overlay
  elKeyerOverlay      = document.getElementById('keyer-overlay');
  elKeyerStatus       = document.getElementById('keyer-status');
  elKeyerBuzzer       = document.getElementById('btn-keyer-buzzer');
  elKeyerStop         = document.getElementById('btn-keyer-stop');
  elKeyerSpeedDisp    = document.getElementById('keyer-speed-display');
  elKeyerProgramArea  = document.getElementById('keyer-program-area');
  elKeyerProgramLabel = document.getElementById('keyer-program-label');
  elKeyerProgramText  = document.getElementById('keyer-program-text');
  elKeyerMemBtns = [];
  for (var mi = 0; mi < 6; mi++) {
    elKeyerMemBtns.push(document.getElementById('keyer-m' + mi));
  }
  elBtnKeyerProgram = document.getElementById('btn-keyer-program');
  // Keyer key type buttons
  elBtnKeyerStraight = document.getElementById('btn-keyer-straight');
  elBtnKeyerIambic   = document.getElementById('btn-keyer-iambic');
}

// ── State ───────────────────────────────────────────────────
var ws            = null;
var kochActive    = false;
var currentWpm    = 20;
var currentLesson = 2;
var sessionCorrect = 0;
var sessionTotal   = 0;
var streak         = 0;
var inputEnabled   = false;
var lastPattern    = '';
var lastDitMs      = 60;
var buzzerEnabled  = true;
var kochPaused     = false;
var echoActive    = false;
var echoCorrect   = 0;
var echoTotal     = 0;
var echoSource    = 'Koch';   // 'Koch' or 'Full'
var keyType       = 'Straight'; // 'Straight' or 'Iambic'
var gridIsSorted  = false;
// Practice state
var practiceActive = false;
var practiceSent   = 0;
var practiceTotal  = 0;
var practiceWords  = [];

// Pre-built audio sequence waiting for the "go" signal (Koch or WORD)
var pendingSeq     = null;

// Active Koch characters
var activeKochChars = [];

// Fallback Koch order
var KOCH_ORDER_FALLBACK = 'KMURESNAPTLWI.JZ=FOY,VG5/Q92H38B?47C1D60X';
var KOCH_PROSIGNS_FALLBACK = [
  { idx: 44, ch: '^', display: 'AR' },
  { idx: 45, ch: '[', display: 'BT' },
  { idx: 46, ch: '|', display: 'SK' }
];

// Server-side per-character stats
var serverStats = null;

// ── WebSocket ───────────────────────────────────────────────
function connect() {
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
    return;
  }
  updateConnStatus('connecting');
  ws = new WebSocket('ws://' + location.host + '/ws');

  ws.onopen = function () {
    updateConnStatus('connected');
    wsSend({ type: 'get_status' });
    logEvent('Connected to trainer');
  };

  ws.onclose = function () {
    updateConnStatus('disconnected');
    setTimeout(connect, 2000);
  };

  ws.onerror = function () {
    updateConnStatus('disconnected');
  };

  ws.onmessage = function (evt) {
    var msg;
    try { msg = JSON.parse(evt.data); } catch (e) { return; }
    handleMessage(msg);
  };
}

function wsSend(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(obj));
  }
}

function updateConnStatus(state) {
  if (!elConnStatus) return;
  elConnStatus.className = 'status ' + state;
  if (state === 'connected')    elConnStatus.innerHTML = '● Online';
  else if (state === 'connecting') elConnStatus.innerHTML = '● Connecting…';
  else                           elConnStatus.innerHTML = '● Offline';
}

// ── Message Router ──────────────────────────────────────────
function handleMessage(msg) {
  switch (msg.type) {

    // ── Connection welcome (sent on WS connect) ──
    case 'connected':
    case 'status':
      currentWpm    = msg.wpm || currentWpm;
      currentLesson = msg.koch_lesson || currentLesson;
      kochActive    = !!msg.koch_active;
      kochPaused    = !!msg.koch_paused;
      if (msg.buzzer_enabled !== undefined) { buzzerEnabled = !!msg.buzzer_enabled; }
      if (msg.koch_chars) {
        applyKochChars(msg.koch_chars, msg.koch_lesson);
      }
      if (msg.echo_active !== undefined) echoActive = !!msg.echo_active;
      if (msg.echo_correct !== undefined) echoCorrect = msg.echo_correct;
      if (msg.echo_total !== undefined) echoTotal = msg.echo_total;
      if (msg.echo_source !== undefined) echoSource = msg.echo_source;
      if (msg.key_type !== undefined) keyType = msg.key_type;
      if (msg.practice_active !== undefined) {
        practiceActive = !!msg.practice_active;
        if (msg.practice_sent  !== undefined) practiceSent  = msg.practice_sent;
        if (msg.practice_total !== undefined) practiceTotal  = msg.practice_total;
      }
      if (msg.keyer_mode !== undefined) {
        keyerMode = (msg.keyer_mode === 'CWKeyer');
        updateKeyerOverlay();
      }
      if (msg.wifi_mode !== undefined) wifiMode = msg.wifi_mode;
      if (msg.wifi_ip   !== undefined) wifiIP   = msg.wifi_ip;
      updateWpmDisplay();
      updateLessonDisplay();
      updateWifiDisplay();
      updateControlButtons();
      updateBuzzerButton();
      updateEchoUi();
      updatePracticeUi();
      break;


    // ── Server-authoritative Koch character list ──
    case 'koch_chars':
      applyKochChars(msg.chars, msg.lesson);
      break;

    // ── Koch session started / stopped ──
    case 'koch_session':
      kochActive    = !!msg.started;
      currentLesson = msg.lesson || currentLesson;
      currentWpm    = msg.wpm || currentWpm;
      if (msg.started) {
        kochPaused = false;
        sessionCorrect = 0;
        sessionTotal   = 0;
        streak         = 0;
        charTrack      = {};
        updateScorebar();
        setDisplayWaiting();
        logEvent('Session started — Lesson ' + currentLesson + ' at ' + currentWpm + ' WPM');
      } else {
        kochPaused = false;
        inputEnabled = false;
        setCharBtnsEnabled(false);
        elDisplayChar.textContent = '—';
        elDisplayChar.className = 'display-char';
        elDisplayMorse.innerHTML = '&nbsp;';
        elDisplayMsg.textContent = 'Session ended.';
        logEvent('Session ended — ' + sessionCorrect + '/' + sessionTotal);
        fetchServerStats();
      }
      updateWpmDisplay();
      updateLessonDisplay();
      updateControlButtons();
      break;

    // ── Koch challenge: prepare audio, then ACK "ready" ──
    case 'koch_challenge':
      currentLesson = msg.lesson || currentLesson;
      updateLessonDisplay();
      setDisplayWaiting();
      inputEnabled = false;
      setCharBtnsEnabled(false);
      elDisplayMsg.textContent = 'Listen…';
      if (msg.pattern && msg.ditMs) {
        lastPattern = msg.pattern;
        lastDitMs   = msg.ditMs;
        pendingSeq  = buildMorseSeqRaw(msg.pattern, msg.ditMs);
        elBtnReplay.disabled = false;
        wsSend({ type: 'koch_ready' });
      }
      break;

    // ── Koch go: ESP32 says play now ──
    case 'koch_go':
      if (pendingSeq) {
        playPreparedSeq(pendingSeq);
        pendingSeq = null;
      }
      break;

    // ── Freeform WORD: prepare audio for one character ──
    case 'word_char':
      if (msg.pattern && msg.ditMs) {
        lastPattern = msg.pattern;
        lastDitMs   = msg.ditMs;
        pendingSeq  = buildMorseSeqRaw(msg.pattern, msg.ditMs);
        elDisplayChar.textContent = msg.char || '';
        elDisplayChar.className = 'display-char';
        elDisplayMorse.textContent = msg.pattern;
        // ACK only when previous audio has finished playing.
        // If idle, ACK immediately; otherwise the playSequence
        // completion callback will send it.
        if (!wordAudioPlaying) {
          wsSend({ type: 'word_ready' });
        } else {
          wordReadyPending = true;
        }
      }
      break;

    // ── Freeform WORD go: ESP32 says play now ──
    case 'word_go':
      if (pendingSeq) {
        playWordSeq(pendingSeq);
        pendingSeq = null;
      }
      break;

    // ── A character finished playing ──
    case 'char_sent':
      if (kochActive) {
        inputEnabled = true;
        setCharBtnsEnabled(true);
        elDisplayMsg.textContent = 'What did you hear?';
        if (msg.morse && !lastPattern) {
          lastPattern = msg.morse;
          lastDitMs = 1200 / currentWpm;
        }
        elBtnReplay.disabled = false;
      }
      break;

    // ── Koch answer result ──
    case 'koch_result':
      inputEnabled = false;
      setCharBtnsEnabled(false);
      sessionCorrect = msg.session_correct;
      sessionTotal   = msg.session_total;
      currentLesson  = msg.lesson || currentLesson;
      if (msg.correct) {
        streak++;
        showResult(msg.expected, true, msg.reaction_ms);
      } else {
        streak = 0;
        showResult(msg.expected, false, msg.reaction_ms, msg.received);
      }
      updateScorebar();
      updateLessonDisplay();
      flashCharBtn(msg.expected, msg.correct);
      if (!msg.correct && msg.received) {
        flashCharBtn(msg.received, false);
      }
      logResult(msg);

      // When the ESP32 signals all characters are mastered,
      // sort the grid alphabetically for easier button location.
      if (msg.sort_grid && !gridIsSorted) {
        gridIsSorted = true;
        sortCharGridAlphabetical();
        logEvent('Grid sorted alphabetically — all characters mastered!');
      }
      break;

    // ── Koch generic event ──
    case 'koch_event':
      elDisplayMsg.textContent = msg.message || '';
      logEvent(msg.message || '');
      break;

    // ── Speed change ──
    case 'speed_change':
      currentWpm = msg.wpm || currentWpm;
      updateWpmDisplay();
      break;

    // ── Playback messages ──
    case 'playback_start':
      elDisplayMsg.textContent = 'Sending: ' + (msg.message || '');
      break;
    case 'playback_done':
      cancelAllSequences();
      if (!kochActive) {
        elDisplayMsg.textContent = 'Done.';
      }
      // Clear keyer playing state
      for (var pd = 0; pd < 6; pd++) {
        if (elKeyerMemBtns[pd]) elKeyerMemBtns[pd].classList.remove('playing');
      }
      if (elKeyerStop) elKeyerStop.disabled = true;
      break;

    // ── Pong ──
    case 'pong':
      logEvent('Pong — uptime ' + msg.uptime_ms + 'ms');
      break;

    // ── Buzzer mute state ──
    case 'buzzer_state':
      buzzerEnabled = !!msg.enabled;
      updateBuzzerButton();
      break;

    // ── Koch pause / resume ──
    case 'koch_pause':
      kochPaused = !!msg.paused;
      if (kochPaused) {
        inputEnabled = false;
        setCharBtnsEnabled(false);
        pendingSeq = null;
        elDisplayMsg.textContent = 'Paused.';
        logEvent('Session paused');
      } else {
        elDisplayMsg.textContent = 'Resumed — listen…';
        logEvent('Session resumed');
      }
      updateControlButtons();
      break;

    // ── Echo commands ──
    case 'echo_session':
      echoActive  = !!msg.started;
      echoCorrect = msg.correct || 0;
      echoTotal   = msg.total   || 0;
      updateEchoUi();
      logEvent(echoActive ? 'Echo training started' : 'Echo training stopped');
      break;
    case 'echo_prompt':
      if (msg.char) {
        elDisplayChar.textContent = msg.char;
        elDisplayChar.className   = 'display-char';
        elDisplayMorse.textContent = msg.pattern || '';
        elDisplayMsg.textContent   = 'Echo: listen…';
      }
      break;
    case 'echo_result':
      echoCorrect = msg.session_correct || echoCorrect;
      echoTotal   = msg.session_total   || echoTotal;
      updateEchoUi();
      var txt = msg.correct
        ? ('Echo OK ' + msg.expected)
        : ('Echo MISS exp ' + msg.expected + ' got ' + (msg.received || '?'));
      elDisplayMsg.textContent = txt;
      elDisplayChar.textContent = msg.expected || '—';
      elDisplayChar.className = 'display-char ' + (msg.correct ? 'correct' : 'wrong');
      logEvent(txt);
      break;

    // ── Echo config (source changed via serial or another client) ──
    case 'echo_config':
      if (msg.echo_source) {
        echoSource = msg.echo_source;
        updateEchoUi();
        logEvent('Echo source: ' + echoSource);
      }
      if (msg.key_type) {
        keyType = msg.key_type;
        updateEchoUi();
        updateKeyerKeyTypeButtons();
      }
      break;

    // ── Key config (key type changed via serial or another client) ──
    case 'key_config':
      if (msg.key_type) {
        keyType = msg.key_type;
        updateEchoUi();
        updateKeyerKeyTypeButtons();
        logEvent('Key type: ' + keyType);
      }
      break;

    // ── Practice player ──
    case 'practice_session':
      practiceActive = !!msg.started;
      practiceSent   = msg.words_sent  || 0;
      practiceTotal  = msg.total_words || 0;
      if (msg.started) {
        practiceWords = [];
        logEvent('Practice started — ' + practiceTotal + ' words at ' + (msg.wpm || currentWpm) + ' WPM');
      } else {
        logEvent('Practice stopped — ' + practiceSent + '/' + practiceTotal + ' words');
      }
      updatePracticeUi();
      break;

    case 'practice_word':
      practiceSent = msg.word_num || practiceSent;
      practiceTotal = msg.total_words || practiceTotal;
      if (msg.word) {
        practiceWords.push(msg.word);
        elDisplayChar.textContent = msg.word;
        elDisplayChar.className = 'display-char';
        elDisplayMorse.innerHTML = '&nbsp;';
        elDisplayMsg.textContent = 'Practice: ' + msg.word_num + '/' + msg.total_words;
      }
      updatePracticeUi();
      break;

    // ── Keyer mode toggle ──
    case 'keyer_mode':
      keyerMode = (msg.keyer_mode === 'CWKeyer');
      updateKeyerOverlay();
      logEvent(keyerMode ? 'CW Keyer mode active' : 'Trainer mode active');
      break;
  
    // ── Keyer real-time TX sidetone ──
    case 'keyer_tx':
      if (msg.on) {
        startKeyerTone();
      } else {
        stopKeyerTone();
      }
      break;

    // ── Keyer memories from ESP32 ──
    case 'keyer_memories':
      if (Array.isArray(msg.memories)) {
        for (var km = 0; km < 6 && km < msg.memories.length; km++) {
          keyerMemories[km] = msg.memories[km] || '';
        }
        updateKeyerMemButtons();
      }
      break;

    // ── Keyer memory playback: full pattern in one shot ──
    case 'keyer_play':
      if (msg.pattern && msg.ditMs) {
        cancelAllSequences();
        var kseq = buildKeyerSeq(msg.pattern, msg.ditMs);
        // Add audio primer if idle (mobile browsers need a wake-up tone)
        var now = Date.now();
        if (now - lastToneEndMs > AUDIO_IDLE_THRESHOLD) {
          kseq.unshift({ tone: false, ms: 20 });
          kseq.unshift({ tone: true,  ms: AUDIO_PRIMER_MS, silent: true });
        }
        playPreparedSeq(kseq);
      }
      break;

    // ── Keyer memory word-by-word playback ──
    case 'keyer_word':
      if (msg.pattern && msg.ditMs) {
        var kwSeq = buildKeyerSeq(msg.pattern, msg.ditMs);
        if (msg.go) {
          // First word: cancel any previous, add primer, play immediately
          cancelAllSequences();
          var kwNow = Date.now();
          if (kwNow - lastToneEndMs > AUDIO_IDLE_THRESHOLD) {
            kwSeq.unshift({ tone: false, ms: 20 });
            kwSeq.unshift({ tone: true, ms: AUDIO_PRIMER_MS, silent: true });
          }
          playKeyerWordSeq(kwSeq);
        } else {
          // Subsequent word: prepend word gap to match ESP32 timing
          // ESP32 plays ELEMENT_GAP + WORD_GAP = 8 dits between words.
          // The WS round-trip eats ~20-50ms of that, so subtract a small
          // allowance to stay aligned rather than falling behind.
          var wordGapMs = msg.ditMs * 8;
          var rttAllowance = 30;
          var gapMs = wordGapMs - rttAllowance;
          if (gapMs < msg.ditMs) gapMs = msg.ditMs;
          kwSeq.unshift({ tone: false, ms: gapMs });
          playKeyerWordSeq(kwSeq);
        }
      }
      break;

    default:
      break;
  }
}

// ── Display Helpers ─────────────────────────────────────────
function setDisplayWaiting() {
  elDisplayChar.textContent = '?';
  elDisplayChar.className = 'display-char waiting';
  elDisplayMorse.innerHTML = '&nbsp;';
}

function showResult(expected, correct, reactionMs, received) {
  elDisplayChar.textContent = expected;
  elDisplayChar.className = 'display-char ' + (correct ? 'correct' : 'wrong');
  elDisplayMsg.textContent = correct
    ? '✓ Correct  (' + reactionMs + 'ms)'
    : '✗ Wrong — was ' + expected + ', you sent ' + (received || '?') + '  (' + reactionMs + 'ms)';

  if (lastPattern) {
    elDisplayMorse.textContent = lastPattern;
  }
}

function updateScorebar() {
  elScoreCorrect.textContent = sessionCorrect;
  elScoreTotal.textContent   = sessionTotal;
  if (sessionTotal > 0) {
    elScorePct.textContent = Math.round(100 * sessionCorrect / sessionTotal) + '%';
  } else {
    elScorePct.textContent = '—';
  }
  elScoreStreak.textContent = '🔥 ' + streak;
}

function updateWpmDisplay() {
  elSpeedDisplay.textContent = currentWpm;
  elHeaderWpm.textContent    = currentWpm + ' WPM';
  if (elKeyerSpeedDisp) elKeyerSpeedDisp.textContent = currentWpm;
}

function updateLessonDisplay() {
  elHeaderLesson.textContent = 'Lesson ' + currentLesson;
}

function updateControlButtons() {
  elBtnStart.disabled = kochActive;
  elBtnStop.disabled  = !kochActive;
  elBtnPause.disabled = !kochActive;
  if (elBtnReset) { elBtnReset.disabled = kochActive; }
  if (kochPaused) {
    elBtnPause.textContent = '▶ Resume';
    elBtnPause.classList.add('resuming');
  } else {
    elBtnPause.textContent = '⏸ Pause';
    elBtnPause.classList.remove('resuming');
  }
}

// ── Character Grid ──────────────────────────────────────────

function applyKochChars(chars, lesson) {
  if (!Array.isArray(chars) || chars.length === 0) return;
  activeKochChars = chars;
  if (lesson) currentLesson = lesson;
  buildCharGrid();
}

function charSortKey(entry) {
  var d = entry.display;
  var c = d.charCodeAt(0);
  // Letters A-Z: 0-25
  if (d.length === 1 && d >= 'A' && d <= 'Z') return c - 65;
  // Digits 0-9: 26-35
  if (d.length === 1 && d >= '0' && d <= '9') return 26 + (c - 48);
  // Punctuation by char code: 36+
  if (d.length === 1) return 36 + c;
  // Prosigns (AR, BT, SK): sort after everything else
  return 200 + c;
}

function buildCharGrid() {
  if (!elCharGrid) return;
  elCharGrid.innerHTML = '';

  if (activeKochChars.length === 0) {
    buildCharGridFallback();
    return;
  }

  // Sort alphabetically: A-Z, then 0-9, then punctuation, then prosigns
  var sorted = activeKochChars.slice().sort(function (a, b) {
    return charSortKey(a) - charSortKey(b);
  });

  for (var i = 0; i < sorted.length; i++) {
    var entry = sorted[i];
    var btn = document.createElement('button');
    btn.className = 'char-btn';
    btn.textContent = entry.display;
    btn.setAttribute('data-char', entry.ch);
    btn.disabled = !inputEnabled;
    btn.addEventListener('click', onCharBtnClick);
    elCharGrid.appendChild(btn);
  }
}

function buildCharGridFallback() {
  if (!elCharGrid) return;
  elCharGrid.innerHTML = '';

  var count = currentLesson;
  var maxCount = KOCH_ORDER_FALLBACK.length + KOCH_PROSIGNS_FALLBACK.length;
  if (count > maxCount) count = maxCount;

  for (var i = 0; i < count; i++) {
    var ch, display;
    if (i < KOCH_ORDER_FALLBACK.length) {
      ch = KOCH_ORDER_FALLBACK[i];
      display = ch;
    } else {
      var pi = i - KOCH_ORDER_FALLBACK.length;
      if (pi < KOCH_PROSIGNS_FALLBACK.length) {
        ch = KOCH_PROSIGNS_FALLBACK[pi].ch;
        display = KOCH_PROSIGNS_FALLBACK[pi].display;
      } else {
        break;
      }
    }
    var btn = document.createElement('button');
    btn.className = 'char-btn';
    btn.textContent = display;
    btn.setAttribute('data-char', ch);
    btn.disabled = !inputEnabled;
    btn.addEventListener('click', onCharBtnClick);
    elCharGrid.appendChild(btn);
  }
}

function onCharBtnClick(e) {
  if (!inputEnabled) return;
  var ch = e.currentTarget.getAttribute('data-char');
  if (!ch) return;
  inputEnabled = false;
  setCharBtnsEnabled(false);
  wsSend({ type: 'koch_answer', char: ch });
}

function setCharBtnsEnabled(enabled) {
  var btns = elCharGrid.querySelectorAll('.char-btn');
  for (var i = 0; i < btns.length; i++) {
    btns[i].disabled = !enabled;
  }
}

function flashCharBtn(ch, correct) {
  ch = ch.toUpperCase();
  var btns = elCharGrid.querySelectorAll('.char-btn');
  for (var i = 0; i < btns.length; i++) {
    var btnChar = btns[i].getAttribute('data-char');
    if (btnChar === ch || btns[i].textContent === ch) {
      btns[i].classList.add(correct ? 'correct' : 'incorrect');
      (function (b) {
        setTimeout(function () {
          b.classList.remove('correct', 'incorrect');
        }, 900);
      })(btns[i]);
    }
  }
}

// ── Commands ────────────────────────────────────────────────
function cmdKochStart() {
  unlockAudio();
  wsSend({ type: 'command', cmd: 'koch start' });
}

function cmdKochStop() {
  pendingSeq = null;
  wsSend({ type: 'koch_stop' });
}

function cmdKochPause() {
  if (kochPaused) {
    wsSend({ type: 'koch_resume' });
  } else {
    wsSend({ type: 'koch_pause' });
  }
}

function cmdReplay() {
  unlockAudio();
  wsSend({ type: 'koch_replay' });
}

function cmdKochReset() {
  if (!confirm('Reset ALL Koch progress?\n\nThis clears your lesson, speed, and all character statistics.')) {
    return;
  }
  wsSend({ type: 'koch_reset' });
  sessionCorrect = 0;
  sessionTotal   = 0;
  streak         = 0;
  charTrack      = {};
  serverStats    = null;
  currentLesson  = 2;
  currentWpm     = 20;
  lastPattern    = '';
  pendingSeq     = null;
  gridIsSorted   = false;
  updateScorebar();
  updateWpmDisplay();
  updateLessonDisplay();
  renderStatsPanel();
  elDisplayChar.textContent = '—';
  elDisplayChar.className = 'display-char';
  elDisplayMorse.innerHTML = '&nbsp;';
  elDisplayMsg.textContent = 'Progress reset.';
  logEvent('All Koch progress reset to defaults');
  setTimeout(fetchServerStats, 500);
}

function cmdSpeed(delta) {
  var newWpm = currentWpm + delta;
  if (newWpm < 1) newWpm = 1;
  if (newWpm > 60) newWpm = 60;
  wsSend({ type: 'set_wpm', wpm: newWpm });
}

function cmdBuzzerToggle() {
  buzzerEnabled = !buzzerEnabled;
  wsSend({ type: 'set_buzzer', enabled: buzzerEnabled ? 'true' : 'false' });
  updateBuzzerButton();
}

function updateBuzzerButton() {
  if (!elBtnBuzzer) return;
  var label = buzzerEnabled ? '🔔 Buzzer' : '🔕 Muted';
  var muted = !buzzerEnabled;
  elBtnBuzzer.textContent = label;
  if (muted) { elBtnBuzzer.classList.add('muted'); } else { elBtnBuzzer.classList.remove('muted'); }
  if (elKeyerBuzzer) {
    elKeyerBuzzer.textContent = label;
    if (muted) { elKeyerBuzzer.classList.add('muted'); } else { elKeyerBuzzer.classList.remove('muted'); }
  }
}

// ── Practice Commands ───────────────────────────────────────
function cmdPracticeSend() {
  if (!elPracticeText) return;
  var text = elPracticeText.value.trim();
  if (text.length === 0) {
    elPracticeStatus.textContent = 'Enter text first.';
    return;
  }
  unlockAudio();
  text = text.replace(/\r\n|\r|\n/g, ' ');
  wsSend({ type: 'practice_text', text: text });
}

function cmdPracticeFile() {
  unlockAudio();
  wsSend({ type: 'practice_file' });
}

function cmdPracticeStop() {
  wsSend({ type: 'practice_stop' });
}

function updatePracticeUi() {
  if (!elPracticeStatus) return;

  if (practiceActive) {
    elPracticeStatus.textContent = 'Playing: ' + practiceSent + ' / ' + practiceTotal + ' words';
    elPracticeStatus.className = 'practice-status active';
  } else {
    if (practiceSent > 0 && practiceSent >= practiceTotal) {
      elPracticeStatus.textContent = 'Complete — ' + practiceTotal + ' words';
    } else if (practiceSent > 0) {
      elPracticeStatus.textContent = 'Stopped at ' + practiceSent + ' / ' + practiceTotal;
    } else {
      elPracticeStatus.textContent = 'Idle';
    }
    elPracticeStatus.className = 'practice-status';
  }

  if (elBtnPracSend) elBtnPracSend.disabled = practiceActive;
  if (elBtnPracFile) elBtnPracFile.disabled = practiceActive;
  if (elBtnPracStop) elBtnPracStop.disabled = !practiceActive;

  if (elPracticeProgress && practiceWords.length > 0) {
    var html = '';
    for (var i = 0; i < practiceWords.length; i++) {
      if (i === practiceWords.length - 1) {
        html += '<span class="current-word">' + practiceWords[i] + '</span> ';
      } else {
        html += practiceWords[i] + ' ';
      }
    }
    elPracticeProgress.innerHTML = html;
  } else if (elPracticeProgress) {
    elPracticeProgress.innerHTML = '';
  }
}

// ── Echo UI ─────────────────────────────────────────────────
function updateEchoUi() {
  if (elEchoStatus) {
    if (echoActive) {
      elEchoStatus.textContent = 'Echo ACTIVE — ' + echoSource +
        ' (' + echoCorrect + '/' + echoTotal + ')';
      elEchoStatus.className = 'echo-status active';
    } else {
      elEchoStatus.textContent = 'Echo inactive — ' + echoSource;
      elEchoStatus.className = 'echo-status';
    }
  }
  if (elEchoResult) {
    elEchoResult.textContent = echoTotal > 0 ? (echoCorrect + '/' + echoTotal) : '—';
  }

  if (elBtnEchoKoch) {
    if (echoSource === 'Koch') {
      elBtnEchoKoch.classList.add('active');
    } else {
      elBtnEchoKoch.classList.remove('active');
    }
    elBtnEchoKoch.disabled = echoActive;
  }
  if (elBtnEchoFull) {
    if (echoSource === 'Full') {
      elBtnEchoFull.classList.add('active');
    } else {
      elBtnEchoFull.classList.remove('active');
    }
    elBtnEchoFull.disabled = echoActive;
  }
  if (elBtnEchoStop) {
    elBtnEchoStop.disabled = !echoActive;
  }

  // Echo panel key type buttons (btn-key-straight / btn-key-iambic)
  if (elBtnKeyStraight) {
    if (keyType === 'Straight') {
      elBtnKeyStraight.classList.add('active');
    } else {
      elBtnKeyStraight.classList.remove('active');
    }
    elBtnKeyStraight.disabled = echoActive;
  }
  if (elBtnKeyIambic) {
    if (keyType === 'Iambic') {
      elBtnKeyIambic.classList.add('active');
    } else {
      elBtnKeyIambic.classList.remove('active');
    }
    elBtnKeyIambic.disabled = echoActive;
  }
}

function cmdEchoKoch() {
  unlockAudio();
  wsSend({ type: 'set_echo_source', source: 'Koch' });
  echoSource = 'Koch';
  updateEchoUi();
  setTimeout(function () { wsSend({ type: 'echo_start' }); }, 50);
}

function cmdEchoFull() {
  unlockAudio();
  wsSend({ type: 'set_echo_source', source: 'Full' });
  echoSource = 'Full';
  updateEchoUi();
  setTimeout(function () { wsSend({ type: 'echo_start' }); }, 50);
}

function cmdEchoStop() { wsSend({ type: 'echo_stop' }); }

function cmdKeyStraight() {
  wsSend({ type: 'set_key_type', key_type: 'Straight' });
  keyType = 'Straight';
  updateEchoUi();
  updateKeyerKeyTypeButtons();
  logEvent('Key type: Straight');
}

function cmdKeyIambic() {
  wsSend({ type: 'set_key_type', key_type: 'Iambic' });
  keyType = 'Iambic';
  updateEchoUi();
  updateKeyerKeyTypeButtons();
  logEvent('Key type: Iambic');
}

// ── File Editor Commands ────────────────────────────────────
function cmdFileLoad() {
  if (!elFileEditorText || !elFileEditorStatus) return;
  elFileEditorStatus.textContent = 'Loading…';
  elFileEditorStatus.className = 'file-editor-status';
  fetch('/api/practice')
    .then(function (res) {
      if (!res.ok) throw new Error('HTTP ' + res.status);
      return res.text();
    })
    .then(function (text) {
      elFileEditorText.value = text;
      elFileEditorStatus.textContent = text.length > 0
        ? 'Loaded ' + text.length + ' bytes'
        : 'File is empty — type your practice text below.';
      elFileEditorStatus.className = 'file-editor-status';
    })
    .catch(function (err) {
      elFileEditorStatus.textContent = 'Load failed: ' + err.message;
      elFileEditorStatus.className = 'file-editor-status error';
    });
}

function cmdFileSave() {
  if (!elFileEditorText || !elFileEditorStatus) return;
  var text = elFileEditorText.value;
  elFileEditorStatus.textContent = 'Saving…';
  elFileEditorStatus.className = 'file-editor-status';
  fetch('/api/practice', {
    method: 'POST',
    headers: { 'Content-Type': 'application/octet-stream' },
    body: text
  })
    .then(function (res) {
      if (!res.ok) throw new Error('HTTP ' + res.status);
      return res.json();
    })
    .then(function (data) {
      if (!data.ok) throw new Error(data.error || 'unknown error');
      var bytes = data.bytes !== undefined ? data.bytes : text.length;
      elFileEditorStatus.textContent = 'Saved (' + bytes + ' bytes)';
      elFileEditorStatus.className = 'file-editor-status saved';
      logEvent('Practice file saved — ' + bytes + ' bytes');
    })
    .catch(function (err) {
      elFileEditorStatus.textContent = 'Save failed: ' + err.message;
      elFileEditorStatus.className = 'file-editor-status error';
    });
}

// ── Web Audio API ───────────────────────────────────────────
var audioCtx      = null;
var gainNode      = null;
var toneFreq      = 700;
var audioUnlocked = false;
var lastToneEndMs = 0;

var AUDIO_IDLE_THRESHOLD = 1500;
var AUDIO_PRIMER_MS      = 150;

var wordAudioPlaying  = false;
var wordReadyPending  = false;
var seqId             = 0;        // incremented to cancel in-flight sequences

function initAudio() {
  if (audioCtx) return true;
  try {
    var AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return false;
    audioCtx = new AC();
    gainNode = audioCtx.createGain();
    gainNode.gain.value = 0.5;
    gainNode.connect(audioCtx.destination);
    return true;
  } catch (e) {
    return false;
  }
}

function unlockAudio() {
  if (!initAudio()) return;
  try { audioCtx.resume(); } catch (e) { /* best effort */ }
  if (!audioUnlocked) {
    try {
      var silent = audioCtx.createOscillator();
      var silentGain = audioCtx.createGain();
      silentGain.gain.value = 0;
      silent.connect(silentGain);
      silentGain.connect(audioCtx.destination);
      silent.start();
      silent.stop(audioCtx.currentTime + 0.001);
      audioUnlocked = true;
    } catch (e) { /* best effort */ }
  }
}

function buildMorseSeqRaw(pattern, ditMs) {
  var seq = [];
  for (var i = 0; i < pattern.length; i++) {
    var el = pattern[i];
    if (el === '.' || el === '-') {
      seq.push({ tone: true,  ms: (el === '.') ? ditMs : ditMs * 3 });
      seq.push({ tone: false, ms: ditMs });
    } else if (el === ' ') {
      seq.push({ tone: false, ms: ditMs * 2 });
    }
  }
  return seq;
}

function buildMorseSeq(pattern, ditMs) {
  var seq = buildMorseSeqRaw(pattern, ditMs);
  var now = Date.now();
  if (now - lastToneEndMs > AUDIO_IDLE_THRESHOLD) {
    seq.unshift({ tone: false, ms: 20 });
    seq.unshift({ tone: true,  ms: AUDIO_PRIMER_MS, silent: true });
  }
  return seq;
}

function playPreparedSeq(seq) {
  if (!initAudio()) return;
  if (!seq || seq.length === 0) return;
  lastToneEndMs = Date.now();
  if (audioCtx.state !== 'running') {
    try {
      var p = audioCtx.resume();
      if (p && typeof p.then === 'function') {
        p.then(function () { playSequence(seq, null); });
      } else {
        setTimeout(function () { playSequence(seq, null); }, 60);
      }
    } catch (e) {
      setTimeout(function () { playSequence(seq, null); }, 60);
    }
    return;
  }
  playSequence(seq, null);
}

function playWordSeq(seq) {
  if (!initAudio()) return;
  if (!seq || seq.length === 0) return;
  wordAudioPlaying = true;
  lastToneEndMs = Date.now();
  var onDone = function () {
    wordAudioPlaying = false;
    lastToneEndMs = Date.now();
    if (wordReadyPending) {
      wordReadyPending = false;
      wsSend({ type: 'word_ready' });
    }
  };
  if (audioCtx.state !== 'running') {
    try {
      var p = audioCtx.resume();
      if (p && typeof p.then === 'function') {
        p.then(function () { playSequence(seq, onDone); });
      } else {
        setTimeout(function () { playSequence(seq, onDone); }, 60);
      }
    } catch (e) {
      setTimeout(function () { playSequence(seq, onDone); }, 60);
    }
    return;
  }
  playSequence(seq, onDone);
}

function playKeyerWordSeq(seq) {
  if (!initAudio()) return;
  if (!seq || seq.length === 0) {
    wsSend({ type: 'keyer_word_done' });
    return;
  }
  lastToneEndMs = Date.now();
  var onDone = function () {
    lastToneEndMs = Date.now();
    wsSend({ type: 'keyer_word_done' });
  };
  if (audioCtx.state !== 'running') {
    try {
      var p = audioCtx.resume();
      if (p && typeof p.then === 'function') {
        p.then(function () { playSequence(seq, onDone); });
      } else {
        setTimeout(function () { playSequence(seq, onDone); }, 60);
      }
    } catch (e) {
      setTimeout(function () { playSequence(seq, onDone); }, 60);
    }
    return;
  }
  playSequence(seq, onDone);
}

function playMorseTone(pattern, ditMs) {
  var seq = buildMorseSeq(pattern, ditMs);
  if (seq.length === 0) return;
  playPreparedSeq(seq);
}

function playSequence(seq, onComplete) {
  var myId = ++seqId;
  var idx = 0;
  function next() {
    if (myId !== seqId) return;   // cancelled
    if (idx >= seq.length) {
      lastToneEndMs = Date.now();
      if (onComplete) onComplete();
      return;
    }
    var step = seq[idx++];
    if (step.tone) {
      var osc = audioCtx.createOscillator();
      osc.type = 'sine';
      osc.frequency.value = toneFreq;
      if (step.silent) {
        var muteGain = audioCtx.createGain();
        muteGain.gain.value = 0;
        osc.connect(muteGain);
        muteGain.connect(audioCtx.destination);
      } else {
        osc.connect(gainNode);
      }
      osc.start();
      setTimeout(function () {
        try { osc.stop(); } catch (e) { /* already stopped */ }
        next();
      }, step.ms);
    } else {
      setTimeout(next, step.ms);
    }
  }
  next();
}

function testTone() {
  unlockAudio();
  playMorseTone('-.-', 1200 / currentWpm);
}

function setVolume(val) {
  if (!initAudio()) return;
  gainNode.gain.value = val / 100;
}

// ── Stats Panel ─────────────────────────────────────────────

function fetchServerStats() {
  fetch('/api/koch_stats')
    .then(function (res) { return res.json(); })
    .then(function (data) {
      serverStats = data;
      renderStatsPanel();
    })
    .catch(function () {
      renderStatsPanel();
    });
}

function renderStatsPanel() {
  if (!elStatsBody) return;

  if (!serverStats && sessionTotal === 0) {
    elStatsBody.innerHTML = '<p class="stats-placeholder">Start a session to see stats.</p>';
    return;
  }

  var html = '';

  if (sessionTotal > 0) {
    var pct = Math.round(100 * sessionCorrect / sessionTotal);
    html += '<p>Session: <b>' + sessionCorrect + '/' + sessionTotal +
            '</b> (' + pct + '%) — Streak: <b>' + streak + '</b></p>';
  }

  if (serverStats && serverStats.chars && serverStats.chars.length > 0) {
    html += '<h4 style="margin:8px 0 4px;color:#f39c12;">All-Time Stats (Server)</h4>';

    var weakest = [];
    for (var w = 0; w < serverStats.chars.length; w++) {
      var sw = serverStats.chars[w];
      if (sw.sent >= 3) {
        var wa = Math.round(100 * sw.correct / sw.sent);
        if (wa < 70) weakest.push(sw.display);
      }
    }
    if (weakest.length > 0) {
      html += '<p style="color:#e74c3c;">⚠ Weakest: <b>' + weakest.join(', ') + '</b></p>';
    }

    html += '<table class="stats-table">';
    html += '<tr><th>Ch</th><th>Sent</th><th>Correct</th><th>Acc</th><th>Avg ms</th><th>Wt</th></tr>';

    var sorted = serverStats.chars.slice().sort(function (a, b) {
      if (a.sent === 0 && b.sent === 0) return 0;
      if (a.sent === 0) return 1;
      if (b.sent === 0) return -1;
      return (a.correct / a.sent) - (b.correct / b.sent);
    });

    for (var i = 0; i < sorted.length; i++) {
      var e = sorted[i];
      var acc = e.sent > 0 ? Math.round(100 * e.correct / e.sent) : 0;
      var cls = '';
      if (e.sent > 0) {
        cls = acc < 70 ? 'weak' : (acc >= 90 ? 'strong' : '');
      }
      html += '<tr class="' + cls + '">';
      html += '<td>' + e.display + '</td>';
      html += '<td>' + e.sent + '</td>';
      html += '<td>' + e.correct + '</td>';
      html += '<td>' + (e.sent > 0 ? acc + '%' : '—') + '</td>';
      html += '<td>' + (e.sent > 0 ? e.avg_ms : '—') + '</td>';
      html += '<td>' + e.prob + '</td>';
      html += '</tr>';
    }
    html += '</table>';
  }

  if (charTrack && Object.keys(charTrack).length > 0) {
    html += '<h4 style="margin:8px 0 4px;color:#2ecc71;">This Session (Local)</h4>';
    html += '<table class="stats-table"><tr><th>Ch</th><th>Sent</th><th>Correct</th><th>Acc</th><th>Avg ms</th></tr>';
    var entries = [];
    for (var ch in charTrack) {
      if (charTrack.hasOwnProperty(ch)) entries.push(charTrack[ch]);
    }
    entries.sort(function (a, b) {
      var accA = a.sent > 0 ? a.correct / a.sent : 1;
      var accB = b.sent > 0 ? b.correct / b.sent : 1;
      return accA - accB;
    });
    for (var j = 0; j < entries.length; j++) {
      var le = entries[j];
      var lacc = le.sent > 0 ? Math.round(100 * le.correct / le.sent) : 0;
      var lavg = le.sent > 0 ? Math.round(le.totalMs / le.sent) : 0;
      var lcls = lacc < 70 ? 'weak' : (lacc >= 90 ? 'strong' : '');
      html += '<tr class="' + lcls + '"><td>' + le.display + '</td><td>' + le.sent +
              '</td><td>' + le.correct + '</td><td>' + lacc + '%</td><td>' + lavg + '</td></tr>';
    }
    html += '</table>';
  }

  elStatsBody.innerHTML = html;
}

function updateStatsPanel() {
  renderStatsPanel();
}

var charTrack = {};

function trackCharResult(msg) {
  var ch = msg.expected;
  if (!ch) return;
  if (!charTrack[ch]) {
    charTrack[ch] = { ch: ch, display: ch, sent: 0, correct: 0, totalMs: 0 };
  }
  charTrack[ch].sent++;
  if (msg.correct) charTrack[ch].correct++;
  charTrack[ch].totalMs += (msg.reaction_ms || 0);
}

// ── Event Log ───────────────────────────────────────────────
var MAX_LOG = 80;

function logResult(msg) {
  trackCharResult(msg);
  updateStatsPanel();
  var cls = msg.correct ? 'correct' : 'wrong';
  var text = msg.correct
    ? '✓ ' + msg.expected + ' (' + msg.reaction_ms + 'ms)'
    : '✗ expected ' + msg.expected + ', got ' + (msg.received || '?') + ' (' + msg.reaction_ms + 'ms)';
  addLogEntry(text, cls);
}

function logEvent(text) {
  addLogEntry(text, 'event');
}

function addLogEntry(text, cls) {
  if (!elLogBody) return;
  var div = document.createElement('div');
  div.className = 'log-entry ' + (cls || '');
  div.textContent = text;
  elLogBody.insertBefore(div, elLogBody.firstChild);
  while (elLogBody.children.length > MAX_LOG) {
    elLogBody.removeChild(elLogBody.lastChild);
  }
}

// ── Init ────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', function () {
  cacheDom();
  buildCharGrid();
  updateScorebar();
  updateWpmDisplay();
  updateLessonDisplay();
  updateBuzzerButton();
  updateControlButtons();
  updateEchoUi();
  updatePracticeUi();
  updateKeyerOverlay();
  setupKeyerLongPress();
  connect();
  fetchServerStats();

  // Auto-load practice file when editor panel is expanded
  var editorPanel = document.getElementById('file-editor-panel');
  if (editorPanel) {
    editorPanel.addEventListener('toggle', function () {
      if (editorPanel.open) cmdFileLoad();
    });
  }

  // Unlock AudioContext on the very first user interaction.
  // Mobile browsers require a user gesture before audio can play.
  // Without this, Echo training and Keyer sidetone have no audio
  // because their playback is triggered by WebSocket messages,
  // not by user taps.
  function onFirstInteraction() {
    unlockAudio();
    document.removeEventListener('touchstart', onFirstInteraction, true);
    document.removeEventListener('click', onFirstInteraction, true);
  }
  document.addEventListener('touchstart', onFirstInteraction, true);
  document.addEventListener('click', onFirstInteraction, true);
});

function sortCharGridAlphabetical() {
  if (activeKochChars.length === 0) return;

  activeKochChars.sort(function (a, b) {
    return sortKey(a) - sortKey(b);
  });
  buildCharGrid();
}

function sortKey(entry) {
  var d = entry.display;
  var c = d.charCodeAt(0);
  if (d.length === 1 && d >= 'A' && d <= 'Z') return c - 65;
  if (d.length === 1 && d >= '0' && d <= '9') return 26 + (c - 48);
  if (d.length === 1) return 36 + c;
  return 200 + c;
}

// Keyer mode variables
var keyerMode       = false;
var keyerMemories   = ['', '', '', '', '', ''];
var keyerProgramSlot = -1;
var keyerSelectMode  = false;

// ── Keyer Overlay ───────────────────────────────────────────
function updateKeyerOverlay() {
  if (!elKeyerOverlay) return;
  if (keyerMode) {
    elKeyerOverlay.classList.remove('hidden');
    unlockAudio();
    wsSend({ type: 'keyer_mem_get' });
    updateKeyerKeyTypeButtons();
    updateKeyerProgramButton();
    ensureKeyerToneNode(); // pre-create oscillator so first key-down is instant
  } else {
    elKeyerOverlay.classList.add('hidden');
    exitKeyerProgramMode();
    destroyKeyerTone();
  }
}

function updateKeyerMemButtons() {
  for (var i = 0; i < 6; i++) {
    var btn = elKeyerMemBtns[i];
    if (!btn) continue;
    btn.classList.remove('unprogrammed', 'programmed', 'playing', 'programming', 'select-target');
    if (keyerProgramSlot === i) {
      btn.classList.add('programming');
    } else if (keyerSelectMode) {
      btn.classList.add('select-target');
    } else if (keyerMemories[i].length > 0) {
      btn.classList.add('programmed');
    } else {
      btn.classList.add('unprogrammed');
    }
  }
  if (elKeyerStop) elKeyerStop.disabled = true;
}

function updateKeyerProgramButton() {
  if (!elBtnKeyerProgram) return;
  if (keyerSelectMode || keyerProgramSlot >= 0) {
    elBtnKeyerProgram.classList.add('active');
    elBtnKeyerProgram.textContent = '✏️ Cancel';
  } else {
    elBtnKeyerProgram.classList.remove('active');
    elBtnKeyerProgram.textContent = '✏️ Program';
  }
}

function updateKeyerKeyTypeButtons() {
  if (elBtnKeyerStraight) {
    elBtnKeyerStraight.classList.remove('confirming');
    if (keyType === 'Straight') {
      elBtnKeyerStraight.classList.add('active');
    } else {
      elBtnKeyerStraight.classList.remove('active');
    }
  }
  if (elBtnKeyerIambic) {
    elBtnKeyerIambic.classList.remove('confirming');
    if (keyType === 'Iambic') {
      elBtnKeyerIambic.classList.add('active');
    } else {
      elBtnKeyerIambic.classList.remove('active');
    }
    elBtnKeyerIambic.disabled = echoActive;
  }
}

function cmdKeyerProgramToggle() {
  if (keyerProgramSlot >= 0) {
    cmdKeyerMemCancel();
    return;
  }
  if (keyerSelectMode) {
    exitKeyerSelectMode();
  } else {
    keyerSelectMode = true;
    if (elKeyerStatus) {
      elKeyerStatus.textContent = 'Tap a memory slot to program…';
      elKeyerStatus.className = 'keyer-status';
    }
    updateKeyerMemButtons();
    updateKeyerProgramButton();
  }
}

function exitKeyerSelectMode() {
  keyerSelectMode = false;
  if (elKeyerStatus) {
    elKeyerStatus.textContent = 'TX Ready';
    elKeyerStatus.className = 'keyer-status';
  }
  updateKeyerMemButtons();
  updateKeyerProgramButton();
}

function exitKeyerProgramMode() {
  keyerSelectMode = false;
  keyerProgramSlot = -1;
  if (elKeyerProgramArea) elKeyerProgramArea.classList.add('hidden');
  if (elKeyerStatus) {
    elKeyerStatus.textContent = 'TX Ready';
    elKeyerStatus.className = 'keyer-status';
  }
  updateKeyerMemButtons();
  updateKeyerProgramButton();
}

function setupKeyerLongPress() {
  setupKeyerKeyTypeLongPress(elBtnKeyerStraight, 'Straight');
  setupKeyerKeyTypeLongPress(elBtnKeyerIambic, 'Iambic');
}

function setupKeyerKeyTypeLongPress(btn, targetType) {
  if (!btn) return;

  btn.addEventListener('click', function (e) {
    e.preventDefault();
    if (keyType === targetType) return;

    wsSend({ type: 'set_key_type', key_type: targetType });
    keyType = targetType;
    updateKeyerKeyTypeButtons();
    updateEchoUi();
    logEvent('Key type: ' + targetType);

    if (elKeyerStatus) {
      elKeyerStatus.textContent = 'Key type: ' + targetType;
      elKeyerStatus.className = 'keyer-status';
    }
  });
}

function cancelKeyerKeyTypeLongPress() {
  if (elBtnKeyerStraight) elBtnKeyerStraight.classList.remove('confirming');
  if (elBtnKeyerIambic) elBtnKeyerIambic.classList.remove('confirming');
}

function enterKeyerProgramMode(slot) {
  keyerSelectMode = false;
  keyerProgramSlot = slot;
  if (elKeyerProgramLabel) {
    elKeyerProgramLabel.textContent = 'Program M' + (slot + 1) + ':';
  }
  if (elKeyerProgramText) {
    elKeyerProgramText.value = keyerMemories[slot];
    elKeyerProgramText.focus();
  }
  if (elKeyerProgramArea) {
    elKeyerProgramArea.classList.remove('hidden');
  }
  if (elKeyerStatus) {
    elKeyerStatus.textContent = 'Programming M' + (slot + 1) + '…';
    elKeyerStatus.className = 'keyer-status';
  }
  updateKeyerMemButtons();
  updateKeyerProgramButton();
}

function cmdKeyerMemSave() {
  if (keyerProgramSlot < 0 || keyerProgramSlot >= 6) return;
  var text = elKeyerProgramText ? elKeyerProgramText.value : '';
  // Strip newlines, carriage returns, and tabs before sending
  text = text.replace(/[\n\r\t]+/g, ' ').replace(/  +/g, ' ').trim();
  // Truncate to max memory size
  if (text.length > 1023) text = text.substring(0, 1023);
  keyerMemories[keyerProgramSlot] = text.toUpperCase();
  wsSend({ type: 'keyer_mem_set', slot: keyerProgramSlot, text: text });
  var slot = keyerProgramSlot;
  keyerProgramSlot = -1;
  if (elKeyerProgramArea) elKeyerProgramArea.classList.add('hidden');
  if (elKeyerStatus) {
    elKeyerStatus.textContent = text.length > 0
      ? 'M' + (slot + 1) + ' saved (' + text.length + ' chars)'
      : 'M' + (slot + 1) + ' cleared';
    elKeyerStatus.className = 'keyer-status';
  }
  updateKeyerMemButtons();
  updateKeyerProgramButton();
  logEvent('Keyer M' + (slot + 1) + ' saved: ' + (text.length > 0 ? text.substring(0, 40) : '(empty)'));
}

function cmdKeyerMemCancel() {
  keyerProgramSlot = -1;
  keyerSelectMode = false;
  if (elKeyerProgramArea) elKeyerProgramArea.classList.add('hidden');
  if (elKeyerStatus) {
    elKeyerStatus.textContent = 'TX Ready';
    elKeyerStatus.className = 'keyer-status';
  }
  updateKeyerMemButtons();
  updateKeyerProgramButton();
}

function cmdKeyerMemPlay(slot) {
  if (keyerSelectMode) {
    enterKeyerProgramMode(slot);
    return;
  }
  if (keyerProgramSlot >= 0) return;
  if (!keyerMemories[slot] || keyerMemories[slot].length === 0) {
    if (elKeyerStatus) {
      elKeyerStatus.textContent = 'M' + (slot + 1) + ' empty — tap ✏️ Program to set up';
      elKeyerStatus.className = 'keyer-status';
    }
    return;
  }

  wsSend({ type: 'keyer_mem_play', slot: slot });

  for (var i = 0; i < 6; i++) {
    if (elKeyerMemBtns[i]) {
      elKeyerMemBtns[i].classList.remove('playing');
    }
  }
  if (elKeyerMemBtns[slot]) {
    elKeyerMemBtns[slot].classList.add('playing');
  }
  if (elKeyerStatus) {
    elKeyerStatus.textContent = 'TX: M' + (slot + 1) + ' — ' +
      keyerMemories[slot].substring(0, 30) + (keyerMemories[slot].length > 30 ? '…' : '');
    elKeyerStatus.className = 'keyer-status active';
  }
  if (elKeyerStop) elKeyerStop.disabled = false;
}

function cmdKeyerMemStop() {
  wsSend({ type: 'keyer_mem_stop' });

  // Kill local Web Audio immediately — cancel all in-flight sequences
  cancelAllSequences();
  wordAudioPlaying = false;
  wordReadyPending = false;
  pendingSeq = null;

  for (var i = 0; i < 6; i++) {
    if (elKeyerMemBtns[i]) elKeyerMemBtns[i].classList.remove('playing');
  }
  if (elKeyerStatus) {
    elKeyerStatus.textContent = 'Stopped';
    elKeyerStatus.className = 'keyer-status';
  }
  if (elKeyerStop) elKeyerStop.disabled = true;
}

var wifiMode       = 'AP';
var wifiIP         = '192.168.4.1';

function updateWifiDisplay() {
  if (!elHeaderWifi) return;
  if (wifiMode === 'STA') {
    elHeaderWifi.textContent = '📡 STA · ' + wifiIP;
  } else {
    elHeaderWifi.textContent = '📶 AP · ' + wifiIP;
  }
}

// ── Keyer Sidetone (real-time key down/up) ──────────────────
var keyerToneOsc  = null;
var keyerToneGain = null;
var keyerKeepAliveOsc  = null;
var keyerKeepAliveGain = null;

function ensureKeyerToneNode() {
  if (!initAudio()) return false;
  if (keyerToneOsc) return true;
  try { audioCtx.resume(); } catch (e) { /* best effort */ }

  // Keep-alive: a silent oscillator that prevents the AudioContext
  // from suspending between characters.  Without this the browser
  // idles the audio thread after ~100-200ms of silence and the
  // first dit/dah of the next character suffers a resume penalty.
  if (!keyerKeepAliveOsc) {
    var kaOsc = audioCtx.createOscillator();
    kaOsc.type = 'sine';
    kaOsc.frequency.value = toneFreq;
    keyerKeepAliveGain = audioCtx.createGain();
    keyerKeepAliveGain.gain.value = 0; // completely silent
    kaOsc.connect(keyerKeepAliveGain);
    keyerKeepAliveGain.connect(audioCtx.destination);
    kaOsc.start();
    keyerKeepAliveOsc = kaOsc;
  }

  // The audible sidetone oscillator
  var osc = audioCtx.createOscillator();
  osc.type = 'sine';
  osc.frequency.value = toneFreq;
  keyerToneGain = audioCtx.createGain();
  keyerToneGain.gain.value = 0; // start silent
  osc.connect(keyerToneGain);
  keyerToneGain.connect(gainNode);
  osc.start();
  keyerToneOsc = osc;
  return true;
}

function startKeyerTone() {
  if (!ensureKeyerToneNode()) return;
  keyerToneGain.gain.cancelScheduledValues(audioCtx.currentTime);
  keyerToneGain.gain.setTargetAtTime(1, audioCtx.currentTime, 0.003);
  lastToneEndMs = Date.now();
}

function stopKeyerTone() {
  if (!keyerToneGain) return;
  keyerToneGain.gain.cancelScheduledValues(audioCtx.currentTime);
  keyerToneGain.gain.setTargetAtTime(0, audioCtx.currentTime, 0.003);
  lastToneEndMs = Date.now();
}

function destroyKeyerTone() {
  if (keyerToneOsc) {
    try { keyerToneOsc.stop(); } catch (e) { /* already stopped */ }
    keyerToneOsc = null;
  }
  keyerToneGain = null;
  if (keyerKeepAliveOsc) {
    try { keyerKeepAliveOsc.stop(); } catch (e) { /* already stopped */ }
    keyerKeepAliveOsc = null;
  }
  keyerKeepAliveGain = null;
}

function cancelAllSequences() {
  seqId++;
}

// Build a full audio sequence from a keyer pattern string.
// '.' / '-' = dit/dah + element gap, ' ' = char gap, '|' = word gap
// NOTE: The ESP32 playback engine adds ELEMENT_GAP then CHAR_GAP/WORD_GAP
// sequentially (not overlapping), so actual gaps are 4 dits between chars
// and 8 dits between words. We match that here to stay in sync.
function buildKeyerSeq(pattern, ditMs) {
  var seq = [];
  for (var i = 0; i < pattern.length; i++) {
    var el = pattern[i];
    if (el === '.' || el === '-') {
      seq.push({ tone: true,  ms: (el === '.') ? ditMs : ditMs * 3 });
      seq.push({ tone: false, ms: ditMs });
    } else if (el === ' ') {
      seq.push({ tone: false, ms: ditMs * 3 });   // +3 dit (total 4 with element gap = ESP32 CharGap)
    } else if (el === '|') {
      seq.push({ tone: false, ms: ditMs * 7 });   // +7 dit (total 8 with element gap = ESP32 WordGap)
    }
  }
  return seq;
}