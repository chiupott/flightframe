#include "terrain.h"
#include "config.h"
#include "draw.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SD.h>
#include <PNGdec.h>
#include <Adafruit_GFX.h>
#include "fonts/Nico14.h"
#include "fonts/Nico16.h"
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_x509_crt_bundle_bin_start");
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_src_x509_crt_bundle_bin_end");
#define SET_CA(c) (c).setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start)
#else
#define SET_CA(c) (c).setCACertBundle(rootca_crt_bundle_start)
#endif

static const int W = 800, H = 480, EYE = 290, TOP = 34, YB = 426, Z = 10;
static const float UP_MAX = 35.0f, DN_MAX = 14.0f, FOV = 110.0f, R_EARTH = 6371000.0f, MAX_M = 60000.0f, STEP = 1.02f, EYE_AGL = 6.0f;
static const char* TERRAIN = "/window/terrain.bin"; static const char* GEN = "/window/gen.txt"; static const char* DEM = "/window/dem.bin"; static const char* TMP = "/window/t.png";

static String genKey() { char b[48]; snprintf(b, sizeof b, "%.3f,%.3f,%u", cfg.lat, cfg.lon, (unsigned)cfg.heading); return b; }
bool terrainReady() {
  if (!SD.exists(TERRAIN)) return false;
  File g = SD.open(GEN); if (!g) return true;                       // made by hand: keep it
  String k = g.readString(); g.close(); return k == genKey();
}

// ---- elevation tiles (AWS Terrain Tiles, terrarium PNG) -> /window/dem.bin: int16 metres, 256x256 per tile, tiles in row order ----
static int tx0, ty0, nx, ny;
static File demOut;
static void* pOpen(const char* fn, int32_t* sz) { static File f; f = SD.open(fn); if (!f) return nullptr; *sz = f.size(); return &f; }
static void pClose(void* h) { if (h) ((File*)h)->close(); }
static int32_t pRead(PNGFILE* p, uint8_t* b, int32_t n) { return ((File*)p->fHandle)->read(b, n); }
static int32_t pSeek(PNGFILE* p, int32_t pos) { return ((File*)p->fHandle)->seek(pos) ? pos : -1; }
static int pDraw(PNGDRAW* d) {
  int bpp = d->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA ? 4 : 3; int16_t row[256];
  for (int x = 0; x < 256 && x < d->iWidth; x++) { const uint8_t* p = d->pPixels + x * bpp; row[x] = (int16_t)(((int)p[0] << 8) + p[1] - 32768 + (p[2] >= 128 ? 1 : 0)); }
  demOut.write((uint8_t*)row, 512); return 1;
}
static bool fetchTile(int tx, int ty) {
  String url = String("https://s3.amazonaws.com/elevation-tiles-prod/terrarium/") + Z + "/" + tx + "/" + ty + ".png";
  SD.remove(TMP); File f = SD.open(TMP, FILE_WRITE); if (!f) return false;
  { WiFiClientSecure c; SET_CA(c); HTTPClient h; h.setTimeout(20000); h.setUserAgent("FlightFrame/" FW_VERSION);
    if (!h.begin(c, url)) { f.close(); return false; }
    int code = h.GET(); bool ok = false;
    if (code == 200) ok = h.writeToStream(&f) > 0;
    h.end(); f.close(); Serial.printf("terrain: tile %d/%d http %d\n", tx, ty, code);
    if (!ok) return false; }
  PNG* png = new PNG(); if (!png) return false;
  int rc = png->open(TMP, pOpen, pClose, pRead, pSeek, pDraw); bool ok = false;
  if (rc == PNG_SUCCESS) { ok = png->decode(nullptr, 0) == PNG_SUCCESS; png->close(); } else Serial.printf("terrain: png open rc %d\n", rc);
  delete png; return ok;
}
static void world(float lat, float lon, float& X, float& Y) {
  float s = sinf(lat * DEG_TO_RAD), n = 262144.0f; X = (lon + 180.0f) / 360.0f * n; Y = (0.5f - logf((1 + s) / (1 - s)) / (4 * PI)) * n;
}

// ---- random access to the elevation mosaic with a small block cache ----
static File demIn; static const int NL = 512; static int32_t ctag[NL]; static uint8_t* cdat = nullptr;   // 512 lines x 64 bytes: neighbouring rays reuse each other's reads
static int16_t demAt(int ix, int iy) {
  int tx = ix >> 8, ty = iy >> 8; if (ix < 0 || iy < 0 || tx >= nx || ty >= ny) return 0;
  uint32_t off = (uint32_t)(ty * nx + tx) * 131072UL + ((uint32_t)((iy & 255) << 8) + (ix & 255)) * 2; uint32_t id = off >> 6, slot = (id * 2654435761u) >> 23 & (NL - 1);
  uint8_t* ln = cdat + slot * 64;
  if (ctag[slot] != (int32_t)id) { demIn.seek(id << 6); demIn.read(ln, 64); ctag[slot] = id; }
  return (int16_t)(ln[off & 63] | (ln[(off & 63) + 1] << 8));
}
static float elev(float lat, float lon) { float X, Y; world(lat, lon, X, Y); int16_t h = demAt((int)lroundf(X - tx0 * 256), (int)lroundf(Y - ty0 * 256)); return h > 0 ? h : 0; }

static float vy(float a) { if (a >= 0) return EYE - (EYE - TOP) * sqrtf(fminf(a, UP_MAX) / UP_MAX); return EYE + (YB - EYE) * sqrtf(fminf(-a, DN_MAX) / DN_MAX); }
struct Col { float H[426], D[426]; };
static int kOf(float h, float d) { float step = d < 12000 ? 25.0f : d < 30000 ? 50.0f : 100.0f; return (int)floorf(h / step); }
static int gOf(float d) { return (int)floorf(log2f(fmaxf(d, 250.0f) / 250.0f) * 2.4f); }
static void shadeColumn(GFXcanvas1& cv, int x, const Col& c, const Col* p) {   // same rules as tools/window_view.py shade()
  bool edgeAbove = false;
  for (int y = 0; y < YB; y++) {
    bool ok = !isnan(c.H[y]), land = ok && c.H[y] > 1.0f, sea = ok && !land; float h = ok ? c.H[y] : 0, d = ok ? c.D[y] : 0;
    bool okP = y > 0 && !isnan(c.H[y - 1]), landP = okP && c.H[y - 1] > 1.0f;
    bool far = d > 40000.0f, ink = false;
    if (land && landP && kOf(h, d) != kOf(c.H[y - 1], c.D[y - 1])) ink = true;
    if (p && land && !isnan(p->H[y]) && p->H[y] > 1.0f && kOf(h, d) != kOf(p->H[y], p->D[y])) ink = true;
    if (ink && (far || h <= 12.0f)) ink = false;
    if (sea && okP && !landP && gOf(d) != gOf(c.D[y - 1]) && ((x + gOf(d) * 7) % 11) < 6) ink = true;
    bool edge = ok && (y == 0 || !okP); if (edge) ink = true;
    if ((edge || edgeAbove) && land && !far) ink = true;
    edgeAbove = edge;
    if (ink) cv.drawPixel(x, y, 1);
  }
}
static void label(GFXcanvas1& cv, const GFXfont* f, int cx, int base, const String& s, bool deg = false) {
  cv.setFont(f); cv.setTextColor(1); int16_t x1, y1; uint16_t w, h; cv.getTextBounds(s, 0, 0, &x1, &y1, &w, &h); cv.setCursor(cx - w / 2, base); cv.print(s);
  if (deg) cv.drawCircle(cx - w / 2 + w + 3, base - 9, 2, 1);
}

bool terrainGenerate() {
  Serial.println("terrain: start");
  SD.mkdir("/window");
  // 1. which tiles: bounding box of the view fan
  float cl = cosf(cfg.lat * DEG_TO_RAD), lamin = cfg.lat, lamax = cfg.lat, lomin = cfg.lon, lomax = cfg.lon;
  for (int k = 0; k <= 22; k++) { float b = (cfg.heading + (k / 22.0f - 0.5f) * FOV) * DEG_TO_RAD, la = cfg.lat + MAX_M * cosf(b) / 111320.0f, lo = cfg.lon + MAX_M * sinf(b) / (111320.0f * cl);
    lamin = fminf(lamin, la); lamax = fmaxf(lamax, la); lomin = fminf(lomin, lo); lomax = fmaxf(lomax, lo); }
  float xa, ya, xb, yb; world(lamax, lomin, xa, ya); world(lamin, lomax, xb, yb);
  tx0 = (int)(xa / 256); ty0 = (int)(ya / 256); nx = (int)(xb / 256) - tx0 + 1; ny = (int)(yb / 256) - ty0 + 1;
  if (nx * ny > 40) { Serial.println("terrain: too many tiles"); return false; }
  // 2. download + decode
  SD.remove(DEM); demOut = SD.open(DEM, FILE_WRITE); if (!demOut) return false;
  int n = 0, total = nx * ny, fails = 0;
  for (int ty = 0; ty < ny; ty++) for (int tx = 0; tx < nx; tx++) {
    char m[40]; snprintf(m, sizeof m, "Downloading terrain %d of %d", ++n, total);
    if (n == 1 || n == total || n % 4 == 0) drawMessage("Building your window view", m);
    size_t before = demOut.position();
    if (!fetchTile(tx0 + tx, ty0 + ty)) { fails++; demOut.seek(before); }       // missing tile: flat sea-level instead
    size_t want = before + 131072UL; if (demOut.position() < want) { uint8_t z[512] = {0}; while (demOut.position() < want) demOut.write(z, 512); }
    if (fails > 3) { demOut.close(); SD.remove(DEM); return false; }
  }
  demOut.close(); SD.remove(TMP);
  // 3. ray-march + shade
  demIn = SD.open(DEM); if (!demIn) return false; cdat = (uint8_t*)malloc(NL * 64); if (!cdat) { demIn.close(); return false; } for (int i = 0; i < NL; i++) ctag[i] = -1;
  GFXcanvas1* cv = new GFXcanvas1(W, H); Col* cols = new Col[2];
  if (!cv || !cv->getBuffer() || !cols) { demIn.close(); delete cv; delete[] cols; return false; }
  cv->fillScreen(0);
  int steps = (int)(logf(MAX_M / 120.0f) / logf(STEP)); float* dist = new float[steps]; float dd = 120.0f; for (int i = 0; i < steps; i++, dd *= STEP) dist[i] = dd;
  float ground = elev(cfg.lat, cfg.lon), h0 = ground + EYE_AGL; Serial.printf("terrain: %dx%d tiles, ground %.0f m\n", nx, ny, ground);
  drawMessage("Building your window view", "Drawing the skyline...");
  int cur = 0; bool havePrev = false;
  for (int x = 0; x < W; x += 2) {
    Col& c = cols[cur];
    for (int y = 0; y < YB; y++) { c.H[y] = NAN; c.D[y] = NAN; }
    float b = (cfg.heading + ((float)x / (W - 1) - 0.5f) * FOV) * DEG_TO_RAD, sb = sinf(b), cb = cosf(b), prev = -99.0f;
    for (int i = 0; i < steps; i++) {
      float d = dist[i], h = elev(cfg.lat + d * cb / 111320.0f, cfg.lon + d * sb / (111320.0f * cl));
      float a = atan2f(h - h0 - d * d / (2 * R_EARTH) * 0.87f, d) * RAD_TO_DEG;
      if (a > prev) {
        int top = (int)vy(a); int bot = prev < -90 ? YB : (int)ceilf(vy(prev)); if (top < 0) top = 0; if (bot > YB) bot = YB;
        for (int y = top; y < bot; y++) { c.H[y] = h; c.D[y] = d; }
        prev = a;
      }
    }
    shadeColumn(*cv, x, c, havePrev ? &cols[cur ^ 1] : nullptr);
    if (x + 1 < W) shadeColumn(*cv, x + 1, c, &c);
    havePrev = true; cur ^= 1;
    if (x % 100 == 0) Serial.printf("terrain: column %d\n", x);
  }
  delete[] dist; delete[] cols; demIn.close(); free(cdat); cdat = nullptr; SD.remove(DEM);
  // 4. guides + compass strip (as in tools/window_view.py)
  static const int GUIDES[6] = {30, 20, 10, 5, 0, -5};
  for (int g = 0; g < 6; g++) { int y = (int)vy(GUIDES[g]); for (int x = 0; x < W; x += GUIDES[g] == 0 ? 6 : 10) cv->drawPixel(x, y, 1);
    cv->setFont(&Nico14); cv->setTextColor(1); cv->setCursor(8, y - 3); if (GUIDES[g] == 0) cv->print("eye level"); else { cv->print(GUIDES[g]); cv->drawCircle(cv->getCursorX() + 3, y - 12, 2, 1); } }
  cv->fillRect(0, YB, W, H - YB, 0); cv->fillRect(0, YB, W, 2, 1);
  static const char* NM[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  for (int deg = 0; deg < 360; deg += 5) {
    float rel = fmodf(deg - (float)cfg.heading + 540.0f, 360.0f) - 180.0f; if (fabsf(rel) > FOV / 2) continue;
    int x = (int)(W / 2 + rel / FOV * (W - 1)); int len = deg % 45 == 0 ? 14 : deg % 15 == 0 ? 8 : 4;
    cv->drawFastVLine(x, YB, len, 1); if (deg % 45 == 0) cv->drawFastVLine(x + 1, YB, len, 1);
    if (deg % 45 == 0) label(*cv, &Nico16, x, YB + 31, NM[deg / 45]); else if (deg % 15 == 0) label(*cv, &Nico14, x, YB + 36, String(deg));
  }
  // 5. write terrain.bin (header: FFWT, v1, fov*10, heading, eye height above sea, 0 peaks; then 800x480 1-bit, 1 = ink)
  SD.remove("/window/terrain.tmp"); File o = SD.open("/window/terrain.tmp", FILE_WRITE); if (!o) { delete cv; return false; }
  uint8_t hd[12] = {'F', 'F', 'W', 'T', 1, (uint8_t)(1100 & 255), (uint8_t)(1100 >> 8), (uint8_t)(cfg.heading & 255), (uint8_t)(cfg.heading >> 8), (uint8_t)((int)lroundf(h0) & 255), (uint8_t)(((int)lroundf(h0) >> 8) & 255), 0};
  bool ok = o.write(hd, 12) == 12 && o.write(cv->getBuffer(), 48000) == 48000; o.close(); delete cv;
  if (!ok) { SD.remove("/window/terrain.tmp"); return false; }
  SD.remove(TERRAIN); SD.rename("/window/terrain.tmp", TERRAIN);
  File g = SD.open(GEN, FILE_WRITE); if (g) { g.print(genKey()); g.close(); }
  Serial.println("terrain: done"); return true;
}
