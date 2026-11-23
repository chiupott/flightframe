#include "gfx.h"
#include "views.h"
#include "fonts.h"
#include "diag.h"
#include "engines.h"
void drawForceFull() { partials = 0; }   // next redraw is a full (flashing) refresh, e.g. after switching view
void drawScreen(Feed feed, bool wifiOk) {
  bool forced = partials == 0;
#ifdef FF_GREY
  bool bw = cfg.view == 0 && cfg.bw;                                           // B/W radar: the original look, partial refreshes, no flash
  g_grey = !bw;
#else
  bool bw = true;
#endif
  if (cfg.view == 1 && !terrainReady()) {                                   // no skyline for this location + facing yet: make it (needs Wi-Fi + SD)
    static uint32_t lastTry = 0;
    if (wifiOk && (!lastTry || millis() - lastTry > 30UL * 60UL * 1000UL)) { lastTry = millis(); terrainGenerate(); winRetry(); }
  }
#ifdef FF_GREY
  if (!bw) {                                                                   // grey views flash the whole screen, so they redraw only every 2 min (radar, window) or 4 min (engines); planes are still fetched every minute so a message card never waits long. BOOT forces a draw and restarts the wait.
    static uint32_t lastAt[3] = {0, 0, 0}; static const uint32_t GAP[3] = {110000UL, 110000UL, 230000UL};   // the poll lands every 60 s, so these pass on the 2nd / 4th poll
    uint8_t v = cfg.view < 3 ? cfg.view : 0;
    if (!forced && lastAt[v] && millis() - lastAt[v] < GAP[v]) return;
    lastAt[v] = millis();
  }
#endif
  bool full = (partials++ % FULL_REFRESH_EVERY) == 0;
  String clk = clockStr();
  if (cfg.view == 1) { winLoad(); if (!winReady()) { drawMessage("Window view needs its terrain file", "Copy /window/terrain.bin to the SD card"); return; } }
  pageBegin(bw, full);
  do { epd.fillScreen(PAPER); if (cfg.view == 1) drawWindow(feed, wifiOk, clk.c_str()); else if (cfg.view == 2) drawEngines(clk.c_str()); else { drawRadar(feed, wifiOk, clk.c_str()); drawList(clk.c_str()); }
#ifdef FF_DIAG   // build with -DFF_DIAG to print a status line (SD / download / feed / plane count) in the corner
    if (cfg.view != 2) { char dg[48]; snprintf(dg, sizeof dg, "sd:%d pack:%d http:%d n:%d", g_dSd, g_dPack, g_dHttp, (int)nPlanes); epd.setFont(nullptr); epd.setTextSize(1); epd.setTextColor(INK); epd.fillRect(626, 466, 170, 12, PAPER); epd.setCursor(632, 468); epd.print(dg); }
#endif
  } while (pageNext());
}
