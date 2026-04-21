#ifndef MORSEWEBSERVER_H
#define MORSEWEBSERVER_H

#include <Arduino.h>

// Web server functions
void initWebServer();
void handleWebServer();   // still called from loop() — now only cleans up WS clients
bool isWebServerRunning();

#endif