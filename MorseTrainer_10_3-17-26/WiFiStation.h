#ifndef WIFISTATION_H
#define WIFISTATION_H

#include <Arduino.h>

// Initialize WiFi in station mode using WiFiManager captive portal.
// Blocks during portal (up to timeout), then returns.
void initWiFiSTA();

// Poll STA connection health — call from loop().
// Auto-reconnects with backoff if the hotspot drops.
// Drives LED: steady = connected, slow blink = reconnecting.
void checkWiFiSTA();

// True after successful STA connection
bool isSTAReady();

// Station IP address as string (valid only when connected)
String getSTAIPAddress();

// Currently connected SSID (from WiFi stack, not manually stored)
String getSTASSID();

// Current RSSI in dBm (valid only when connected)
int getSTARSSI();

// Clear saved WiFi credentials (reboot to open config portal)
void resetSTACredentials();

#endif