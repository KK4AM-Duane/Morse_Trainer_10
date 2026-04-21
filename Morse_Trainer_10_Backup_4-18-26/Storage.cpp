#include "Storage.h"

static Preferences prefs;
static bool ready = false;

void initStorage() {
  prefs.begin("cwtrainer", false);   // read-write
  ready = true;
  Serial.println("✓ NVS storage initialized");
}

Preferences& getPrefs() {
  return prefs;
}

bool isStorageReady() {
  return ready;
}