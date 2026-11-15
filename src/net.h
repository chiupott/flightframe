#pragma once
#include <WiFiClientSecure.h>
// TLS client that verifies server certificates against the ESP-IDF root bundle.
bool httpsGet(const String& url, String& body, uint32_t timeoutMs = 10000);
bool wifiConnect(uint32_t timeoutMs = 20000);
