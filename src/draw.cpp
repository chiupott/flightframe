// STUB renderer: proves the data path and projection on the real panel. The final look (NicoClean font,
// ADS-B Radar icons, coastline, rain dots, logos from SD) is ported from the design boards next.
#include "draw.h"
#include "config.h"
#include "adsb.h"
#include "icons.h"
#include "map.h"
#include "view.h"
#include "storage.h"
#include <ctype.h>
#include "status_data.h"
#include <SPI.h>
#ifdef FF_GREY
#include "epd_grey.h"
#else
#include <GxEPD2_BW.h>
#endif
#include "clock.h"
#include <vector>
#include <algorithm>
#include "fonts/Nico14.h"
#include "fonts/Nico16.h"
#include "fonts/NicoItalic14.h"
#ifdef FF_GREY
static EpdGrey epd;                                                                                    // 4 grey levels, full-screen flash every redraw
#else
static GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> epd(GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));   // full 48 KB frame buffer, one page
#endif
static uint8_t partials = 0;
#define INK GxEPD_BLACK
#define PAPER GxEPD_WHITE
#ifndef FF_GREY
#define GREY_L INK    // black-and-white build: greys fall back to black
#define GREY_D INK
#endif

// ---- text helpers: NicoClean pixel fonts; UTF-8 arrows etc. map to the font's spare slots ----
static String fx(const String& s) { String o; for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (c == 0xE2 && i + 2 < s.length()) { uint8_t b = s[i + 1], d = s[i + 2]; if (b == 0x86 && d == 0x92) { o += (char)0x80; i += 2; continue; } if (b == 0x80 && d == 0x93) { o += (char)0x81; i += 2; continue; } }
    if (c == 0xC2 && i + 1 < s.length() && (uint8_t)s[i + 1] == 0xB7) { o += (char)0x82; i += 1; continue; }
    o += (char)c; } return o; }
static int tw(const GFXfont* f, const String& s) { epd.setTextSize(1); epd.setFont(f); int16_t x1, y1; uint16_t w, h; epd.getTextBounds(fx(s), 0, 0, &x1, &y1, &w, &h); return x1 + w; }
static int put(const GFXfont* f, int x, int y, const String& s, uint16_t col = INK) { epd.setTextSize(1); epd.setFont(f); epd.setTextColor(col); epd.setCursor(x, y); epd.print(fx(s)); return x + tw(f, s); }
static void putHalo(const GFXfont* f, int x, int y, const String& s, int r, uint16_t col = INK) {            // paper outline so text stays readable over the map
  for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (dx || dy) { if (abs(dx) + abs(dy) > r + 1) continue; put(f, x + dx, y + dy, s, PAPER); }
  put(f, x, y, s, col);
}
static void hline(int x, int y, int w, int th) { epd.fillRect(x, y, w, th, INK); }
static void box(int x, int y, int w, int h) { epd.fillRect(x, y, w, h, PAPER); epd.drawRect(x, y, w, h, INK); epd.drawRect(x + 1, y + 1, w - 2, h - 2, INK); }
static void bmp(const Bmp1& b, int x, int y) { int stride = (b.w + 7) / 8; for (int r = 0; r < b.h; r++) for (int c = 0; c < b.w; c++) if (b.bits[r * stride + (c >> 3)] & (0x80 >> (c & 7))) epd.drawPixel(x + c, y + r, INK); }

// airline logo from the SD card (/logos/<ICAO>.lg); airline = first 3 letters of an airline-style callsign (3 letters + digit)
static bool airlineOf(const char* cs, char* out) { if (strlen(cs) < 4 || !isalpha(cs[0]) || !isalpha(cs[1]) || !isalpha(cs[2]) || !isdigit(cs[3])) return false; memcpy(out, cs, 3); out[3] = 0; for (int i = 0; i < 3; i++) out[i] = toupper(out[i]); return true; }
static void drawLogo(const char* cs, int x, int y) {
  char al[4]; if (!airlineOf(cs, al)) return;
  size_t n = 0; uint8_t* b = storLoad((String("/logos/") + al + ".lg").c_str(), n);
  if (b && n > 2) { int w = b[0], h = b[1], stride = (w + 7) / 8; for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) if (b[2 + r * stride + (c >> 3)] & (0x80 >> (c & 7))) epd.drawPixel(x + c, y + r, INK); free(b); return; }
  free(b);
  box(x, y + 9, 44, 26); put(&Nico14, x + 22 - tw(&Nico14, al) / 2, y + 27, al);        // no logo on the card: code badge
}
static float dU(float mi) { return cfg.km ? mi * 1.609344f : cfg.nm ? mi * 0.868976f : mi; }
static const char* uName() { return cfg.km ? "km" : cfg.nm ? "nm" : "mi"; }
static const int LX = 483, RH = 88;
static void drawList(const char* clock) {
  epd.fillRect(LX, 0, 800 - LX, 480, PAPER);        // wipe anything the radar spilled over (labels, icons, trails near the edge)
  epd.fillRect(LX, 0, 3, 480, INK);
  put(&Nico16, LX + 15, 26, String("TOP ") + cfg.rows + " NEAREST");
  put(&Nico14, 800 - 12 - tw(&Nico14, clock), 25, clock);
  hline(LX + 3, 40, 800 - LX - 3, 2);
  for (uint8_t i = 0; i < cfg.rows; i++) {
    int top = 42 + i * RH;
    if (i < nPlanes) {
      const Aircraft& a = planes[i];
      int x = put(&Nico16, LX + 15, top + 22, a.routeKnown ? String(a.from) + " \xE2\x86\x92 " + a.to : String("Route unknown"));
      put(&Nico14, x + 6, top + 22, String("(") + String(dU(a.distMi), 1) + " " + uName() + ")", GREY_D);
      drawLogo(a.cs, 800 - 12 - 44, top + 22);
      const Sprite& sp = spriteFor(a.type, a.cat);
      int x2 = put(&Nico14, LX + 15, top + 50, String(a.fn[0] ? a.fn : a.hex)); x2 = put(&Nico14, x2 + 8, top + 50, a.type, GREY_D);   // flight number black, aircraft type grey
      drawSprite(epd, x2 + 6 + (int)(sp.side * 0.75f) / 2 + 2, top + 44, 90, sp, 0.75f);   // row icon: nose right, centred on the callsign line
      String alt = String((long)a.alt); if (a.alt >= 1000) alt = String((long)(a.alt / 1000)) + "," + (a.alt % 1000 < 100 ? "0" : "") + (a.alt % 1000 < 10 ? "0" : "") + String((long)(a.alt % 1000));
      put(&Nico14, LX + 15, top + 77, alt + " ft \xC2\xB7 " + String((int)a.gs) + " kt \xC2\xB7 " + a.reg);
    }
    if (i < cfg.rows - 1) hline(LX + 3, top + RH - 2, 800 - LX - 3, 2);
  }
}

// ---- radar ----
static void ring(int r) {                                                    // dotted ring: 3 on / 5 off, 2 px, paper halo
  float circ = 2 * PI * r;
#ifdef FF_GREY
  for (float s = 0; s < circ; s += 1) { if (((int)s % 8) >= 4) continue; float a = s / r; epd.fillRect(VCX + (int)lroundf(r * sinf(a)) - 1, VCY - (int)lroundf(r * cosf(a)) - 1, 3, 3, GREY_D); }   // grey dots, no halo
  return;
#endif
  for (int pass = 0; pass < 2; pass++) for (float s = 0; s < circ; s += 1) {
    if (pass == 1 && ((int)s % 8) >= 3) continue;
    float a = s / r; int x = VCX + (int)lroundf(r * sinf(a)), y = VCY - (int)lroundf(r * cosf(a));
    if (pass == 0) epd.fillRect(x - 2, y - 2, 5, 5, PAPER); else epd.fillRect(x, y, 2, 2, INK);
  }
}
struct Rect { int x0, y0, x1, y1; };
static bool hit(const Rect& a, const Rect& b) { return !(a.x1 < b.x0 || b.x1 < a.x0 || a.y1 < b.y0 || b.y1 < a.y0); }
static void drawTrail(const Aircraft& a, float x, float y) {
  const Trail* t = trailFor(a.hex); if (!t || !t->n) return;
  float px[MAX_TRAIL_PTS + 1], py[MAX_TRAIL_PTS + 1]; int n = 0; px[n] = x; py[n++] = y;
  float len = 0;
  for (int k = t->n - 1; k >= 0 && len < 78; k--) { viewProj(t->lat[k], t->lon[k], px[n], py[n]); len += hypotf(px[n] - px[n - 1], py[n] - py[n - 1]); n++; }
  for (int pass = 0; pass < 2; pass++) for (int i = 0; i + 1 < n; i++) {
    if (pass == 0) { for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) if (abs(dx) + abs(dy) <= 3) epd.drawLine(px[i] + dx, py[i] + dy, px[i + 1] + dx, py[i + 1] + dy, PAPER); }
    else { epd.drawLine(px[i], py[i], px[i + 1], py[i + 1], GREY_L); epd.drawLine(px[i] + 1, py[i], px[i + 1] + 1, py[i + 1], GREY_L); epd.drawLine(px[i], py[i] + 1, px[i + 1], py[i + 1] + 1, GREY_L); }
  }
}
static void drawRadar(Feed feed, bool wifiOk, const char* clock) {
  mapDraw(epd);
  ring(VR); ring(VR / 2);
  String mi = String((int)lroundf(dU(cfg.radiusMi) / 2)) + " " + uName(), mo = String((int)lroundf(dU(cfg.radiusMi))) + " " + uName();
  putHalo(&Nico14, 246, 140, mi, 1); putHalo(&Nico14, 246, 462, mo, 1);
  epd.fillCircle(VCX, VCY, 10, PAPER); epd.drawCircle(VCX, VCY, 10, INK); epd.drawCircle(VCX, VCY, 9, INK); epd.fillCircle(VCX, VCY, 3, INK);

  // positions + keep-out boxes (overlays first, then plane icons, then labels as they are placed)
  float X[MAX_AIRCRAFT], Y[MAX_AIRCRAFT]; int R_[MAX_AIRCRAFT]; Rect pbox[MAX_AIRCRAFT], lbox[MAX_AIRCRAFT];
  std::vector<Rect> boxes = {{9, 9, 143, 35}, {14, 46, 54, 102}, {150, 9, 470, 35}, {9, 445, 67, 471}, {229, 450, 264, 468}, {229, 128, 274, 144}};
  for (uint8_t i = 0; i < nPlanes; i++) {
    viewProj(planes[i].lat, planes[i].lon, X[i], Y[i]);
    const Sprite& sp = spriteFor(planes[i].type, planes[i].cat); R_[i] = max(13, sp.side / 2);
    pbox[i] = {(int)X[i] - R_[i], (int)Y[i] - R_[i], (int)X[i] + R_[i], (int)Y[i] + R_[i]}; boxes.push_back(pbox[i]);
  }
  for (uint8_t i = 0; i < nPlanes; i++) drawTrail(planes[i], X[i], Y[i]);
  for (uint8_t i = 0; i < nPlanes; i++) { const Aircraft& a = planes[i]; drawSprite(epd, (int)X[i], (int)Y[i], viewAngle(a.track), spriteFor(a.type, a.cat)); }
  // labels: try the full 3-line label (callsign / type / route) in 12 spots; if none is free drop the type line, then the route, and only then take the least-overlapping in-bounds spot
  for (uint8_t i = 0; i < nPlanes; i++) {
    const Aircraft& a = planes[i];
    String t1 = a.fn[0] ? a.fn : a.hex, t2 = String("\xE2\x86\x92 ") + (a.routeKnown ? a.to : "?"), t3 = a.type;
    float x = X[i], y = Y[i]; int r = R_[i] + 1;
    struct Cand { Rect b; float x0; float lx, ly; int anc; };
    auto cand = [&](int lines, int k, int w) {
      float ly[12] = {y - 8, y - 8, y - r - 8 - 14.0f * (lines - 1), y + r + 16, y - 30, y - 30, y + 12, y + 12, y - 46, y - 46, y + 28, y + 28};
      float lx[12] = {x + r, x - r, x, x, x + r, x - r, x + r, x - r, x + r, x - r, x + r, x - r};
      int anc[12] = {0, 1, 2, 2, 0, 1, 0, 1, 0, 1, 0, 1};
      Cand c{}; c.lx = lx[k]; c.ly = ly[k]; c.anc = anc[k];
      c.x0 = c.anc == 0 ? c.lx : c.anc == 1 ? c.lx - w : c.lx - w / 2.0f;
      c.b = {(int)(c.x0 - 2), (int)(c.ly - 12), (int)(c.x0 + w + 2), (int)(c.ly + 14 * (lines - 1) + 5)};
      return c;
    };
    auto inBounds = [](const Rect& b) { return !(b.x0 < 2 || b.x1 > 478 || b.y0 < 2 || b.y1 > 478); };
    auto hits = [&](const Rect& b) { int n = 0; for (size_t j = 0; j < boxes.size(); j++) { if (j >= 6 && j - 6 == i) continue; if (hit(b, boxes[j])) n++; } return n; };
    int lines = 3, pick = -1; Cand ch{};
    for (int L = 3; L >= 1 && pick < 0; L--) {
      int w = tw(&Nico14, t1); if (L >= 2) w = max(w, tw(&Nico14, t2)); if (L == 3) w = max(w, tw(&NicoItalic14, t3));
      for (int k = 0; k < 12 && pick < 0; k++) { Cand c = cand(L, k, w); if (inBounds(c.b) && !hits(c.b)) { pick = k; ch = c; lines = L; } }
    }
    bool placed = pick >= 0;
    if (!placed) {                                  // crowded: the in-bounds 1-line spot that overlaps least
      int w = tw(&Nico14, t1), bestHits = 1 << 30; lines = 1;
      for (int k = 0; k < 12; k++) { Cand c = cand(1, k, w); if (!inBounds(c.b)) continue; int h = hits(c.b); if (h < bestHits) { bestHits = h; ch = c; pick = k; } }
      if (pick < 0) { ch = cand(1, 0, w); pick = 0; }
    }
    if (placed || pick >= 0) boxes.push_back(ch.b);
    int dx = 0; { int w = ch.b.x1 - ch.b.x0 - 4; if (ch.x0 + w > 477) dx = 477 - (int)(ch.x0 + w); if (ch.x0 + dx < 3) dx = 3 - (int)ch.x0; }   // last resort: shift inside the radar
    auto line = [&](const GFXfont* f, int yy, const String& s, uint16_t col = INK) { int sw = tw(f, s); int xx = ch.anc == 0 ? (int)ch.x0 : ch.anc == 1 ? (int)(ch.lx - sw) : (int)(ch.lx - sw / 2.0f); putHalo(f, xx + dx, yy, s, 2, col); };
    int ly = (int)ch.ly;
    line(&Nico14, ly, t1);
    if (lines == 3) { line(&NicoItalic14, ly + 14, t3, GREY_D); line(&Nico14, ly + 28, t2); }
    else if (lines == 2) line(&Nico14, ly + 14, t2);
  }

  // overlays (design coordinates, radar view)
  box(9, 9, 134, 26); put(&Nico16, 18, 28, String("RADAR \xC2\xB7 ") + (int)lroundf(dU(cfg.radiusMi)) + (cfg.km ? " KM" : " MI"));
  box(14, 46, 40, 56);
  { float a = -cfg.heading * DEG_TO_RAD; int cx = 34, cy = 70; auto P = [&](float px, float py, int& ox, int& oy) { ox = cx + (int)lroundf(px * cosf(a) - py * sinf(a)); oy = cy + (int)lroundf(px * sinf(a) + py * cosf(a)); };
    int x0, y0, x1, y1, x2, y2, x3, y3, x4, y4; P(0, 12, x0, y0); P(0, -12, x1, y1); P(-6, -5, x2, y2); P(6, -5, x3, y3);
    for (int o = 0; o < 2; o++) { epd.drawLine(x0 + o, y0, x1 + o, y1, INK); epd.drawLine(x2 + o, y2, x1 + o, y1, INK); epd.drawLine(x3 + o, y3, x1 + o, y1, INK); } (void)x4; (void)y4; }
  put(&Nico14, 29, 94, "N");
  box(9, 445, 58, 26);
  bmp(ST[wifiOk ? ST_WIFI_OK : ST_WIFI_DOWN], 16, 450);
  bmp(ST[feed == Feed::Live ? ST_DATA_LIVE : feed == Feed::Stale ? ST_DATA_STALE : ST_DATA_NONE], 43, 449);
  box(150, 9, 320, 26);
  String banner = String("Updated ") + clock + " \xC2\xB7 " + nPlanes + (nPlanes == 1 ? " plane" : " planes");
  put(&Nico14, 460 - tw(&Nico14, banner), 28, banner);
}


// ---- window view: the frame hangs upright by a window and shows the sky as the window sees it ----
// /window/terrain.bin (800x480 skyline, contours, guides, compass; made once by tools/window_view.py) is the background.
// Planes are placed by compass bearing and height angle and drawn from pre-rendered 3D pictures in /sprites.bin (tools/bake_planes.py).
static const int W_EYE = 290, W_TOP = 34, W_YB = 426;              // y of eye level, of the top of the sky, of the compass strip
static float winY(float ang) {                                      // height angle -> pixel row; same curve as tools/window_view.py
  if (ang >= 0) return W_EYE - (W_EYE - W_TOP) * sqrtf(fminf(ang, 35.0f) / 35.0f);
  return W_EYE + (W_YB - W_EYE) * sqrtf(fminf(-ang, 14.0f) / 14.0f);
}
struct WinPeak { int16_t x, y; uint16_t hm; char nm[25]; };
static struct { bool tried = false, ok = false; uint16_t fov10 = 1750, head = 0; int16_t h0 = 0; uint8_t np = 0; WinPeak pk[6]; size_t bmp = 0; } wp;
static uint8_t* spTbl = nullptr; static int spN = 0; static bool spTried = false;
static const char* WIN_TERRAIN = "/window/terrain.bin"; static const char* WIN_SPRITES = "/sprites.bin";
static const int SP_YAWS = 13, SP_PITCH = 3, SP_SIZES = 5; static const int SP_LEN[SP_SIZES] = {36, 52, 72, 96, 120};

static void winLoad() {
  if (wp.tried) return; wp.tried = true;
  uint8_t h[220]; if (!storReadRect(WIN_TERRAIN, 0, 0, 0, 1, 0, sizeof h, h) || memcmp(h, "FFWT", 4) || h[4] != 1) return;
  wp.fov10 = h[5] | (h[6] << 8); wp.head = h[7] | (h[8] << 8); wp.h0 = (int16_t)(h[9] | (h[10] << 8)); wp.np = min<int>(h[11], 6);
  size_t o = 12;
  for (int i = 0; i < wp.np; i++) {
    if (o + 7 > sizeof h) { wp.np = i; break; }
    WinPeak& k = wp.pk[i]; k.x = (int16_t)(h[o] | (h[o + 1] << 8)); k.y = (int16_t)(h[o + 2] | (h[o + 3] << 8)); k.hm = h[o + 4] | (h[o + 5] << 8);
    int n = min<int>(h[o + 6], 24); if (o + 7 + n > sizeof h) { wp.np = i; break; } memcpy(k.nm, h + o + 7, n); k.nm[n] = 0; o += 7 + h[o + 6];
  }
  wp.bmp = o; wp.ok = true;
  if (!spTried) {                                                    // picture index: kept in memory, pictures are read one at a time
    spTried = true; uint8_t hd[6];
    if (storReadRect(WIN_SPRITES, 0, 0, 0, 1, 0, 6, hd) && !memcmp(hd, "FFSP", 4)) {
      int n = hd[4] | (hd[5] << 8); spTbl = (uint8_t*)malloc(8 * n);
      if (spTbl && storReadRect(WIN_SPRITES, 6, 0, 0, 1, 0, 8 * n, spTbl)) spN = n; else { free(spTbl); spTbl = nullptr; }
    }
  }
}
static int winClass(const Aircraft& a) {                            // 0 narrow, 1 wide, 2 quad, 3 jumbo (747), 4 prop, 5 bizjet
  struct M { const char* t; uint8_t c; };
  static const M tbl[] = {{"B763", 1}, {"B764", 1}, {"B772", 1}, {"B77W", 1}, {"B77L", 1}, {"B788", 1}, {"B789", 1}, {"B78X", 1}, {"A332", 1}, {"A333", 1}, {"A338", 1}, {"A339", 1}, {"A359", 1}, {"A35K", 1},
    {"A343", 2}, {"A345", 2}, {"A346", 2}, {"A388", 2}, {"B744", 3}, {"B748", 3}, {"AT76", 4}, {"AT75", 4}, {"AT72", 4}, {"DH8D", 4}, {"DH8C", 4}, {"BE36", 4}, {"C172", 4}, {"PC12", 4}, {"SR22", 4},
    {"GLF6", 5}, {"GLF5", 5}, {"GLF4", 5}, {"C56X", 5}, {"C25A", 5}, {"C680", 5}, {"LJ45", 5}, {"E55P", 5}, {"CL60", 5}, {"F900", 5}, {"FA7X", 5}, {"GL5T", 5}};
  for (auto& m : tbl) if (!strcmp(a.type, m.t)) return m.c;
  if (!strcmp(a.cat, "A5")) return 1; if (!strcmp(a.cat, "A1") || !strcmp(a.cat, "A7")) return 4;
  return 0;
}
static bool spGet(int cls, int yi, int pi, int si, uint8_t* buf, size_t cap, int& w, int& h) {
  if (!spTbl) return false;
  uint16_t key = ((cls * SP_YAWS + yi) * SP_PITCH + pi) * SP_SIZES + si; int lo = 0, hi = spN - 1;
  while (lo <= hi) {
    int m = (lo + hi) / 2; const uint8_t* e = spTbl + 8 * m; uint16_t k = e[0] | (e[1] << 8);
    if (k == key) {
      uint32_t off = e[2] | (e[3] << 8) | (e[4] << 16) | ((uint32_t)e[5] << 24); w = e[6]; h = e[7]; size_t n = (size_t)((w + 7) / 8) * h;
      return n <= cap && storReadRect(WIN_SPRITES, off, 0, 0, 1, 0, n, buf);
    }
    if (k < key) lo = m + 1; else hi = m - 1;
  }
  return false;
}
static String commas(int32_t v) { String t = String((long)(v < 0 ? -v : v)), o; int n = t.length(); for (int i = 0; i < n; i++) { if (i && (n - i) % 3 == 0) o += ","; o += t[i]; } return (v < 0 ? "-" : "") + o; }
struct WinP { uint8_t i; float dist, rel, el, yaw; int16_t x, y, px, py, w, h; };

static void drawWindow(Feed feed, bool wifiOk, const char* clock) {
  (void)feed; (void)wifiOk;
  // background: skyline + guides + compass, streamed from the SD card 16 rows at a time
  { uint8_t* rows = (uint8_t*)malloc(100 * 16); if (!rows) return;
    for (int y = 0; y < 480; y += 16) if (storReadRect(WIN_TERRAIN, wp.bmp, 100, y, 16, 0, 100, rows)) epd.drawBitmap(0, y, rows, 800, 16, INK);
    free(rows); }
  float fov = wp.fov10 / 10.0f, head = wp.head, cl = cosf(cfg.lat * DEG_TO_RAD);
  const float R = 6371000.0f;
  WinP* P = (WinP*)malloc(sizeof(WinP) * MAX_AIRCRAFT); int np = 0; if (!P) return;
  for (uint8_t i = 0; i < nPlanes; i++) {
    const Aircraft& a = planes[i];
    float dy = (a.lat - cfg.lat) * 111320.0f, dx = (a.lon - cfg.lon) * 111320.0f * cl, dist = hypotf(dx, dy); if (dist < 500) continue;
    float brg = fmodf(atan2f(dx, dy) * RAD_TO_DEG + 360, 360), rel = fmodf(brg - head + 540, 360) - 180;
    if (fabsf(rel) > fov / 2 + 4) continue;
    float el = atan2f(a.alt * 0.3048f - wp.h0 - dist * dist / (2 * R) * 0.87f, dist) * RAD_TO_DEG, yaw = fmodf(a.track - brg + 540, 360) - 180;
    P[np++] = WinP{i, dist, rel, el, yaw, (int16_t)(400 + rel / fov * 799), (int16_t)winY(el), 0, 0, 0, 0};
  }
  std::sort(P, P + np, [](const WinP& a, const WinP& b) { return a.dist > b.dist; });              // far first: near planes sit on top
  std::vector<Rect> taken = {{9, 9, 300, 36}, {500, 9, 791, 36}};
  uint8_t* sp = (uint8_t*)malloc(2048); uint8_t* mir = (uint8_t*)malloc(2048);
  for (int k = 0; k < np && sp && mir; k++) {
    WinP& q = P[k]; const Aircraft& a = planes[q.i];
    int yi = min<int>(SP_YAWS - 1, (int)lroundf(fabsf(q.yaw) / 15.0f)), pi = q.el < 7.5f ? 0 : q.el < 22.5f ? 1 : 2;
    float L = constrain(900000.0f / q.dist, 36.0f, 120.0f); int si = 0; for (int j = 1; j < SP_SIZES; j++) if (fabsf(L - SP_LEN[j]) < fabsf(L - SP_LEN[si])) si = j;
    int w = 0, h = 0; if (!spGet(winClass(a), yi, pi, si, sp, 2048, w, h)) {                         // no pictures on the card: a plain dot
      epd.fillCircle(q.x, q.y, 4, INK); q.px = q.x - 4; q.py = q.y - 4; q.w = q.h = 9; continue; }
    int stride = (w + 7) / 8; const uint8_t* img = sp;
    if (q.yaw < 0) {                                                                                   // flying left: mirror the picture
      memset(mir, 0, (size_t)stride * h);
      for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) if (sp[r * stride + (c >> 3)] & (0x80 >> (c & 7))) { int d = w - 1 - c; mir[r * stride + (d >> 3)] |= 0x80 >> (d & 7); }
      img = mir;
    }
    q.w = w; q.h = h; q.px = q.x - w / 2; q.py = q.y - h / 2; int rows = min<int>(h, W_YB - q.py); if (rows <= 0) continue;
    static const int8_t hx[8] = {-3, 3, 0, 0, -2, 2, -2, 2}, hy[8] = {0, 0, -3, 3, -2, -2, 2, 2};
    for (int j = 0; j < 8; j++) epd.drawBitmap(q.px + hx[j], q.py + hy[j], img, w, rows, PAPER);       // paper halo so the picture stays readable over the ridges
    epd.drawBitmap(q.px, q.py, img, w, rows, INK);
    taken.push_back({q.px, q.py, q.px + w, q.py + h});
  }
  free(sp); free(mir);
  // peak names: above the summit, slid up if a plane is in the way
  for (int i = 0; i < wp.np; i++) {
    const WinPeak& k = wp.pk[i]; String lab = String(k.nm) + " " + k.hm + " m"; int w = tw(&Nico14, lab);
    Rect b{}; for (int up = 22; up <= 88; up += 22) { b = {k.x - w / 2 - 3, k.y - up - 18, k.x + w / 2 + 3, k.y - up}; bool bad = false; for (auto& t : taken) if (hit(b, t)) { bad = true; break; } if (!bad) break; }
    epd.drawLine(k.x, k.y - 4, k.x, b.y1, INK); box(b.x0, b.y0, b.x1 - b.x0, b.y1 - b.y0); put(&Nico14, k.x - w / 2, b.y0 + 15, lab); taken.push_back(b);
  }
  // labels: nearest plane first so it gets the best spot; a label that finds no free spot is left out
  std::vector<int> ord; for (int k = 0; k < np; k++) ord.push_back(k);
  std::sort(ord.begin(), ord.end(), [&](int a, int b) { return P[a].dist < P[b].dist; });
  for (int k : ord) {
    const WinP& q = P[k]; const Aircraft& a = planes[q.i]; if (!q.w) continue;
    String t1 = String(a.fn[0] ? a.fn : a.hex) + (a.type[0] ? String(" ") + a.type : String("")), t2 = commas(a.alt) + " ft \xC2\xB7 " + String((int)lroundf(dU(q.dist / 1609.344f))) + " " + uName();
    Rect own{q.px, q.py, q.px + q.w, q.py + q.h}; bool placed = false; Rect box_{}; int px0 = 0, py0 = 0, lines = 2;
    for (lines = 2; lines >= 1 && !placed; lines--) {
      int w = lines == 2 ? max(tw(&Nico14, t1), tw(&Nico14, t2)) : tw(&Nico14, t1), hh = lines == 2 ? 29 : 15; float x = q.x, y = q.y;
      float sx[10] = {x - w / 2.0f, x - w / 2.0f, (float)own.x1 + 6, (float)own.x0 - w - 6, x - w / 2.0f, x - w / 2.0f, (float)own.x1 + 6, (float)own.x0 - w - 6, (float)own.x1 + 6, (float)own.x0 - w - 6};
      float sy[10] = {(float)own.y1 + 2, (float)own.y0 - hh - 3, y - hh / 2.0f, y - hh / 2.0f, (float)own.y1 + 2 + hh, (float)own.y0 - 2 * hh - 5, y + 10, y + 10, y - hh - 8, y - hh - 8};
      for (int j = 0; j < 10 && !placed; j++) {
        int lx = (int)constrain(sx[j], 4.0f, 796.0f - w), ly = (int)sy[j]; Rect b{lx - 2, ly - 1, lx + w + 2, ly + hh};
        if (b.y0 < 3 || b.y1 >= W_YB) continue; bool bad = false; for (auto& t : taken) if (hit(b, t) && !(t.x0 == own.x0 && t.y0 == own.y0 && t.x1 == own.x1 && t.y1 == own.y1)) { bad = true; break; }
        if (!bad) { placed = true; box_ = b; px0 = lx; py0 = ly; }
      }
      if (placed) break;
    }
    if (!placed) continue;
    int gap = max(max(box_.x0 - own.x1, own.x0 - box_.x1), max(box_.y0 - own.y1, own.y0 - box_.y1));
    if (gap > 5) { float cx = (box_.x0 + box_.x1) / 2.0f, cy = (box_.y0 + box_.y1) / 2.0f; for (int t = 0; t < 100; t += 4) epd.drawPixel((int)(cx + (q.x - cx) * t / 100.0f), (int)(cy + (q.y - cy) * t / 100.0f), INK); }
    epd.fillRect(box_.x0, box_.y0, box_.x1 - box_.x0 + 1, box_.y1 - box_.y0 + 1, PAPER);
    put(&Nico14, px0, py0 + 12, t1); if (lines == 2) put(&Nico14, px0, py0 + 26, t2);
    taken.push_back(box_);
  }
  free(P);
  // overlays
  box(9, 9, 291, 27); put(&Nico16, 18, 28, String("WINDOW \xC2\xB7 facing ") + (int)wp.head + "\xC2\xB0 \xC2\xB7 " + (int)wp.h0 + " m up");
  String ban = String("Updated ") + clock + " \xC2\xB7 " + np + (np == 1 ? " plane in view" : " planes in view"); int bw = tw(&Nico14, ban) + 22;
  box(791 - bw, 9, bw, 27); put(&Nico14, 791 - bw + 10, 28, ban);
}

#include "engview.h"
#include "diag.h"
void drawInit() { SPI.begin(EPD_SCK, SD_MISO, EPD_MOSI, EPD_CS); epd.init(115200, true, 2, false); epd.setRotation(0); }
void drawMessage(const char* l1, const char* l2) {
  epd.setFullWindow(); epd.firstPage();
  do { epd.fillScreen(PAPER); put(&Nico16, 40, 200, l1); put(&Nico14, 40, 232, l2); } while (epd.nextPage());
}
void drawForceFull() { partials = 0; }   // next redraw is a full (flashing) refresh, e.g. after switching view
void drawScreen(Feed feed, bool wifiOk) {
  bool full = (partials++ % FULL_REFRESH_EVERY) == 0;
  if (full) epd.setFullWindow(); else epd.setPartialWindow(0, 0, 800, 480);
  epd.firstPage();
  String clk = clockStr();
  if (cfg.view == 1) { winLoad(); if (!wp.ok) { drawMessage("Window view needs its terrain file", "Copy /window/terrain.bin to the SD card"); return; } }
  do { epd.fillScreen(PAPER); if (cfg.view == 1) drawWindow(feed, wifiOk, clk.c_str()); else if (cfg.view == 2) drawEngines(clk.c_str()); else { drawRadar(feed, wifiOk, clk.c_str()); drawList(clk.c_str()); }
#ifdef FF_DIAG   // build with -DFF_DIAG to print a status line (SD / download / feed / plane count) in the corner
    if (cfg.view != 2) { char dg[48]; snprintf(dg, sizeof dg, "sd:%d pack:%d http:%d n:%d", g_dSd, g_dPack, g_dHttp, (int)nPlanes); epd.setFont(nullptr); epd.setTextSize(1); epd.setTextColor(INK); epd.fillRect(626, 466, 170, 12, PAPER); epd.setCursor(632, 468); epd.print(dg); }
#endif
  } while (epd.nextPage());
}
