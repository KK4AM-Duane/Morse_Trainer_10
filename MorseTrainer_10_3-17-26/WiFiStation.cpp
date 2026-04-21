#include "WiFiStation.h"
#include "WiFiAPMode.h"       // for LED_PIN extern
#include "esp_wifi.h"
#include <ESPAsyncWebServer.h> // must come before WiFiManager to avoid HTTP_ANY macro conflict
#include <WiFi.h>
#include <WiFiManager.h>

// ── Portal configuration ───────────────────────────────────────────
static const char* PORTAL_AP_NAME     = "CW_Trainer_Setup";
static const char* PORTAL_AP_PASSWORD = "morse1234";   // min 8 chars
static const int   PORTAL_TIMEOUT_SEC = 180;            // 3 min, then retry in background
static const int   CONNECT_TIMEOUT_SEC = 15;            // per-attempt timeout

// ── RSSI thresholds ────────────────────────────────────────────────
static const int RSSI_WEAK_THRESHOLD  = -75;  // warn user
static const int RSSI_UNUSABLE_THRESHOLD = -85;  // connection will be unreliable

// ── Runtime state ──────────────────────────────────────────────────
static WiFiManager wm;
static bool staReady = false;
static String staIPAddress = "";

// ── Reconnect state ────────────────────────────────────────────────
static const unsigned long RECONNECT_CHECK_INTERVAL_MS = 3000;
static const unsigned long RECONNECT_BACKOFF_INIT_MS   = 2000;
static const unsigned long RECONNECT_BACKOFF_MAX_MS    = 30000;
static const unsigned long RECONNECT_WIFI_BEGIN_WAIT_MS = 5000;  // wait for WiFi.begin() to associate
static unsigned long lastReconnectCheckMs   = 0;
static unsigned long reconnectBackoffMs     = RECONNECT_BACKOFF_INIT_MS;
static unsigned long lastReconnectAttemptMs = 0;
static bool          reconnecting           = false;

// ── RSSI logging helper ────────────────────────────────────────────
static void printRSSIWarning(int rssi) {
  if (rssi <= RSSI_UNUSABLE_THRESHOLD) {
    Serial.printf("  ⚠ RSSI %d dBm — VERY WEAK, connection will be unreliable!\n", rssi);
    Serial.println("  ⚠ Move closer to the access point or use AP mode instead.");
  } else if (rssi <= RSSI_WEAK_THRESHOLD) {
    Serial.printf("  ⚠ RSSI %d dBm — weak signal, may drop occasionally\n", rssi);
  }
}

// ── Public accessors ───────────────────────────────────────────────

bool isSTAReady() {
  return staReady;
}

String getSTAIPAddress() {
  return staIPAddress;
}

String getSTASSID() {
  return WiFi.SSID();
}

int getSTARSSI() {
  return WiFi.RSSI();
}

void resetSTACredentials() {
  wm.resetSettings();
  Serial.println("WiFi credentials cleared.");
  Serial.println("  Reboot with switch in STA position to open config portal.\n");
}

// ── Init (called once from setup) ──────────────────────────────────

void initWiFiSTA() {
  Serial.println("\n======================================");
  Serial.println("Initializing WiFi Station Mode...");
  Serial.println("  Using WiFiManager captive portal");
  Serial.println("======================================");

  staReady = false;
  staIPAddress = "";
  reconnecting = false;
  reconnectBackoffMs = RECONNECT_BACKOFF_INIT_MS;

  // ── CRITICAL: reset the WiFi driver to a clean STA state ────────
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false);  // clear stale connection, keep saved credentials
  delay(100);              // let the driver settle

  Serial.println("  WiFi driver reset to STA mode");

  // WiFiManager configuration
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_SEC);
  wm.setConnectTimeout(CONNECT_TIMEOUT_SEC);
  wm.setWiFiAutoReconnect(false);  // we handle reconnect manually

  // Provide LED feedback while portal is active
  wm.setAPCallback([](WiFiManager *mgr) {
    Serial.println(">>> Config portal opened — connect to AP to configure WiFi");
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
  });

  Serial.printf("  Portal AP: \"%s\" / \"%s\"\n", PORTAL_AP_NAME, PORTAL_AP_PASSWORD);
  Serial.println("  If no saved network, connect to that AP to configure.");
  Serial.println("  Waiting for connection...");

  bool connected = wm.autoConnect(PORTAL_AP_NAME, PORTAL_AP_PASSWORD);

  if (connected) {
    staIPAddress = WiFi.localIP().toString();

    uint8_t mac[6] = {};
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    Serial.println();
    Serial.println("✓ Connected to hotspot!");
    Serial.printf("  SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("  IP Address: %s\n", staIPAddress.c_str());
    Serial.printf("  Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printRSSIWarning(WiFi.RSSI());
    Serial.println("--------------------------------------");
    Serial.printf("  Open http://%s in your browser\n", staIPAddress.c_str());
    Serial.println("======================================\n");

    digitalWrite(LED_PIN, HIGH);
    staReady = true;
  } else {
    Serial.println("\n✗ WiFiManager: portal timed out — not connected.");
    Serial.println("  Will keep retrying saved credentials in background.");
    Serial.println("  Reboot to reopen the config portal.");
    Serial.println("======================================\n");

    // Ensure WiFi is in STA mode for background reconnection.
    WiFi.mode(WIFI_STA);

    reconnecting = true;
    lastReconnectAttemptMs = millis();
  }
}

// ── Loop poller: reconnect + LED feedback ──────────────────────────

void checkWiFiSTA() {
  unsigned long now = millis();

  // Throttle checks
  if (now - lastReconnectCheckMs < RECONNECT_CHECK_INTERVAL_MS) {
    if (reconnecting) {
      // Slow blink: 500ms on/off while reconnecting
      digitalWrite(LED_PIN, ((now / 500) % 2 == 0) ? HIGH : LOW);
    }
    return;
  }
  lastReconnectCheckMs = now;

  bool connected = (WiFi.status() == WL_CONNECTED);

  // ── Was connected, now lost ──
  if (staReady && !connected) {
    staReady = false;
    staIPAddress = "";
    reconnecting = true;
    reconnectBackoffMs = RECONNECT_BACKOFF_INIT_MS;
    lastReconnectAttemptMs = now;

    Serial.println("\n>>> STA: connection lost — will auto-reconnect");
  }

  // ── Reconnecting: try again after backoff ──
  if (reconnecting && !connected) {
    if (now - lastReconnectAttemptMs >= reconnectBackoffMs) {
      Serial.printf(">>> STA: reconnecting (backoff %lums)...\n", reconnectBackoffMs);

      WiFi.disconnect(false);   // don't erase saved credentials
      delay(100);               // let the driver fully disconnect
      WiFi.begin();             // re-reads saved credentials from NVS

      lastReconnectAttemptMs = now;

      if (reconnectBackoffMs < RECONNECT_BACKOFF_MAX_MS) {
        reconnectBackoffMs = reconnectBackoffMs * 3 / 2;
        if (reconnectBackoffMs > RECONNECT_BACKOFF_MAX_MS) {
          reconnectBackoffMs = RECONNECT_BACKOFF_MAX_MS;
        }
      }
    }

    // Slow blink while reconnecting
    digitalWrite(LED_PIN, ((now / 500) % 2 == 0) ? HIGH : LOW);
    return;
  }

  // ── Just reconnected ──
  if (reconnecting && connected) {
    staIPAddress = WiFi.localIP().toString();
    staReady = true;
    reconnecting = false;
    reconnectBackoffMs = RECONNECT_BACKOFF_INIT_MS;

    int rssi = WiFi.RSSI();
    Serial.printf("\n>>> STA: reconnected! IP %s  RSSI %d dBm\n",
                  staIPAddress.c_str(), rssi);
    Serial.printf(">>> Open http://%s in your browser\n", staIPAddress.c_str());
    printRSSIWarning(rssi);

    // Steady LED
    digitalWrite(LED_PIN, HIGH);
  }
}