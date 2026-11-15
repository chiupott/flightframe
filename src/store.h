#pragma once
#include "config.h"
void storeLoad();
void storeSave();
void storeWipe();                       // forget Wi-Fi + location (3x power-cycle reset)
uint8_t bootCount();                    // increments on every boot, returns the new value
void bootCountClear();
// OTA bookkeeping
String prefGet(const char* k);
void prefSet(const char* k, const String& v);
