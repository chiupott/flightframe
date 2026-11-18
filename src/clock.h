#pragma once
#include <Arduino.h>
void clockInit(uint32_t waitMs = 8000);   // start NTP (UK time zone); waits up to waitMs for the first sync
uint32_t clockNow();                       // unix seconds, 0 until synced
bool clockSet();                          // true once the clock has synced (TLS needs this too)
String clockStr();                        // "13:04" or "1:04 pm" per setting; "--:--" until synced
String clockFmt(uint32_t t);                // a unix time as local "13:04" / "1:04 pm" per setting
