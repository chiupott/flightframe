#include "rain.h"
#include "config.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <SD.h>
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_x509_crt_bundle_bin_start");
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3   // Arduino core 3.x (ESP32-C6) wants the bundle size too
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_src_x509_crt_bundle_bin_end");
#define SET_CA(c) (c).setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start)
#else
#define SET_CA(c) (c).setCACertBundle(rootca_crt_bundle_start)
#endif
// A GitHub Action rebuilds this file every 10 minutes from RainViewer radar tiles and uploads it to a rolling pre-release called "rain".
#define RAIN_URL "https://github.com/" GH_OWNER "/" GH_REPO "/releases/download/rain/rain.bin"
bool rainRefresh() {
  WiFiClientSecure c; SET_CA(c);
  HTTPClient h; h.setTimeout(20000); h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); h.setUserAgent("FlightFrame/" FW_VERSION);
  if (!h.begin(c, RAIN_URL)) return false;
  int code = h.GET(); int len = h.getSize();
  if (code != 200 || len < 14 || len > 200000) { h.end(); return false; }
  SD.mkdir("/map"); SD.remove("/map/rain.tmp");
  File f = SD.open("/map/rain.tmp", FILE_WRITE); if (!f) { h.end(); return false; }
  WiFiClient* s = h.getStreamPtr(); uint8_t buf[1024]; int left = len; uint32_t last = millis(); bool head = false, ok = true;
  while (left > 0 && h.connected() && millis() - last < 15000) {
    int n = s->readBytes(buf, min(left, (int)sizeof buf));
    if (n <= 0) { delay(5); continue; }
    if (!head) { if (n < 4 || memcmp(buf, "FFR2", 4)) { ok = false; break; } head = true; }
    if (f.write(buf, n) != (size_t)n) { ok = false; break; }
    left -= n; last = millis();
  }
  f.close(); h.end();
  if (!ok || left != 0) { SD.remove("/map/rain.tmp"); return false; }
  SD.remove("/map/rain.bin");
  return SD.rename("/map/rain.tmp", "/map/rain.bin");
}
