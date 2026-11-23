#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include "config.h"
#include "store.h"
#include "msg.h"
#include "net.h"
#include "geo.h"
#include "adsb.h"
#include "draw.h"
#include "portal.h"
#include "ota.h"
#include "storage.h"
#include "rain.h"
#include "sdpack.h"
#include "diag.h"

static uint32_t lastOta = 0, lastGood = 0, lastRain = 0; static bool markedGood = false;

#ifndef BTN_PIN
#define BTN_PIN 9      // BOOT button on the C6 board (IO9); short press = next view
#endif
// BOOT button: an interrupt counts every press, even while the screen is busy redrawing. The loop then steps the view once per press and
// redraws once, shortly after the last press, so tapping three times jumps three views with one refresh.
static volatile uint8_t btnCount = 0; static volatile uint16_t btnRaw = 0; static volatile uint32_t btnLast = 0;
static volatile bool btnDown = false; static volatile uint32_t btnUp = 0;
// One physical press = one count: only a clean press edge counts, and the contact bounce when the button is let go (which looked like a second press) is ignored.
static void IRAM_ATTR btnIsr() {
  uint32_t m = millis(); btnRaw++;
  if (gpio_get_level((gpio_num_t)BTN_PIN) == 0) { if (!btnDown && m - btnUp > 80) { btnDown = true; btnLast = m; if (btnCount < 3) btnCount++; } }
  else if (btnDown) { btnDown = false; btnUp = m; }
}

static bool sdOk = false;   // SD card mounted (retried in loop so a card can be pushed in later)

void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP); attachInterrupt(digitalPinToInterrupt(BTN_PIN), btnIsr, CHANGE);
  otaBootCheck();                                   // rolls back a bad update after 3 failed boots
  storeLoad();
#ifdef FF_DEV_RESTORE   // developer recovery after an accidental settings wipe: reuse the Wi-Fi the chip itself remembers, test location Manchester Airport
  if (!cfg.ssid.length()) {
    WiFi.mode(WIFI_STA); WiFi.begin(); uint32_t t = millis(); while (WiFi.status() != WL_CONNECTED && millis() - t < 20000) delay(250);
    if (WiFi.status() == WL_CONNECTED) { cfg.ssid = WiFi.SSID(); cfg.pw = WiFi.psk(); cfg.postcode = "M90 1QX"; cfg.lat = 53.3537f; cfg.lon = -2.2750f; cfg.view = 1; storeSave(); }
  }
#endif
#ifdef FF_DEV_LOC   // developer test: set the frame location from build flags (DEV_PC postcode, DEV_HEAD facing), never stored in the repo; coordinates come from the postcode lookup
  if (cfg.postcode != DEV_PC) { cfg.postcode = DEV_PC; cfg.lat = NAN; cfg.lon = NAN; cfg.heading = DEV_HEAD; cfg.headingWin = DEV_HEAD; cfg.view = 1; cfg.bw = false; storeSave(); }
#endif
  drawInit(); sdOk = storInit();
#ifdef FF_SWATCH
  drawSwatches(); for (;;) delay(1000);
#endif
  esp_reset_reason_t rr = esp_reset_reason(); bool manual = rr == ESP_RST_POWERON || rr == ESP_RST_EXT || rr == ESP_RST_BROWNOUT;   // flashing / USB resets must not count towards the 3-reset factory reset
  if (manual && bootCount() >= 3) {                           // 3 power-cycles in quick succession = reset
    storeWipe(); bootCountClear();
    drawWelcome(); portalRun();
  }
  if (!cfg.ssid.length()) { drawWelcome(); portalRun(); }
  drawMessage("Connecting...", cfg.ssid.c_str());
  if (!wifiConnect()) { drawMessage("No Wi-Fi", "Retrying..."); }
  if (isnan(cfg.lat) && WiFi.status() == WL_CONNECTED) {            // geocode once; stays on the device
    float la, lo; GeoResult r = geocodeEx(cfg.postcode, cfg.country, la, lo);
    if (r == GEO_OK) { cfg.lat = la; cfg.lon = lo; storeSave(); }
    else if (r == GEO_NOTFOUND) { drawMessage("Postcode not found", cfg.postcode.c_str()); delay(8000); storeWipe(); ESP.restart(); }
    // GEO_NET: no internet yet. Keep everything; the loop tries again below.
  }
}

void loop() {
  static uint32_t t0 = 0; static bool booted = false, packTried = false, force = false;
  if (millis() > GOOD_BOOT_MS && !booted) { bootCountClear(); booted = true; }     // survived 5 s: not a reset attempt
  static bool pend = false, viewOnly = false, card = false; static uint32_t pressAt = 0;
  if (card) { if (btnCount) { btnCount = 0; card = false; drawForceFull(); force = true; viewOnly = true; } else { delay(40); return; } }          // a message card is up: any BOOT press closes it and brings the view back
  if (btnCount) { uint8_t n = btnCount; btnCount = 0; Serial.printf("btn: %u taps (%u raw edges) at view %d bw %d, pin=%d\n", n, (unsigned)btnRaw, cfg.view, cfg.bw, digitalRead(BTN_PIN)); while (n--) { if (cfg.view == 0 && !cfg.bw) cfg.bw = true; else { cfg.bw = false; cfg.view = (cfg.view + 1) % 3; } } pend = true; pressAt = millis(); }
  if (pend && millis() - pressAt > 700) { pend = false; Serial.printf("btn: redraw view %d bw %d\n", cfg.view, cfg.bw); storeSave(); drawForceFull(); force = true; viewOnly = true; }        // BOOT button: next view, remembered
  settingsPoll(); if (settingsChanged()) {
    String pc = settingsTakePostcode();
    if (pc.length()) {                                                           // new postcode from the settings page: look it up, keep the old location if it is not found
      settingsSuspend(); drawMessage("Finding your new location", pc.c_str()); float la, lo;
      if (WiFi.status() == WL_CONNECTED && geocodePostcode(pc, la, lo)) { cfg.postcode = pc; cfg.lat = la; cfg.lon = lo; storeSave(); }
      else { drawMessage("Postcode not found", "Keeping the old location"); delay(4000); }
    }
    drawForceFull(); force = true; }        // settings page on the home Wi-Fi
  if (!force && millis() - t0 < (cfg.view == 0 && cfg.bw ? 15000UL : POLL_MS) && t0) {
    delay(20); return;
  }
  force = false; t0 = millis(); settingsSuspend(); uint32_t lp = t0; auto lap = [&](const char* w) { uint32_t n = millis(); if (n - lp > 1500) Serial.printf("slow step: %s %lu ms\n", w, (unsigned long)(n - lp)); lp = n; };
  if (viewOnly) {                                                              // a BOOT-button view change: redraw at once from the planes we already hold, no fetch first
    viewOnly = false;
    if (lastGood) {
      bool w = WiFi.status() == WL_CONNECTED;
      if (w && cfg.view == 0) for (uint8_t i = 0; i < nPlanes && i < cfg.rows; i++) routeLookup(planes[i]);   // cached routes are instant
      uint32_t tv = millis(); drawScreen(millis() - lastGood < 3UL * POLL_MS ? Feed::Live : Feed::Stale, w);
      Serial.printf("view change: drawn in %lu ms, %u taps waiting\n", (unsigned long)(millis() - tv), (unsigned)btnCount);
      return;
    }
  }
  bool wifi = WiFi.status() == WL_CONNECTED;
  if (!wifi) wifi = wifiConnect(10000);
  if (isnan(cfg.lat)) {                                                          // location still unknown (no internet when it was set up): keep trying, never wipe the settings for this
    float la, lo; GeoResult r = wifi ? geocodeEx(cfg.postcode, cfg.country, la, lo) : GEO_NET;
    if (r == GEO_OK) { cfg.lat = la; cfg.lon = lo; storeSave(); }
    else if (r == GEO_NOTFOUND) { drawMessage("Postcode not found", cfg.postcode.c_str()); delay(8000); storeWipe(); ESP.restart(); }
    else { drawMessage("Waiting for the internet", "Will try again in a minute"); delay(45000); return; }
  }
  if (!sdOk) sdOk = storInit();
  g_dSd = sdOk;
  lap("wifi+sd");
  if (wifi && sdOk && !packTried) { packTried = true; g_dPack = sdpackEnsure(); }     // first boot with a blank card: fetch map + plane pictures once per boot
  lap("sdpack");
  Feed feed = Feed::None;
  if (wifi && adsbPoll()) {
    if (cfg.view == 0) for (uint8_t i = 0; i < nPlanes && i < cfg.rows; i++) routeLookup(planes[i]);   // only the rows we show
    feed = Feed::Live; lastGood = millis();
  } else if (lastGood && millis() - lastGood < 10UL * 60UL * 1000UL) feed = Feed::Stale;
  if (wifi && cfg.rain && (!lastRain || millis() - lastRain > 10UL * 60UL * 1000UL)) { lastRain = millis(); Serial.printf("heap before rain: %u max %u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap()); rainRefresh(); Serial.printf("heap after rain: %u\n", ESP.getFreeHeap()); }
  lap("planes+routes+rain");
  static uint32_t lastMsg = 0; static Msg mg;
  if (wifi && feed == Feed::Live && (!lastMsg || millis() - lastMsg > 55000UL)) { lastMsg = millis(); if (msgPoll(mg)) { lap("msg"); msgDraw(mg); card = true; return; } }      // a private note: show it instead of the view
  uint32_t tD = millis(); drawScreen(feed, wifi); uint32_t tE = millis();
  Serial.printf("cycle: fetch %lu ms, draw %lu ms, taps waiting %u\n", (unsigned long)(tD - t0), (unsigned long)(tE - tD), (unsigned)btnCount);
  if (!lastGood && feed == Feed::None) t0 = millis() - ((cfg.view == 0 && cfg.bw ? 15000UL : POLL_MS) - 8000UL);   // nothing fetched yet (network still settling after boot): try again in 8 s, not a minute
  if (feed == Feed::Live && !markedGood) { otaMarkGood(); markedGood = true; }
  if (wifi && feed == Feed::Live && (!lastOta || millis() - lastOta > OTA_CHECK_MS)) { otaCheckAndUpdate(); lastOta = otaChecked() ? millis() : (millis() - OTA_CHECK_MS + 5UL * 60UL * 1000UL); }   // only once the internet is proven; a failed check is retried in 5 minutes
}
