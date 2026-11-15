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
#include <GxEPD2_BW.h>
#include "clock.h"
#include <vector>
#include "fonts/Nico14.h"
#include "fonts/Nico16.h"
#include "fonts/NicoItalic14.h"
static GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> epd(GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));   // full 48 KB frame buffer, one page
static uint8_t partials = 0;
#define INK GxEPD_BLACK
#define PAPER GxEPD_WHITE

// ---- text helpers: NicoClean pixel fonts; UTF-8 arrows etc. map to the font's spare slots ----
static String fx(const String& s) { String o; for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (c == 0xE2 && i + 2 < s.length()) { uint8_t b = s[i + 1], d = s[i + 2]; if (b == 0x86 && d == 0x92) { o += (char)0x80; i += 2; continue; } if (b == 0x80 && d == 0x93) { o += (char)0x81; i += 2; continue; } }
    if (c == 0xC2 && i + 1 < s.length() && (uint8_t)s[i + 1] == 0xB7) { o += (char)0x82; i += 1; continue; }
    o += (char)c; } return o; }
static int tw(const GFXfont* f, const String& s) { epd.setTextSize(1); epd.setFont(f); int16_t x1, y1; uint16_t w, h; epd.getTextBounds(fx(s), 0, 0, &x1, &y1, &w, &h); return x1 + w; }
static int put(const GFXfont* f, int x, int y, const String& s, uint16_t col = INK) { epd.setTextSize(1); epd.setFont(f); epd.setTextColor(col); epd.setCursor(x, y); epd.print(fx(s)); return x + tw(f, s); }
static void putHalo(const GFXfont* f, int x, int y, const String& s, int r) {            // paper outline so text stays readable over the map
  for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (dx || dy) { if (abs(dx) + abs(dy) > r + 1) continue; put(f, x + dx, y + dy, s, PAPER); }
  put(f, x, y, s);
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
static float dU(float mi) { return cfg.km ? mi * 1.609344f : mi; }
static const char* uName() { return cfg.km ? "km" : "mi"; }
static const int LX = 483, RH = 88;
static void drawList(const char* clock) {
  epd.fillRect(LX, 0, 3, 480, INK);
  put(&Nico16, LX + 15, 26, String("TOP ") + cfg.rows + " NEAREST");
  put(&Nico14, 800 - 12 - tw(&Nico14, clock), 25, clock);
  hline(LX + 3, 40, 800 - LX - 3, 2);
  for (uint8_t i = 0; i < cfg.rows; i++) {
    int top = 42 + i * RH;
    if (i < nPlanes) {
      const Aircraft& a = planes[i];
      int x = put(&Nico16, LX + 15, top + 22, a.routeKnown ? String(a.from) + " \xE2\x86\x92 " + a.to : String("Route unknown"));
      put(&Nico14, x + 6, top + 22, String("(") + String(dU(a.distMi), 1) + " " + uName() + ")");
      drawLogo(a.cs, 800 - 12 - 44, top + 22);
      const Sprite& sp = spriteFor(a.type, a.cat);
      int x2 = put(&Nico14, LX + 15, top + 50, String(a.cs[0] ? a.cs : a.hex) + " " + a.type);
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
    else { epd.drawLine(px[i], py[i], px[i + 1], py[i + 1], INK); epd.drawLine(px[i] + 1, py[i], px[i + 1] + 1, py[i + 1], INK); epd.drawLine(px[i], py[i] + 1, px[i + 1], py[i + 1] + 1, INK); }
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
  for (uint8_t i = 0; i < nPlanes; i++) {
    const Aircraft& a = planes[i];
    String t1 = a.cs[0] ? a.cs : a.hex, t2 = String("\xE2\x86\x92 ") + (a.routeKnown ? a.to : "?"), t3 = a.type;
    int w = max(tw(&Nico14, t1), max(tw(&NicoItalic14, t3), tw(&Nico14, t2)));
    float x = X[i], y = Y[i]; int r = R_[i] + 1;
    struct C { int anc; float lx, ly; } c[12] = {{0, x + r, y - 8}, {1, x - r, y - 8}, {2, x, y - r - 36}, {2, x, y + r + 16}, {0, x + r, y - 30}, {1, x - r, y - 30}, {0, x + r, y + 12}, {1, x - r, y + 12}, {0, x + r, y - 46}, {1, x - r, y - 46}, {0, x + r, y + 28}, {1, x - r, y + 28}};
    int pick = -1; Rect pb{};
    for (int k = 0; k < 12 && pick < 0; k++) {
      float x0 = c[k].anc == 0 ? c[k].lx : c[k].anc == 1 ? c[k].lx - w : c[k].lx - w / 2.0f;
      Rect b{(int)(x0 - 2), (int)(c[k].ly - 12), (int)(x0 + w + 2), (int)(c[k].ly + 33)};
      if (b.x0 < 2 || b.x1 > 478 || b.y0 < 2 || b.y1 > 478) continue;
      bool bad = false; for (size_t j = 0; j < boxes.size(); j++) { if (j >= 6 && j - 6 == i) continue; if (hit(b, boxes[j])) { bad = true; break; } }
      if (!bad) { pick = k; pb = b; }
    }
    bool placed = pick >= 0; if (!placed) pick = 0;
    float x0 = c[pick].anc == 0 ? c[pick].lx : c[pick].anc == 1 ? c[pick].lx - w : c[pick].lx - w / 2.0f; int ly = (int)c[pick].ly;
    if (placed) boxes.push_back(pb);
    auto line = [&](const GFXfont* f, int yy, const String& s) { int sw = tw(f, s); int xx = c[pick].anc == 0 ? (int)x0 : c[pick].anc == 1 ? (int)(c[pick].lx - sw) : (int)(c[pick].lx - sw / 2.0f); putHalo(f, xx, yy, s, 2); };
    line(&Nico14, ly, t1); line(&NicoItalic14, ly + 14, t3); line(&Nico14, ly + 28, t2);
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

void drawInit() { SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS); epd.init(115200, true, 2, false); epd.setRotation(0); }
void drawMessage(const char* l1, const char* l2) {
  epd.setFullWindow(); epd.firstPage();
  do { epd.fillScreen(PAPER); put(&Nico16, 40, 200, l1); put(&Nico14, 40, 232, l2); } while (epd.nextPage());
}
void drawScreen(Feed feed, bool wifiOk) {
  bool full = (partials++ % FULL_REFRESH_EVERY) == 0;
  if (full) epd.setFullWindow(); else epd.setPartialWindow(0, 0, 800, 480);
  epd.firstPage();
  String clk = clockStr();
  do { epd.fillScreen(PAPER); drawRadar(feed, wifiOk, clk.c_str()); drawList(clk.c_str()); } while (epd.nextPage());
}
