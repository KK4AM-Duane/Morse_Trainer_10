#ifndef WIFIAPMODE_H
#define WIFIAPMODE_H

#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_netif.h"

// External GPIO pin declaration (defined in .ino)
extern const unsigned int LED_PIN;

// WiFi AP configuration
extern const char* AP_SSID;
extern const char* AP_PASSWORD;

// ── WiFi operating mode (AP vs STA) ───────────────────────────────
// Read once at boot from GPIO19; determines which WiFi init runs.
enum class WiFiOpMode : uint8_t {
  AP,   // ESP32 creates its own network (default / current behavior)
  STA   // ESP32 joins phone hotspot for internet access
};

void        setWiFiOpMode(WiFiOpMode mode);
WiFiOpMode  getWiFiOpMode();
const char* getWiFiOpModeName();   // returns "AP" or "STA"

// WiFi AP functions
void initWiFiAP();
void checkWiFiClients();
void blinkLEDForConnection();
int getStationCount();
bool isAPReady();

#endif