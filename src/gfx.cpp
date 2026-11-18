#include "gfx.h"
#include "fonts.h"
#include <SPI.h>
#include <new>
// Shared drawing layer: the two panel drivers, page loop, text helpers (pixel + anti-aliased), small primitives.
#ifdef FF_GREY
static EpdBW* ebp = nullptr;                                                   // B/W driver (48 KB buffer): only exists while the B/W radar is on screen, so the grey views keep that RAM free
#define eb (*ebp)
#else
EpdBW eb(GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));                    // B/W build: always there
#endif
#ifdef FF_GREY
EpdGrey eg;                                                                                     // 4-grey driver: full-screen flash every redraw
Adafruit_GFX* G = &eg;                                                                          // whichever driver the current view draws with
#endif
uint8_t partials = 0;
#ifdef FF_GREY
bool g_grey = true;      // false while the B/W radar view is drawn: greys become black, sea/rain go back to patterns, text goes back to the pixel font
#else
bool g_grey = false;
#endif

// ---- text helpers: NicoClean pixel fonts; UTF-8 arrows etc. map to the font's spare slots ----
String fx(const String& s) { String o; for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (c == 0xE2 && i + 2 < s.length()) { uint8_t b = s[i + 1], d = s[i + 2]; if (b == 0x86 && d == 0x92) { o += (char)0x80; i += 2; continue; } if (b == 0x80 && d == 0x93) { o += (char)0x81; i += 2; continue; } }
    if (c == 0xC2 && i + 1 < s.length() && (uint8_t)s[i + 1] == 0xB7) { o += (char)0x82; i += 1; continue; }
    if (g_grey && c == 0xC2 && i + 1 < s.length() && (uint8_t)s[i + 1] == 0xB0) { o += (char)0x7F; i += 1; continue; }     // degree sign (anti-aliased fonts have it)
    o += (char)c; } return o; }
#ifdef FF_GREY
// anti-aliased text (grey views): each pixel font has an AA twin with the same cap height; glyph pixels are 2-bit coverage
static const AFont* aaFor(const GFXfont* f) {
  if (!g_grey) return nullptr;
  if (f == &Nico12) return &AA12; if (f == &Nico13) return &AA13; if (f == &Nico14) return &AA14; if (f == &Nico15) return &AA15;
  if (f == &Nico16) return &AA16; if (f == &Nico17) return &AA17; if (f == &NicoItalic14) return &AAI14; return nullptr;
}
int aaWidth(const AFont* a, const String& t) { int w = 0; for (size_t i = 0; i < t.length(); i++) { uint8_t c = t[i]; if (c >= a->first && c <= a->last) w += a->g[c - a->first].adv; } return w; }
int aaDraw(const AFont* a, int x, int y, const String& t, uint16_t col, bool solid) {
  int pen = x;
  for (size_t i = 0; i < t.length(); i++) {
    uint8_t ch = t[i]; if (ch < a->first || ch > a->last) continue;
    const AGlyph& g = a->g[ch - a->first]; int idx = 0;
    for (int r = 0; r < g.h; r++) for (int c = 0; c < g.w; c++, idx++) {
      int q = (a->bits[g.off + (idx >> 2)] >> (6 - 2 * (idx & 3))) & 3; if (!q) continue;
      uint16_t k = solid ? col : col == 0xFFFF ? (q == 3 ? 0xFFFF : q == 2 ? 0xAD55 : 0x528A) : col == 0x0000 ? (q == 3 ? 0x0000 : q == 2 ? 0x528A : 0xAD55) : (q >= 2 ? col : 0xAD55);
      epd.drawPixel(pen + g.xo + c, y + g.yo + r, k);
    }
    pen += g.adv;
  }
  return pen;
}
#endif
int tw(const GFXfont* f, const String& s) {
#ifdef FF_GREY
  if (const AFont* a = aaFor(f)) return aaWidth(a, fx(s));
#endif
  epd.setTextSize(1); epd.setFont(f); int16_t x1, y1; uint16_t w, h; epd.getTextBounds(fx(s), 0, 0, &x1, &y1, &w, &h); return x1 + w; }
int put(const GFXfont* f, int x, int y, const String& s, uint16_t col) {
#ifdef FF_GREY
  if (const AFont* a = aaFor(f)) return aaDraw(a, x, y, fx(s), col, false);
#endif
  epd.setTextSize(1); epd.setFont(f); epd.setTextColor(col); epd.setCursor(x, y); epd.print(fx(s)); return x + tw(f, s); }
void putHalo(const GFXfont* f, int x, int y, const String& s, int r, uint16_t col) {            // paper outline so text stays readable over the map
#ifdef FF_GREY
  if (const AFont* a = aaFor(f)) { String t = fx(s);
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (dx || dy) { if (abs(dx) + abs(dy) > r + 1) continue; aaDraw(a, x + dx, y + dy, t, PAPER, true); }
    aaDraw(a, x, y, t, col, false); return; }
#endif
  for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (dx || dy) { if (abs(dx) + abs(dy) > r + 1) continue; put(f, x + dx, y + dy, s, PAPER); }
  put(f, x, y, s, col);
}
void hline(int x, int y, int w, int th) { epd.fillRect(x, y, w, th, INK); }
void box(int x, int y, int w, int h) { epd.fillRect(x, y, w, h, PAPER); epd.drawRect(x, y, w, h, INK); epd.drawRect(x + 1, y + 1, w - 2, h - 2, INK); }
void bmp(const Bmp1& b, int x, int y) { int stride = (b.w + 7) / 8; for (int r = 0; r < b.h; r++) for (int c = 0; c < b.w; c++) if (b.bits[r * stride + (c >> 3)] & (0x80 >> (c & 7))) epd.drawPixel(x + c, y + r, INK); }

float dU(float mi) { return cfg.km ? mi * 1.609344f : cfg.nm ? mi * 0.868976f : mi; }
const char* uName() { return cfg.km ? "km" : cfg.nm ? "nm" : "mi"; }
String commas(int32_t v) { String t = String((long)(v < 0 ? -v : v)), o; int n = t.length(); for (int i = 0; i < n; i++) { if (i && (n - i) % 3 == 0) o += ","; o += t[i]; } return (v < 0 ? "-" : "") + o; }
static uint8_t lastDrv = 0;                                                  // 0 none, 1 black-and-white driver, 2 grey driver
void pageBegin(bool bw, bool full) {                                  // start a picture on the driver this view needs; switching driver forces a full refresh
#ifdef FF_GREY
  if (!bw) { if (ebp) { delete ebp; ebp = nullptr; } G = &eg; lastDrv = 2; eg.setFullWindow(); eg.firstPage(); return; }
  if (!ebp) ebp = new (std::nothrow) EpdBW(GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
  if (!ebp) { cfg.bw = false; g_grey = true; G = &eg; lastDrv = 2; eg.setFullWindow(); eg.firstPage(); return; }   // not enough free RAM for the B/W driver: show the grey radar instead
  G = ebp; lastDrv = lastDrv == 1 ? 1 : 0;
#endif
  if (lastDrv != 1) { eb.init(115200, true, 2, false); eb.setRotation(0); lastDrv = 1; full = true; partials = 1; }
  if (full) eb.setFullWindow(); else eb.setPartialWindow(0, 0, 800, 480);
  eb.firstPage();
}
bool pageNext() {
#ifdef FF_GREY
  if (G == &eg) return eg.nextPage();
#endif
  return eb.nextPage();
}
void drawInit() { SPI.begin(EPD_SCK, SD_MISO, EPD_MOSI, EPD_CS);
#ifdef FF_GREY
  eg.init(); eg.setRotation(0);
#endif
}
void drawMessage(const char* l1, const char* l2) {
#ifdef FF_GREY
  g_grey = true; pageBegin(false, true);
#else
  pageBegin(true, true);
#endif
  do { epd.fillScreen(PAPER); put(&Nico16, 40, 200, l1); put(&Nico14, 40, 232, l2); } while (pageNext());
}

void drawProgress(const char* title, const char* line, int pct, int etaSec) {
  pct = constrain(pct, 0, 100);
#ifdef FF_GREY
  g_grey = true; pageBegin(false, true);
#else
  pageBegin(true, true);
#endif
  String eta = etaSec < 0 ? String("") : etaSec < 90 ? String("About a minute left") : String("About ") + String((etaSec + 30) / 60) + " minutes left";
  do {
    epd.fillScreen(PAPER);
    put(&Nico17, 40, 170, title); put(&Nico14, 40, 204, line, GREY_D);
    epd.fillRoundRect(40, 240, 720, 30, 15, GREY_L); if (pct > 0) epd.fillRoundRect(40, 240, max(30, 720 * pct / 100), 30, 15, INK);
    put(&Nico16, 40, 312, String(pct) + "%"); if (eta.length()) put(&Nico14, 760 - tw(&Nico14, eta), 310, eta, GREY_D);
    put(&Nico14, 40, 450, "This only happens once for each place and direction.", GREY_D);
  } while (pageNext());
}
