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
  if (cfg.view == 2 && !forced) {                                              // engines view: nothing moves, so only redraw (= flash) when the counts change, or every 5 minutes
    static uint32_t lastSig = 0, lastAt = 0; Tally T; tallyPlanes(T);
    uint32_t sig = (T.engines * 131u + T.people) * 31u + T.planesCounted + (feed == Feed::Live ? 7u : 0u);
    if (lastAt && sig == lastSig && millis() - lastAt < 300000UL) return;
    lastSig = sig; lastAt = millis();
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
