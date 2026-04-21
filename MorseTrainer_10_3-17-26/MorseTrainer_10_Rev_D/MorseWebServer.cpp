#include "MorseWebServer.h"
#include "MorseWebSocket.h"
#include "WiFiAP.h"
#include "MorseLookup.h"
#include "MorsePlayback.h"
#include "KochTrainer.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// Async web server on port 80 (replaces synchronous WebServer)
static AsyncWebServer server(80);
static bool serverRunning = false;

bool isWebServerRunning() {
  return serverRunning;
}

// Serve a file from LittleFS with correct content type
static String getContentType(const String &path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".png"))  return "image/png";
  if (path.endsWith(".ico"))  return "image/x-icon";
  return "text/plain";
}

// Handle /api/status endpoint
static void handleApiStatus(AsyncWebServerRequest *request) {
  String json = "{";
  json += "\"uptime_ms\":" + String(millis()) + ",";
  json += "\"uptime_s\":" + String(millis() / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"min_free_heap\":" + String(ESP.getMinFreeHeap()) + ",";
  json += "\"stations\":" + String(getStationCount()) + ",";
  json += "\"wpm\":" + String(WPM) + ",";

  String modeStr;
  if (currentMode == MorseMode::Progmem) {
    modeStr = "PROGMEM";
  } else if (currentMode == MorseMode::Progtable) {
    modeStr = "PROGTABLE";
  } else {
    modeStr = "Vector";
  }
  json += "\"mode\":\"" + modeStr + "\",";
  json += "\"playing\":";
  json += (playback.state != PlaybackState::Idle) ? "true" : "false";
  json += "}";

  request->send(200, "application/json", json);
  Serial.println(">>> Web: /api/status requested");
}

// Prosign display name for JSON output
static String prosignDisplay(char c) {
  switch (c) {
    case '^': return "AR";
    case '[': return "BT";
    case '|': return "SK";
    default:  return String(c);
  }
}

// Handle /api/koch_stats endpoint — returns per-character stats from the server
static void handleApiKochStats(AsyncWebServerRequest *request) {
  const CharStats *stats = getKochCharStats();
  int activeCount        = getKochActiveCount();
  uint8_t lesson         = getKochLesson();
  uint16_t sessCorrect   = getKochSessionCorrect();
  uint16_t sessTotal     = getKochSessionTotal();

  // Build JSON response
  // Pre-reserve a reasonable size: ~80 bytes per char entry + envelope
  String json;
  json.reserve(120 + activeCount * 80);

  json += "{\"lesson\":";
  json += String(lesson);
  json += ",\"session_correct\":";
  json += String(sessCorrect);
  json += ",\"session_total\":";
  json += String(sessTotal);
  json += ",\"koch_active\":";
  json += isKochActive() ? "true" : "false";
  json += ",\"wpm\":";
  json += String(WPM);
  json += ",\"chars\":[";

  for (int i = 0; i < activeCount; i++) {
    if (i > 0) json += ',';
    json += "{\"ch\":\"";
    char c = stats[i].character;
    // Escape backslash and quote for valid JSON
    if (c == '\\')      json += "\\\\";
    else if (c == '"')  json += "\\\"";
    else                json += c;
    json += "\",\"display\":\"";
    json += prosignDisplay(c);
    json += "\",\"sent\":";
    json += String(stats[i].totalSent);
    json += ",\"correct\":";
    json += String(stats[i].totalCorrect);
    json += ",\"avg_ms\":";
    json += String(stats[i].avgReactionMs);
    json += ",\"prob\":";
    json += String(stats[i].probability);
    json += '}';
  }

  json += "]}";

  request->send(200, "application/json", json);
  Serial.println(">>> Web: /api/koch_stats requested");
}

// Handle GET /api/practice — read /practice.txt from LittleFS
static void handleApiPracticeGet(AsyncWebServerRequest *request) {
  if (!LittleFS.exists("/practice.txt")) {
    request->send(200, "text/plain", "");
    return;
  }
  File f = LittleFS.open("/practice.txt", "r");
  if (!f) {
    request->send(500, "text/plain", "Failed to open file");
    return;
  }
  String text = f.readString();
  f.close();
  request->send(200, "text/plain", text);
  Serial.printf(">>> Web: GET /api/practice (%d bytes)\n", (int)text.length());
}

// Handle POST /api/practice — write body to /practice.txt in LittleFS
static void handleApiPracticePost(AsyncWebServerRequest *request) {
  // Body is collected by handleApiPracticeBody below
  request->send(200, "application/json", "{\"ok\":true}");
}

// Accumulate POST body into /practice.txt
static String practiceUploadBuf;

static void handleApiPracticeBody(AsyncWebServerRequest *request,
                                  uint8_t *data, size_t len,
                                  size_t index, size_t total) {
  if (index == 0) {
    practiceUploadBuf = "";
    practiceUploadBuf.reserve(total);
  }
  for (size_t i = 0; i < len; i++) {
    practiceUploadBuf += (char)data[i];
  }
  if (index + len >= total) {
    // All data received — write to LittleFS
    File f = LittleFS.open("/practice.txt", "w");
    if (f) {
      f.print(practiceUploadBuf);
      f.close();
      Serial.printf(">>> Web: POST /api/practice — saved %d bytes\n",
                    (int)practiceUploadBuf.length());
    } else {
      Serial.println(">>> Web: POST /api/practice — FAILED to open file for writing");
    }
    practiceUploadBuf = "";
  }
}

// Initialize LittleFS, WebSocket, and async web server
void initWebServer() {
  Serial.println("\n======================================");
  Serial.println("Initializing Web Server...");
  Serial.println("======================================");

  // Check that WiFi AP is running first
  if (!isAPReady()) {
    Serial.println("✗ WiFi AP is not running!");
    Serial.println("  Web server requires WiFi AP to be active.");
    Serial.println("======================================\n");
    return;
  }
  Serial.println("✓ WiFi AP is active");

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("✗ LittleFS mount failed!");
    Serial.println("  Make sure you've uploaded files using");
    Serial.println("  the LittleFS upload tool.");
    Serial.println("======================================\n");
    return;
  }
  Serial.println("✓ LittleFS mounted");

  // List files in LittleFS for debugging
  Serial.println("  Files in LittleFS:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  if (!file) {
    Serial.println("    (no files found - upload with LittleFS tool)");
  }
  while (file) {
    Serial.printf("    /%s (%d bytes)\n", file.name(), file.size());
    file = root.openNextFile();
  }

  // --- WebSocket (must be registered before server.begin()) ---
  initWebSocket(server);

  // --- REST API ---
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/koch_stats", HTTP_GET, handleApiKochStats);
  server.on("/api/practice", HTTP_GET, handleApiPracticeGet);
  server.on("/api/practice", HTTP_POST, handleApiPracticePost, NULL, handleApiPracticeBody);

  // --- Static files from LittleFS (catch-all) ---
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // --- 404 handler ---
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "404: Not Found");
  });

  // Start the server
  server.begin();
  serverRunning = true;

  Serial.println("✓ Async web server started on port 80");
  Serial.println("--------------------------------------");
  Serial.println("Open http://192.168.4.1 in your browser");
  Serial.println("API:  http://192.168.4.1/api/status");
  Serial.println("API:  http://192.168.4.1/api/koch_stats");
  Serial.println("API:  http://192.168.4.1/api/practice (GET/POST)");
  Serial.println("WS:   ws://192.168.4.1/ws");
  Serial.println("======================================\n");
}

// Called from loop() — async server doesn't need handleClient(),
// but we still need to prune dead WebSocket connections.
void handleWebServer() {
  if (serverRunning) {
    cleanupWebSocketClients();
  }
}