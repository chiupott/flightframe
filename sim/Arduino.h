#pragma once
// Host shim: Arduino core API (portable String/Print from arduino/ArduinoCore-API) + fake clock.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define main arduino_unused_main
#include "api/ArduinoAPI.h"
#undef main
using namespace arduino;
using arduino::String;
#ifndef PROGMEM
#define PROGMEM
#endif
void delay(unsigned long ms);
struct SerialShim : public Print { void begin(long) {} size_t write(uint8_t c) override { putchar(c); return 1; } size_t write(const uint8_t* b, size_t n) override { return fwrite(b, 1, n, stdout); } };
extern SerialShim Serial;
struct EspShim { void restart() { printf("[ESP.restart]\n"); exit(0); } };
extern EspShim ESP;
