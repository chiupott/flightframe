#pragma once
#include <Arduino.h>

// ---- Project constants (edit once the repo exists) ----
#define GH_OWNER   "chiupott"          // GitHub user that owns the public repo
#define GH_REPO    "flightframe"
#define GH_ASSET   "firmware.bin"       // release asset the frame downloads
#define AP_NAME    "FlightFrame-Setup"  // open Wi-Fi network shown during setup
#define PORTAL_IP  "192.168.4.1"

// ---- Timing ----
#define POLL_MS            30000UL      // plane poll + redraw (adsb.fi limit is 1 req/s, we are far below)
#define OTA_CHECK_MS       (6UL*3600UL*1000UL)
#define FULL_REFRESH_EVERY 12           // partial refreshes between full ones
#define GOOD_BOOT_MS       5000UL       // uptime after which the 3x power-cycle counter resets
#define MAX_TRAIL_PTS      10
#define MAX_AIRCRAFT       24
#define TRAIL_KEEP_MS      (5UL*60UL*1000UL)

// ---- Display pins: Waveshare-style ESP32 driver board (CHECK against the kit on arrival) ----
#define EPD_SCK 13
#define EPD_MOSI 14
#define EPD_CS 15
#define EPD_DC 27
#define EPD_RST 26
#define EPD_BUSY 25
// SD card (logos) pins: CHECK on kit day
#define SD_CS 5

struct Config {
  String ssid, pw, postcode;
  float lat = NAN, lon = NAN;
  float radiusMi = 50;     // radar radius in miles (always stored in miles; km users get a converted value)
  uint8_t rows = 5;
  uint16_t heading = 0;    // degrees that point "up" on the screen
  bool flip = false, rain = true, h24 = true, km = false;   // km: show distances and radius in kilometres
  bool valid() const { return ssid.length() && !isnan(lat) && !isnan(lon); }
};
extern Config cfg;
