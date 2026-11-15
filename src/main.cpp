#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "store.h"
#include "net.h"
#include "geo.h"
#include "adsb.h"
#include "draw.h"
#include "portal.h"
#include "ota.h"
#include "storage.h"
#include "rain.h"

static uint32_t lastOta = 0, lastGood = 0, lastRain = 0; static bool markedGood = false;

void setup() {
  Serial.begin(115200);
  otaBootCheck();                                   // rolls back a bad update after 3 failed boots
  storeLoad();
  drawInit(); storInit();
  if (bootCount() >= 3) {                           // 3 power-cycles in quick succession = reset
    storeWipe(); bootCountClear();
    drawMessage("Reset done", "Join Wi-Fi: " AP_NAME); portalRun();
  }
  if (!cfg.ssid.length()) { drawMessage("Welcome", "Join Wi-Fi: " AP_NAME); portalRun(); }
  drawMessage("Connecting...", cfg.ssid.c_str());
  if (!wifiConnect()) { drawMessage("No Wi-Fi", "Retrying..."); }
  if (isnan(cfg.lat) && WiFi.status() == WL_CONNECTED) {            // geocode once; stays on the device
    float la, lo;
    if (geocodePostcode(cfg.postcode, la, lo)) { cfg.lat = la; cfg.lon = lo; storeSave(); }
    else { drawMessage("Postcode not found", cfg.postcode.c_str()); delay(8000); storeWipe(); ESP.restart(); }
  }
}

void loop() {
  static uint32_t t0 = 0; static bool booted = false;
  if (millis() > GOOD_BOOT_MS && !booted) { bootCountClear(); booted = true; }     // survived 5 s: not a reset attempt
  if (millis() - t0 < POLL_MS && t0) { delay(200); return; }
  t0 = millis();
  bool wifi = WiFi.status() == WL_CONNECTED;
  if (!wifi) wifi = wifiConnect(10000);
  Feed feed = Feed::None;
  if (wifi && adsbPoll()) {
    if (cfg.view == 0) for (uint8_t i = 0; i < nPlanes && i < cfg.rows; i++) routeLookup(planes[i]);   // only the rows we show
    feed = Feed::Live; lastGood = millis();
  } else if (lastGood && millis() - lastGood < 10UL * 60UL * 1000UL) feed = Feed::Stale;
  if (wifi && cfg.rain && (!lastRain || millis() - lastRain > 10UL * 60UL * 1000UL)) { lastRain = millis(); rainRefresh(); }
  drawScreen(feed, wifi);
  if (feed == Feed::Live && !markedGood) { otaMarkGood(); markedGood = true; }
  if (wifi && (!lastOta || millis() - lastOta > OTA_CHECK_MS)) { lastOta = millis(); otaCheckAndUpdate(); }
}
