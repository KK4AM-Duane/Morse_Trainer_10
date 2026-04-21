#include "MorseWebServer.h"
#include "MorseWebSocket.h"
#include "WiFiAPMode.h"
#include "WiFiStation.h"
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
  json += "\"wifi_mode\":\"" + String(getWiFiOpModeName()) + "\",";
  json += "\"sta_connected\":" + String(isSTAReady() ? "true" : "false") + ",";

  String modeStr;
  if (currentMode == MorseMode::Koch) {
    modeStr = "Koch";
  } else {
    modeStr = "Progtable";
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
  json += ",\"chars:[";

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

// Accumulate POST body into /practice.txt
static String practiceUploadBuf;
static int    practiceUploadWritten = 0;

// Handle POST /api/practice — write body to /practice.txt in LittleFS
static void handleApiPracticePost(AsyncWebServerRequest *request) {
  if (practiceUploadWritten < 0) {
    request->send(500, "application/json", "{\"ok\":false,\"error\":\"write failed\"}");
  } else if (practiceUploadWritten == 0 && request->contentLength() > 0) {
    request->send(500, "application/json", "{\"ok\":false,\"error\":\"body not received\"}");
  } else {
    String json = "{\"ok\":true,\"bytes\":" + String(practiceUploadWritten) + "}";
    request->send(200, "application/json", json);
  }
}

static void handleApiPracticeBody(AsyncWebServerRequest *request,
                                  uint8_t *data, size_t len,
                                  size_t index, size_t total) {
  if (index == 0) {
    practiceUploadBuf = "";
    practiceUploadBuf.reserve(total);
    practiceUploadWritten = 0;
  }
  for (size_t i = 0; i < len; i++) {
    practiceUploadBuf += (char)data[i];
  }
  if (index + len >= total) {
    File f = LittleFS.open("/practice.txt", "w");
    if (f) {
      practiceUploadWritten = (int)f.print(practiceUploadBuf);
      f.close();
      Serial.printf(">>> Web: POST /api/practice — saved %d bytes\n",
                    practiceUploadWritten);
    } else {
      Serial.println(">>> Web: POST /api/practice — FAILED to open file for writing");
      practiceUploadWritten = -1;
    }
    practiceUploadBuf = "";
  }
}

// Initialize LittleFS, WebSocket, and async web server
void initWebServer() {
  Serial.println("\n======================================");
  Serial.println("Initializing Web Server...");
  Serial.println("======================================");

  // In AP mode, require AP to be ready before starting.
  // In STA mode, start the server immediately — it will accept
  // connections once WiFi connects (or reconnects later).
  // AsyncWebServer binds to 0.0.0.0 and works as soon as an
  // IP is assigned, even if that happens after server.begin().
  WiFiOpMode mode = getWiFiOpMode();
  if (mode == WiFiOpMode::AP && !isAPReady()) {
    Serial.println("✗ WiFi AP is not running!");
    Serial.println("  Web server requires an active WiFi interface.");
    Serial.println("======================================\n");
    return;
  }
  if (mode == WiFiOpMode::STA) {
    if (isSTAReady()) {
      Serial.printf("✓ WiFi STA connected — IP %s\n", getSTAIPAddress().c_str());
    } else {
      Serial.println("⏳ WiFi STA not yet connected — server will be ready when WiFi connects");
    }
  } else {
    Serial.printf("✓ WiFi %s is active\n", getWiFiOpModeName());
  }

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("✗ LittleFS mount failed!");
    Serial.println("  Make sure you've uploaded files using");
    Serial.println("  the LittleFS upload tool.");
    Serial.println("======================================\n");
    return;
  }
  Serial.println("✓ LittleFS mounted");

  // ── CRITICAL: LittleFS/SPI flash init may hijack GPIO23 (VSPI MOSI). ──
  // Reclaim it as a plain GPIO output and force LOW to prevent TX glitch.
  extern const unsigned int TX_KEY_PIN;
  pinMode(TX_KEY_PIN, OUTPUT);
  digitalWrite(TX_KEY_PIN, LOW);

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

  // Build the correct base URL for serial output
  String baseUrl;
  if (mode == WiFiOpMode::STA && isSTAReady()) {
    baseUrl = "http://" + getSTAIPAddress();
  } else if (mode == WiFiOpMode::STA) {
    baseUrl = "http://<pending-ip>";
  } else {
    baseUrl = "http://192.168.4.1";
  }

  Serial.println("✓ Async web server started on port 80");
  Serial.println("--------------------------------------");
  Serial.printf("  Open %s in your browser\n", baseUrl.c_str());
  Serial.printf("  API:  %s/api/status\n", baseUrl.c_str());
  Serial.printf("  API:  %s/api/koch_stats\n", baseUrl.c_str());
  Serial.printf("  API:  %s/api/practice (GET/POST)\n", baseUrl.c_str());
  Serial.printf("  WS:   ws://%s/ws\n",
                (mode == WiFiOpMode::STA && isSTAReady()) ? getSTAIPAddress().c_str() : "192.168.4.1");
  Serial.println("======================================\n");
}

// Called from loop() — async server doesn't need handleClient(),
// but we still need to prune dead WebSocket connections.
void handleWebServer() {
  if (serverRunning) {
    cleanupWebSocketClients();
  }
}