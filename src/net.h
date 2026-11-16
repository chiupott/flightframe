#pragma once
#include <WiFiClientSecure.h>
// TLS client that verifies server certificates against the ESP-IDF root bundle.
#include <ArduinoJson.h>
// Streams a big JSON reply straight into a filtered document (never holds the whole body in RAM). HTTP/1.0 = no chunked encoding.
bool httpsGetJson(const String& url, JsonDocument& doc, JsonDocument& filter, uint32_t timeoutMs = 15000);
bool httpsGet(const String& url, String& body, uint32_t timeoutMs = 10000);
bool wifiConnect(uint32_t timeoutMs = 20000);
