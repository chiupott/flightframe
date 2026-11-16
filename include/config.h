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
#define MAX_AIRCRAFT       40
#define TRAIL_KEEP_MS      (5UL*60UL*1000UL)

// ---- Pins ----
#if CONFIG_IDF_TARGET_ESP32C6 || defined(BOARD_SEENGREAT_C6)
// Seengreat ESP32-C6 e-paper driver board (from its manual). The SD card shares the SPI bus with the panel.
#define EPD_SCK 5
#define EPD_MOSI 4
#define EPD_CS 7
#define EPD_DC 3
#define EPD_RST 6
#define EPD_BUSY 2
#define SD_CS 20
#define SD_MISO 19
#else
// Waveshare-style ESP32 driver board (unverified)
#define EPD_SCK 13
#define EPD_MOSI 14
#define EPD_CS 15
#define EPD_DC 27
#define EPD_RST 26
#define EPD_BUSY 25
#define SD_CS 5
#define SD_MISO -1
#endif

struct Config {
  String ssid, pw, postcode;
  String assetKey;         // read-only GitHub key for the private logo pack (set in the setup page; never in the repo)
  float lat = NAN, lon = NAN;
  float radiusMi = 50;     // radar radius in miles (always stored in miles; km users get a converted value)
  uint8_t rows = 5;
  uint16_t heading = 0;    // degrees that point "up" on the screen (radar) / the way the window faces (window view)
  uint8_t view = 0;        // 0 = radar + list, 1 = window view, 2 = engines view
  uint8_t hl = 3;          // engines view: engine maker drawn solid black (0 none, 1 CFM, 2 P&W, 3 GE, 4 Rolls-Royce)
  bool flip = false, rain = true, h24 = true, km = false, nm = false;   // km / nm: show distances and radius in kilometres / nautical miles (neither = miles)
  bool valid() const { return ssid.length() && !isnan(lat) && !isnan(lon); }
};
extern Config cfg;
