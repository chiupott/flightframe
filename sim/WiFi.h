#pragma once
#include "Arduino.h"
enum { WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum { WIFI_STA = 1, WIFI_AP_STA = 3 };
struct WiFiShim { int status() { return WL_CONNECTED; } void mode(int) {} };
extern WiFiShim WiFi;
