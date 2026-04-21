#ifndef WIFIAP_H
#define WIFIAP_H

#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_netif.h"

// External GPIO pin declaration (defined in .ino)
extern const unsigned int LED_PIN;

// WiFi AP configuration
extern const char* AP_SSID;
extern const char* AP_PASSWORD;

// WiFi AP functions
void initWiFiAP();
void checkWiFiClients();
void blinkLEDForConnection();
int getStationCount();
bool isAPReady();

#endif