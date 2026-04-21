#include "WiFiAP.h"

// WiFi AP configuration
const char* AP_SSID = "ESP32_Morse_Trainer";
const char* AP_PASSWORD = "morse1234";

// Track connected stations
static int previousStationCount = 0;
static unsigned long lastCheckTime = 0;
static const unsigned long CHECK_INTERVAL = 2000;
static bool apReady = false;  // Don't check clients until AP is ready

// LED blink state
static bool ledBlinkActive = false;
static unsigned long ledBlinkStartTime = 0;
static const unsigned long LED_BLINK_DURATION = 3000;

// Check if AP is ready
bool isAPReady() {
  return apReady;
}

// Get station count using ESP-IDF API
int getStationCount() {
  if (!apReady) {
    return 0;
  }
  
  // Zero-initialize to prevent stale/garbage data
  wifi_sta_list_t stationList = {};
  memset(&stationList, 0, sizeof(wifi_sta_list_t));
  
  esp_err_t err = esp_wifi_ap_get_sta_list(&stationList);
  if (err == ESP_OK) {
    return stationList.num;
  }
  return 0;
}

// Initialize WiFi in AP mode using ESP-IDF API
void initWiFiAP() {
  Serial.println("\n======================================");
  Serial.println("Initializing WiFi Access Point...");
  Serial.println("======================================");

  apReady = false;

  // Step 1: Initialize network interface (safe to call if already done)
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("✗ Netif init failed: %d\n", err);
    return;
  }

  // Step 2: Create event loop (safe to call if already done)
  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("✗ Event loop failed: %d\n", err);
    return;
  }

  // Step 3: Create AP network interface (only if not already created)
  esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (ap_netif == NULL) {
    ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
      Serial.println("✗ Failed to create AP netif!");
      return;
    }
  }

  // Step 4: Initialize WiFi driver (safe to call if already done)
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&cfg);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("✗ WiFi init failed: %d\n", err);
    return;
  }

  // Step 5: Set mode to AP
  err = esp_wifi_set_mode(WIFI_MODE_AP);
  if (err != ESP_OK) {
    Serial.printf("✗ Set mode failed: %d\n", err);
    return;
  }

  // Step 6: Configure AP settings
  wifi_config_t ap_config = {};
  memset(&ap_config, 0, sizeof(wifi_config_t));
  strlcpy((char*)ap_config.ap.ssid, AP_SSID, sizeof(ap_config.ap.ssid));
  strlcpy((char*)ap_config.ap.password, AP_PASSWORD, sizeof(ap_config.ap.password));
  ap_config.ap.ssid_len = strlen(AP_SSID);
  ap_config.ap.channel = 1;
  ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  ap_config.ap.max_connection = 4;
  ap_config.ap.beacon_interval = 100;

  err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  if (err != ESP_OK) {
    Serial.printf("✗ Set config failed: %d\n", err);
    return;
  }

  // Step 7: Start WiFi
  err = esp_wifi_start();
  if (err != ESP_OK) {
    Serial.printf("✗ WiFi start failed: %d\n", err);
    return;
  }

  // Let AP stabilize
  delay(500);

  // Get AP IP address
  esp_netif_ip_info_t ip_info;
  memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
  esp_netif_get_ip_info(ap_netif, &ip_info);

  // Get MAC address
  uint8_t mac[6] = {};
  esp_wifi_get_mac(WIFI_IF_AP, mac);

  Serial.println("✓ AP started successfully!");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Password: ");
  Serial.println(AP_PASSWORD);
  Serial.printf("IP Address: %d.%d.%d.%d\n", IP2STR(&ip_info.ip));
  Serial.printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println("--------------------------------------");
  Serial.println("Connect your phone/computer to this network");
  Serial.printf("Then ping %d.%d.%d.%d\n", IP2STR(&ip_info.ip));
  Serial.println("======================================\n");

  // Turn on LED to indicate AP is active
  digitalWrite(LED_PIN, HIGH);

  // Now it's safe to start checking for clients
  // Set lastCheckTime to now so first check happens after CHECK_INTERVAL
  lastCheckTime = millis();
  previousStationCount = 0;
  apReady = true;
}

// Check and report connected stations
void checkWiFiClients() {
  if (!apReady) {
    return;
  }

  unsigned long currentTime = millis();

  if (currentTime - lastCheckTime < CHECK_INTERVAL) {
    return;
  }

  lastCheckTime = currentTime;

  int stationCount = getStationCount();

  if (stationCount != previousStationCount) {
    if (stationCount > previousStationCount) {
      Serial.print("\n>>> Station connected! Total stations: ");
      Serial.println(stationCount);

      ledBlinkActive = true;
      ledBlinkStartTime = currentTime;
    } else {
      Serial.print("\n>>> Station disconnected. Total stations: ");
      Serial.println(stationCount);
    }

    previousStationCount = stationCount;
  }
}

// Blink LED when a station connects
void blinkLEDForConnection() {
  if (!ledBlinkActive) {
    return;
  }

  unsigned long currentTime = millis();

  if (currentTime - ledBlinkStartTime < LED_BLINK_DURATION) {
    if ((currentTime / 250) % 2 == 0) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  } else {
    ledBlinkActive = false;
    digitalWrite(LED_PIN, HIGH);
  }
}