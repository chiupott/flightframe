#include "net.h"
#include "diag.h"
#include "store.h"
#include "clock.h"
#include <WiFi.h>
#include <HTTPClient.h>
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_x509_crt_bundle_bin_start");
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3   // Arduino core 3.x (ESP32-C6) wants the bundle size too
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_src_x509_crt_bundle_bin_end");
#define SET_CA(c) (c).setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start)
#else
#define SET_CA(c) (c).setCACertBundle(rootca_crt_bundle_start)
#endif
bool wifiConnect(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA); WiFi.begin(cfg.ssid.c_str(), cfg.pw.c_str());
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < timeoutMs) delay(250);
  if (WiFi.status() != WL_CONNECTED) return false;
  clockInit();                                   // TLS certificates are date-checked, so the clock must be right before any HTTPS call
  return true;
}
bool httpsGet(const String& url, String& body, uint32_t timeoutMs) {
  WiFiClientSecure c; SET_CA(c);
  HTTPClient h; h.setTimeout(timeoutMs); h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  h.setUserAgent("FlightFrame/" FW_VERSION);
  if (!h.begin(c, url)) return false;
  int code = h.GET(); g_dHttp = code;
  if (code == 200) body = h.getString();
  h.end();
  return code == 200;
}

bool httpsGetJson(const String& url, JsonDocument& doc, JsonDocument& filter, uint32_t timeoutMs) {
  WiFiClientSecure c; SET_CA(c);
  HTTPClient h; h.setTimeout(timeoutMs); h.useHTTP10(true); h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  h.setUserAgent("FlightFrame/" FW_VERSION);
  if (!h.begin(c, url)) return false;
  int code = h.GET(); g_dHttp = code;
  bool ok = false;
  if (code == 200) {
    // ArduinoJson cannot read straight from a network stream (an empty buffer looks like the end), so read the body into one
    // buffer first, then parse it through the filter. HTTP/1.0 = no chunking, ends when the server closes.
    size_t cap = 96 * 1024, total = 0; char* buf = (char*)malloc(cap);
    WiFiClient* sp = h.getStreamPtr(); uint32_t last = millis();
    while (buf && (h.connected() || sp->available()) && millis() - last < timeoutMs) {
      if (total + 1024 > cap) { if (cap >= 160 * 1024) break; char* nb = (char*)realloc(buf, cap + 32 * 1024); if (!nb) break; buf = nb; cap += 32 * 1024; }
      int n = sp->readBytes(buf + total, 1024);
      if (n > 0) { total += n; last = millis(); } else delay(2);
    }
    if (buf) { DeserializationError err = deserializeJson(doc, buf, total, DeserializationOption::Filter(filter)); ok = !err; if (err) g_dHttp = -1000 - (int)err.code(); }
    free(buf);
  }
  h.end();
  return ok;
}
