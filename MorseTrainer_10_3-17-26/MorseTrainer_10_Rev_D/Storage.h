#ifndef STORAGE_H
#define STORAGE_H

#include <Preferences.h>

// Initialize NVS storage (call once from setup, before anything loads state)
void initStorage();

// Shared Preferences handle (namespace "cwtrainer")
Preferences& getPrefs();

// True after initStorage() has been called
bool isStorageReady();

#endif