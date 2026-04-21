#include "MorseWebSocket.h"
#include "MorsePlayback.h"
#include "MorseLookup.h"
#include "MorseCommands.h"
#include "KochTrainer.h"
#include "WiFiAP.h"
#include "EchoTrainer.h"
#include "KeyInput.h"          // for getKeyTypeName / setKeyType / KeyType
#include "CWKeyerMode.h"       // for getOperatingModeName / isCWKeyerMode

// WebSocket endpoint lives at /ws
static AsyncWebSocket ws("/ws");

// ---------------- Internal helpers ----------------

// Build a JSON string for the active Koch characters.
// Returns e.g. ["K","M","U","R"] with prosign display names.
static String buildKochCharsJson() {
  int count = getKochActiveCount();
  String json = "[";
  for (int i = 0; i < count; i++) {
    if (i > 0) json += ',';
    char c = KOCH_ORDER_FULL[i];
    // Use display names for prosign symbols
    String display;
    if (c == '^')       display = "AR";
    else if (c == '[')  display = "BT";
    else if (c == '|')  display = "SK";
    else                display = String(c);
    json += "{\"ch\":\"";
    // Escape backslash and quote for valid JSON
    if (c == '\\')      json += "\\\\";
    else if (c == '"')  json += "\\\"";
    else                json += c;
    json += "\",\"display\":\"" + display + "\"}";
  }
  json += "]";
  return json;
}

// ---------------- Outbound: server → all clients ----------------

void wsBroadcastCharSent(char ch, const String &morse) {
  if (ws.count() == 0) return;

  String json = "{\"type\":\"char_sent\",\"char\":\"";
  if (ch == '\\')      json += "\\\\";
  else if (ch == '"')  json += "\\\"";
  else                 json += ch;
  json += "\",\"morse\":\"" + morse +
          "\",\"ditMs\":" + String(DOT_MS) +
          ",\"timestamp\":" + String(millis()) + "}";

  ws.textAll(json);
}

void wsBroadcastPlaybackStart(const String &message, unsigned int wpm) {
  if (ws.count() == 0) return;

  String json = "{\"type\":\"playback_start\",\"message\":\"" + message +
                "\",\"wpm\":" + String(wpm) + "}";
  ws.textAll(json);
}

void wsBroadcastPlaybackDone() {
  if (ws.count() == 0) return;
  ws.textAll("{\"type\":\"playback_done\"}");
}

void wsBroadcastSpeedChange(unsigned int wpm) {
  if (ws.count() == 0) return;

  String json = "{\"type\":\"speed_change\",\"wpm\":" + String(wpm) + "}";
  ws.textAll(json);
}

void wsBroadcastStatus() {
  if (ws.count() == 0) return;

  String modeStr;
  if (currentMode == MorseMode::Koch)             modeStr = "Koch";
  else if (currentMode == MorseMode::Progmem)     modeStr = "PROGMEM";
  else if (currentMode == MorseMode::Progtable)   modeStr = "PROGTABLE";
  else                                             modeStr = "Vector";

  String json = "{\"type\":\"status\""
                ",\"wpm\":" + String(WPM) +
                ",\"mode\":\"" + modeStr + "\""
                ",\"playing\":" + String(playback.state != PlaybackState::Idle ? "true" : "false") +
                ",\"koch_active\":" + String(isKochActive() ? "true" : "false") +
                ",\"koch_paused\":" + String(isKochPaused() ? "true" : "false") +
                ",\"koch_lesson\":" + String(getKochLesson()) +
                ",\"koch_chars\":" + buildKochCharsJson() +
                ",\"buzzer_enabled\":" + String(isBuzzerEnabled() ? "true" : "false") +
                ",\"stations\":" + String(getStationCount()) +
                ",\"uptime_ms\":" + String(millis()) +
                ",\"free_heap\":" + String(ESP.getFreeHeap()) +
                ",\"echo_active\":" + String(isEchoActive() ? "true" : "false") +
                ",\"echo_correct\":" + String(getEchoCorrect()) +
                ",\"echo_total\":" + String(getEchoTotal()) +
                ",\"echo_source\":\"" + String(getEchoCharSourceName()) + "\""
                ",\"key_type\":\"" + String(getKeyTypeName()) + "\""
                ",\"keyer_mode\":\"" + String(getOperatingModeName()) + "\""
                ",\"practice_active\":" + String(isPracticeActive() ? "true" : "false") +
                ",\"practice_sent\":" + String(getPracticeWordsSent()) +
                ",\"practice_total\":" + String(getPracticeTotalWords()) +
                "}";
  ws.textAll(json);
}

// --- Freeform (WORD) playback broadcasts ---

void wsBroadcastWordChar(char ch, const String &pattern, unsigned int ditMs) {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"word_char\",\"char\":\"";
  if (ch == '\\')      json += "\\\\";
  else if (ch == '"')  json += "\\\"";
  else                 json += ch;
  json += "\",\"pattern\":\"" + pattern +
          "\",\"ditMs\":" + String(ditMs) + "}";
  ws.textAll(json);
}

void wsBroadcastWordGo() {
  if (ws.count() == 0) return;
  ws.textAll("{\"type\":\"word_go\"}");
}

// --- Koch trainer broadcasts ---

void wsBroadcastKochChallenge(uint8_t lesson, uint16_t questionNum,
                               const String &pattern, unsigned int ditMs) {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"koch_challenge\""
                ",\"lesson\":" + String(lesson) +
                ",\"question\":" + String(questionNum) +
                ",\"pattern\":\"" + pattern + "\""
                ",\"ditMs\":" + String(ditMs) +
                ",\"timestamp\":" + String(millis()) + "}";
  ws.textAll(json);
}

void wsBroadcastKochGo() {
  if (ws.count() == 0) return;
  ws.textAll("{\"type\":\"koch_go\"}");
}

void wsBroadcastKochResult(char expected, char received, bool correct,
                           unsigned long reactionMs,
                           uint16_t sessionCorrect, uint16_t sessionTotal,
                           uint8_t lesson) {
  if (ws.count() == 0) return;

  String json = "{\"type\":\"koch_result\""
                ",\"expected\":\"";
  if (expected == '\\')      json += "\\\\";
  else if (expected == '"')  json += "\\\"";
  else                       json += expected;
  json += "\",\"received\":\"";
  if (received == '\\')      json += "\\\\";
  else if (received == '"')  json += "\\\"";
  else                       json += received;
  json += "\",\"correct\":" + String(correct ? "true" : "false") +
          ",\"reaction_ms\":" + String(reactionMs) +
          ",\"session_correct\":" + String(sessionCorrect) +
          ",\"session_total\":" + String(sessionTotal) +
          ",\"lesson\":" + String(lesson) +
          ",\"sort_grid\":" + String(isKochGridSorted() ? "true" : "false") +
          "}";
  ws.textAll(json);
}

void wsBroadcastKochSession(bool started, uint8_t lesson, unsigned int wpm) {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"koch_session\""
                ",\"started\":" + String(started ? "true" : "false") +
                ",\"lesson\":" + String(lesson) +
                ",\"wpm\":" + String(wpm) + "}";
  ws.textAll(json);
}

void wsBroadcastKochEvent(const String &text) {
  if (ws.count() == 0) return;
  String escaped = text;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  String json = "{\"type\":\"koch_event\",\"message\":\"" + escaped + "\"}";
  ws.textAll(json);
}

void wsBroadcastKochChars() {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"koch_chars\""
                ",\"lesson\":" + String(getKochLesson()) +
                ",\"chars\":" + buildKochCharsJson() + "}";
  ws.textAll(json);
}

// --- Echo trainer broadcasts ---

void wsBroadcastEchoSession(bool started, uint16_t correct, uint16_t total) {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"echo_session\""
                ",\"started\":" + String(started ? "true" : "false") +
                ",\"correct\":" + String(correct) +
                ",\"total\":" + String(total) +
                ",\"echo_source\":\"" + String(getEchoCharSourceName()) + "\""
                ",\"key_type\":\"" + String(getKeyTypeName()) + "\""
                "}";
  ws.textAll(json);
}

void wsBroadcastEchoPrompt(char expected, const String &pattern, unsigned int ditMs) {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"echo_prompt\",\"char\":\"";
  if (expected == '\\') json += "\\\\";
  else if (expected == '"') json += "\\\"";
  else json += expected;
  json += "\",\"pattern\":\"" + pattern +
          "\",\"ditMs\":" + String(ditMs) + "}";
  ws.textAll(json);
}

void wsBroadcastEchoResult(char expected, char received, bool correct,
                           uint16_t correctCount, uint16_t totalCount) {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"echo_result\""
                ",\"expected\":\"";
  if (expected == '\\') json += "\\\\";
  else if (expected == '"') json += "\\\"";
  else json += expected;
  json += "\",\"received\":\"";
  if (received == '\\') json += "\\\\";
  else if (received == '"') json += "\\\"";
  else json += received;
  json += "\",\"correct\":" + String(correct ? "true" : "false") +
          ",\"session_correct\":" + String(correctCount) +
          ",\"session_total\":" + String(totalCount) + "}";
  ws.textAll(json);
}

void wsBroadcastEchoConfig() {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"echo_config\""
                ",\"echo_source\":\"" + String(getEchoCharSourceName()) + "\""
                ",\"key_type\":\"" + String(getKeyTypeName()) + "\""
                "}";
  ws.textAll(json);
}

void wsBroadcastKeyConfig() {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"key_config\""
                ",\"key_type\":\"" + String(getKeyTypeName()) + "\""
                "}";
  ws.textAll(json);
}

// ---------------- Inbound: client → server ----------------

// Simple JSON key extraction (avoids pulling in ArduinoJson for a few fields)
static String jsonGetString(const String &json, const String &key) {
  String search = "\"" + key + "\":\"";
  int start = json.indexOf(search);
  if (start < 0) return "";
  start += search.length();
  int end = json.indexOf('"', start);
  if (end < 0) return "";
  return json.substring(start, end);
}

static int jsonGetInt(const String &json, const String &key) {
  String search = "\"" + key + "\":";
  int start = json.indexOf(search);
  if (start < 0) return -1;
  start += search.length();
  // Skip whitespace
  while (start < (int)json.length() && json[start] == ' ') start++;
  int end = start;
  while (end < (int)json.length() && (isDigit(json[end]) || json[end] == '-')) end++;
  return json.substring(start, end).toInt();
}

static void handleIncomingMessage(AsyncWebSocketClient *client, const String &msg) {
  String type = jsonGetString(msg, "type");

  // Fast-path handshake ACKs
  if (type == "word_ready") {
    wordPhoneReady();
    return;
  }
  if (type == "koch_ready") {
    kochPhoneReady();
    return;
  }

  Serial.printf(">>> WS[%u]: %s\n", client->id(), msg.c_str());

  // --- Koch stop: highest priority, works unconditionally ---
  if (type == "koch_stop") {
    if (isKochActive()) {
      stopKochSession();
    }
    return;
  }

  if (type == "send") {
    String message = jsonGetString(msg, "message");
    message.toUpperCase();
    if (message.length() > 0 && playback.state == PlaybackState::Idle && !isKochActive()) {
      startTransmission(message);
    }
  }
  else if (type == "ping") {
    String json = "{\"type\":\"pong\",\"uptime_ms\":" + String(millis()) + "}";
    client->text(json);
    Serial.println(">>> Pong sent");
  }
  else if (type == "chat") {
    String message = jsonGetString(msg, "message");
    if (message.length() > 0) {
      Serial.printf(">>> Chat from WS[%u]: %s\n", client->id(), message.c_str());
      message.replace("\\", "\\\\");
      message.replace("\"", "\\\"");
      String json = "{\"type\":\"chat_echo\",\"message\":\"" + message +
                    "\",\"from\":" + String(client->id()) +
                    ",\"timestamp\":" + String(millis()) + "}";
      ws.textAll(json);
    }
  }
  else if (type == "koch_answer") {
    String ch = jsonGetString(msg, "char");
    if (ch.length() > 0 && isKochActive()) {
      kochSubmitAnswer(ch[0]);
    }
  }
  else if (type == "set_wpm") {
    int newWPM = jsonGetInt(msg, "wpm");
    if (newWPM >= 1 && newWPM <= 60) {
      setWPM(newWPM);
      wsBroadcastSpeedChange(WPM);
    }
  }
  else if (type == "set_buzzer") {
    String val = jsonGetString(msg, "enabled");
    bool en = (val == "true" || val == "1");
    setBuzzerEnabled(en);
    saveBuzzerToNVS();
    wsBroadcastBuzzerState(isBuzzerEnabled());
  }
  else if (type == "koch_pause") {
    if (isKochActive() && !isKochPaused()) {
      pauseKochSession();
    }
  }
  else if (type == "koch_resume") {
    if (isKochPaused()) {
      resumeKochSession();
    }
  }
  else if (type == "koch_reset") {
    resetKochProgress();
  }
  else if (type == "echo_start") {
    startEchoTraining();
  }
  else if (type == "echo_stop") {
    stopEchoTraining();
  }
  else if (type == "echo_stat") {
    wsBroadcastEchoSession(isEchoActive(), getEchoCorrect(), getEchoTotal());
  }
  else if (type == "set_echo_source") {
    String src = jsonGetString(msg, "source");
    if (isEchoActive()) {
      Serial.println(">>> Cannot change echo source while training is active");
    } else if (src == "koch" || src == "Koch") {
      setEchoCharSource(EchoCharSource::Koch);
    } else if (src == "full" || src == "Full") {
      setEchoCharSource(EchoCharSource::Full);
    }
  }
  else if (type == "set_key_type") {
    String kt = jsonGetString(msg, "key_type");
    if (isEchoActive()) {
      Serial.println(">>> Cannot change key type while echo training is active");
    } else if (kt == "straight" || kt == "Straight") {
      setKeyType(KeyType::Straight);
      wsBroadcastKeyConfig();
    } else if (kt == "iambic" || kt == "Iambic") {
      setKeyType(KeyType::Iambic);
      wsBroadcastKeyConfig();
    }
  }
  else if (type == "get_status") {
    String modeStr;
    if (currentMode == MorseMode::Koch)             modeStr = "Koch";
    else if (currentMode == MorseMode::Progmem)     modeStr = "PROGMEM";
    else if (currentMode == MorseMode::Progtable)   modeStr = "PROGTABLE";
    else                                             modeStr = "Vector";

    String json = "{\"type\":\"status\""
                  ",\"wpm\":" + String(WPM) +
                  ",\"mode\":\"" + modeStr + "\""
                  ",\"playing\":" + String(playback.state != PlaybackState::Idle ? "true" : "false") +
                  ",\"koch_active\":" + String(isKochActive() ? "true" : "false") +
                  ",\"koch_paused\":" + String(isKochPaused() ? "true" : "false") +
                  ",\"koch_lesson\":" + String(getKochLesson()) +
                  ",\"koch_chars\":" + buildKochCharsJson() +
                  ",\"buzzer_enabled\":" + String(isBuzzerEnabled() ? "true" : "false") +
                  ",\"stations\":" + String(getStationCount()) +
                  ",\"uptime_ms\":" + String(millis()) +
                  ",\"free_heap\":" + String(ESP.getFreeHeap()) +
                  ",\"echo_active\":" + String(isEchoActive() ? "true" : "false") +
                  ",\"echo_correct\":" + String(getEchoCorrect()) +
                  ",\"echo_total\":" + String(getEchoTotal()) +
                  ",\"echo_source\":\"" + String(getEchoCharSourceName()) + "\""
                  ",\"key_type\":\"" + String(getKeyTypeName()) + "\""
                  ",\"keyer_mode\":\"" + String(getOperatingModeName()) + "\""
                  ",\"practice_active\":" + String(isPracticeActive() ? "true" : "false") +
                  ",\"practice_sent\":" + String(getPracticeWordsSent()) +
                  ",\"practice_total\":" + String(getPracticeTotalWords()) +
                  "}";
    client->text(json);
  }
  else if (type == "response") {
    String ch = jsonGetString(msg, "char");
    int reactionMs = jsonGetInt(msg, "reaction_ms");
    Serial.printf(">>> WS training response: '%s' in %d ms\n", ch.c_str(), reactionMs);
  }
  else if (type == "command") {
    String cmd = jsonGetString(msg, "cmd");
    if (cmd.length() > 0) {
      processCommand(cmd);
    }
  }
  else if (type == "practice_text") {
    String text = jsonGetString(msg, "text");
    if (text.length() > 0) {
      startPractice(text);
    }
  }
  else if (type == "practice_file") {
    startPracticeFromFile();
  }
  else if (type == "practice_stop") {
    stopPractice();
  }
  else if (type == "keyer_mem_set") {
    int slot = jsonGetInt(msg, "slot");
    String text = jsonGetString(msg, "text");
    if (slot >= 0 && slot < KEYER_MEM_COUNT) {
      setKeyerMemory(slot, text);
      wsBroadcastKeyerMemories();
    }
  }
  else if (type == "keyer_mem_play") {
    int slot = jsonGetInt(msg, "slot");
    if (slot >= 0 && slot < KEYER_MEM_COUNT) {
      playKeyerMemory(slot);
    }
  }
  else if (type == "keyer_mem_stop") {
    stopKeyerMemoryPlayback();
  }
  else if (type == "keyer_mem_get") {
    // Send all memories to the requesting client
    String json = "{\"type\":\"keyer_memories\",\"memories\":[";
    for (int i = 0; i < KEYER_MEM_COUNT; i++) {
      if (i > 0) json += ',';
      String mem = getKeyerMemory(i);
      mem.replace("\\", "\\\\");
      mem.replace("\"", "\\\"");
      json += "\"" + mem + "\"";
    }
    json += "]}";
    client->text(json);
  }
  else {
    Serial.printf(">>> WS[%u]: unknown type '%s'\n", client->id(), type.c_str());
  }
}

// ---------------- WebSocket event dispatcher ----------------

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf(">>> WS[%u] connected from %s\n", client->id(),
                     client->remoteIP().toString().c_str());
      {
        String welcome = "{\"type\":\"connected\",\"wpm\":" + String(WPM) +
                          ",\"koch_active\":" + String(isKochActive() ? "true" : "false") +
                          ",\"koch_paused\":" + String(isKochPaused() ? "true" : "false") +
                          ",\"koch_lesson\":" + String(getKochLesson()) +
                          ",\"koch_chars\":" + buildKochCharsJson() +
                          ",\"buzzer_enabled\":" + String(isBuzzerEnabled() ? "true" : "false") +
                          ",\"echo_active\":" + String(isEchoActive() ? "true" : "false") +
                          ",\"echo_correct\":" + String(getEchoCorrect()) +
                          ",\"echo_total\":" + String(getEchoTotal()) +
                          ",\"echo_source\":\"" + String(getEchoCharSourceName()) + "\""
                          ",\"key_type\":\"" + String(getKeyTypeName()) + "\""
                          ",\"keyer_mode\":\"" + String(getOperatingModeName()) + "\""
                          ",\"msg\":\"CW Trainer WebSocket ready\"}";
        client->text(welcome);
      }
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf(">>> WS[%u] disconnected\n", client->id());
      break;

    case WS_EVT_DATA:
      {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
          String msg;
          msg.reserve(len);
          for (size_t i = 0; i < len; i++) {
            msg += (char)data[i];
          }
          handleIncomingMessage(client, msg);
        }
      }
      break;

    case WS_EVT_PONG:
      break;

    case WS_EVT_ERROR:
      Serial.printf(">>> WS[%u] error(%u): %s\n", client->id(),
                     *((uint16_t *)arg), (char *)data);
      break;
  }
}

// ---------------- Public init / cleanup ----------------

void initWebSocket(AsyncWebServer &server) {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  Serial.println("✓ WebSocket handler registered at /ws");
}

void cleanupWebSocketClients() {
  ws.cleanupClients();
}

void wsBroadcastBuzzerState(bool enabled) {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"buzzer_state\",\"enabled\":";
  json += enabled ? "true" : "false";
  json += "}";
  ws.textAll(json);
}

void wsBroadcastKochPause(bool paused) {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"koch_pause\",\"paused\":";
  json += paused ? "true" : "false";
  json += "}";
  ws.textAll(json);
}

void wsBroadcastKeyerMode() {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"keyer_mode\""
                ",\"keyer_mode\":\"" + String(getOperatingModeName()) + "\""
                "}";
  ws.textAll(json);
}

void wsBroadcastPracticeSession(bool started, uint16_t wordsSent, uint16_t totalWords) {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"practice_session\""
                ",\"started\":" + String(started ? "true" : "false") +
                ",\"words_sent\":" + String(wordsSent) +
                ",\"total_words\":" + String(totalWords) +
                ",\"wpm\":" + String(WPM) +
                "}";
  ws.textAll(json);
}

void wsBroadcastPracticeWord(const String &word, uint16_t wordNum, uint16_t totalWords) {
  if (ws.count() == 0) return;
  String escaped = word;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  String json = "{\"type\":\"practice_word\""
                ",\"word\":\"" + escaped + "\""
                ",\"word_num\":" + String(wordNum) +
                ",\"total_words\":" + String(totalWords) +
                "}";
  ws.textAll(json);
}

void wsBroadcastKeyerMemories() {
  if (ws.count() == 0) return;
  String json = "{\"type\":\"keyer_memories\",\"memories\":[";
  for (int i = 0; i < KEYER_MEM_COUNT; i++) {
    if (i > 0) json += ',';
    String mem = getKeyerMemory(i);
    mem.replace("\\", "\\\\");
    mem.replace("\"", "\\\"");
    json += "\"" + mem + "\"";
  }
  json += "]}";
  ws.textAll(json);
}