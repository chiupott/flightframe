#include "map.h"
#include "view.h"
#include "config.h"
#include "storage.h"
#include "clock.h"
#include <SD.h>
#include <vector>
#include <algorithm>
#include <math.h>
#include <string.h>
#define GREY_L 0xAD55   // light grey (sea)
#define GREY_D 0x528A   // dark grey (rain)
extern bool g_grey;
#define INK 0x0000
#define PAPER 0xFFFF
struct Edge { int16_t x0, y0, x1, y1; };                                   // 1/8 pixel units: half the memory of floats, so the whole coastline fits
static const float EQ = 8.0f;

// ---------- land ----------
static int16_t rd16(const uint8_t* p) { return (int16_t)(p[0] | (p[1] << 8)); }
static int32_t rd32(const uint8_t* p) { return (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)); }
// Adds one coastline segment to E, keeping only what can change the fill or show in the view square:
// segments wholly above/below it are dropped; ones wholly left/right are squashed to the edge so scanline parity stays right.
static const size_t EDGE_CAP = 5200;                                       // 41 KB
static void addEdge(std::vector<Edge>& E, float px, float py, float x, float y) {
  if (E.size() >= EDGE_CAP) return;                                      // never grow the vector (a doubling needs one big free block, and the heap has none to spare)
  if ((py < -3 && y < -3) || (py > VW + 3 && y > VW + 3)) return;
  if (px < -4 && x < -4) { px = x = -4; } else if (px > VW + 4 && x > VW + 4) { px = x = VW + 4; }
  E.push_back({(int16_t)lroundf(px * EQ), (int16_t)lroundf(py * EQ), (int16_t)lroundf(x * EQ), (int16_t)lroundf(y * EQ)});
}
static void landEdges(std::vector<Edge>& E) {
  File f = SD.open("/map/land.bin"); if (!f) return;
  uint8_t h[8]; if (f.read(h, 8) != 8) { f.close(); return; }
  if (!memcmp(h, "FFL2", 4)) {                                                   // north-west Europe, 1e-4 deg, streamed ring by ring (rings far from the view are skipped)
    uint32_t nr = (uint32_t)rd32(h + 4);
    float la[4], lo[4]; for (int k = 0; k < 4; k++) viewUnproj((k & 1) ? VW : 0, (k & 2) ? VW : 0, la[k], lo[k]);
    int32_t vminLo = (int32_t)((min(min(lo[0], lo[1]), min(lo[2], lo[3])) - 0.05f) * 1e4f), vmaxLo = (int32_t)((max(max(lo[0], lo[1]), max(lo[2], lo[3])) + 0.05f) * 1e4f);
    int32_t vminLa = (int32_t)((min(min(la[0], la[1]), min(la[2], la[3])) - 0.05f) * 1e4f), vmaxLa = (int32_t)((max(max(la[0], la[1]), max(la[2], la[3])) + 0.05f) * 1e4f);
    static uint8_t buf[512];
    for (uint32_t r = 0; r < nr; r++) {
      uint8_t rh[20]; if (f.read(rh, 20) != 20) break;
      uint32_t np = (uint32_t)rd32(rh); int32_t x0 = rd32(rh + 4), y0 = rd32(rh + 8), x1 = rd32(rh + 12), y1 = rd32(rh + 16);
      if (x1 < vminLo || x0 > vmaxLo || y1 < vminLa || y0 > vmaxLa) { f.seek(f.position() + (size_t)np * 8); continue; }
      float px = 0, py = 0; uint32_t i = 0;
      while (i < np) {
        uint32_t take = min<uint32_t>(np - i, sizeof buf / 8); if (f.read(buf, take * 8) != (int)(take * 8)) { f.close(); return; }
        for (uint32_t k = 0; k < take; k++, i++) {
          float x, y; viewProj(rd32(buf + k * 8 + 4) / 1e4f, rd32(buf + k * 8) / 1e4f, x, y);
          if (!i) { px = x; py = y; }
          else { float ex = x - px, ey = y - py; if (i == np - 1 || ex * ex + ey * ey >= 2.0f) { addEdge(E, px, py, x, y); px = x; py = y; } }   // skip sub-pixel steps: far fewer edges, same picture
        }
      }
    }
    f.close(); return;
  }
  if (memcmp(h, "FFL1", 4)) { f.close(); return; }                              // old UK + Ireland file, loaded whole
  f.close();
  size_t n; uint8_t* b = storLoad("/map/land.bin", n); if (!b) return;
  uint16_t nr = (uint16_t)rd16(b + 4); size_t p = 6;
  for (uint16_t r = 0; r < nr && p + 12 <= n; r++) {
    uint16_t np = (uint16_t)rd16(b + p); p += 12;
    float px = 0, py = 0;
    for (uint16_t i = 0; i < np && p + 4 <= n; i++, p += 4) {
      float lon = rd16(b + p) / 2000.0f - 12.0f, lat = rd16(b + p + 2) / 2500.0f + 49.0f, x, y;
      viewProj(lat, lon, x, y);
      if (i) addEdge(E, px, py, x, y);
      px = x; py = y;
    }
  }
  free(b);
}
static void fillLand(Adafruit_GFX& g, const std::vector<Edge>& E) {
  std::vector<float> xs;
  for (int y = 0; y < VW; y++) {
    float yc = y + 0.5f; xs.clear();
    for (const Edge& e : E) { float y0 = e.y0 / EQ, y1 = e.y1 / EQ; if ((y0 <= yc && y1 > yc) || (y1 <= yc && y0 > yc)) { float x0 = e.x0 / EQ, x1 = e.x1 / EQ; xs.push_back(x0 + (yc - y0) * (x1 - x0) / (y1 - y0)); } }
    std::sort(xs.begin(), xs.end());
    for (size_t i = 0; i + 1 < xs.size(); i += 2) {
      int a = max(0, (int)lroundf(xs[i])), c = min(VW, (int)lroundf(xs[i + 1]));
      if (c > a) g.fillRect(a, y, c - a, 1, PAPER);
    }
  }
}
static bool clipLine(float& x0, float& y0, float& x1, float& y1) {           // Liang-Barsky against the view square
  float dx = x1 - x0, dy = y1 - y0, t0 = 0, t1 = 1, p[4] = {-dx, dx, -dy, dy}, q[4] = {x0 + 2, VW + 2 - x0, y0 + 2, VW + 2 - y0};
  for (int i = 0; i < 4; i++) {
    if (p[i] == 0) { if (q[i] < 0) return false; }
    else { float r = q[i] / p[i]; if (p[i] < 0) { if (r > t1) return false; if (r > t0) t0 = r; } else { if (r < t0) return false; if (r < t1) t1 = r; } }
  }
  float nx0 = x0 + t0 * dx, ny0 = y0 + t0 * dy, nx1 = x0 + t1 * dx, ny1 = y0 + t1 * dy; x0 = nx0; y0 = ny0; x1 = nx1; y1 = ny1; return true;
}
static void outlineLand(Adafruit_GFX& g, const std::vector<Edge>& E) {
  for (const Edge& q : E) {
    float x0 = q.x0 / EQ, y0 = q.y0 / EQ, x1 = q.x1 / EQ, y1 = q.y1 / EQ;
    if (!clipLine(x0, y0, x1, y1)) continue;
    g.drawLine(x0, y0, x1, y1, INK); g.drawLine(x0 + 1, y0, x1 + 1, y1, INK); g.drawLine(x0, y0 + 1, x1, y1 + 1, INK);
  }
}

// ---------- rain: 1 bit/pixel mask at half-res z7 web-mercator tiles ----------
// File /map/rain.bin: "FFR2", tx0, ty0 (tile origin), w, h (le16, half-res px), unix time (le32), then h rows of (w+7)/8 bytes, MSB first.
// Only the rows and bytes the view needs are read, so the 60 KB UK-wide file never sits in RAM.
struct RainMask { int tx0 = 0, ty0 = 0, w = 0, h = 0, cu0 = 0, cv0 = 0, cw = 0, ch = 0; uint8_t* bits = nullptr; size_t stride = 0; };
static void worldPx(float lat, float lon, float& X, float& Y) {
  float s = sinf(lat * DEG_TO_RAD); X = (lon + 180) / 360 * 32768; Y = (0.5f - logf((1 + s) / (1 - s)) / (4 * PI)) * 32768;
}
static bool rainWet(const RainMask& m, float lat, float lon) {
  float Xw, Yw; worldPx(lat, lon, Xw, Yw);
  int u = (int)floorf((Xw - m.tx0 * 256) / 2) - m.cu0, v = (int)floorf((Yw - m.ty0 * 256) / 2) - m.cv0;
  if (u < 0 || v < 0 || u >= m.cw || v >= m.ch) return false;
  return m.bits[v * m.stride + (u >> 3)] & (0x80 >> (u & 7));
}
static const int CELL = 3, GN = VW / CELL;                                         // 160 x 160 cell grid
typedef uint8_t Grid[GN][GN];
static void morph(Grid in, Grid out, bool dilate) {
  for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) {
    int n = 0; for (int dj = -1; dj <= 1; dj++) for (int di = -1; di <= 1; di++) { int jj = j + dj, ii = i + di; n += (jj >= 0 && ii >= 0 && jj < GN && ii < GN) ? in[jj][ii] : 0; }
    out[j][i] = dilate ? (n > 0) : (n == 9);
  }
}
static bool loadRain(RainMask& m) {
  uint8_t h[14];
  if (!storReadRect("/map/rain.bin", 0, 14, 0, 1, 0, 14, h) || memcmp(h, "FFR2", 4)) return false;
  m.tx0 = h[4]; m.ty0 = h[5]; m.w = rd16(h + 6); m.h = rd16(h + 8);
  uint32_t ts = h[10] | (h[11] << 8) | (h[12] << 16) | ((uint32_t)h[13] << 24), now = clockNow();
  if (now && ts && now > ts + 90 * 60) return false;                              // radar older than 90 min (GitHub runs the 10-minute job about hourly): better no rain than wrong rain
  float umin = 1e9f, umax = -1e9f, vmin = 1e9f, vmax = -1e9f;
  for (int k = 0; k < 4; k++) {                                                   // bounding box of the radar view in mask pixels
    float la, lo, X, Y; viewUnproj((k & 1) ? VW : 0, (k & 2) ? VW : 0, la, lo); worldPx(la, lo, X, Y);
    float u = (X - m.tx0 * 256) / 2, v = (Y - m.ty0 * 256) / 2; umin = min(umin, u); umax = max(umax, u); vmin = min(vmin, v); vmax = max(vmax, v);
  }
  int u0 = max(0, (int)floorf(umin) - 2), u1 = min(m.w - 1, (int)ceilf(umax) + 2), v0 = max(0, (int)floorf(vmin) - 2), v1 = min(m.h - 1, (int)ceilf(vmax) + 2);
  if (u1 < u0 || v1 < v0) return false;                                           // frame is outside the mask: no rain data here
  m.cu0 = (u0 / 8) * 8; m.cv0 = v0; m.cw = u1 - m.cu0 + 1; m.ch = v1 - v0 + 1; m.stride = (m.cw + 7) / 8;
  m.bits = (uint8_t*)malloc(m.stride * m.ch); if (!m.bits) return false;
  if (!storReadRect("/map/rain.bin", 14, (m.w + 7) / 8, m.cv0, m.ch, m.cu0 / 8, (int)m.stride, m.bits)) { free(m.bits); m.bits = nullptr; return false; }
  return true;
}
static void drawRain(Adafruit_GFX& g) {
  RainMask m; if (!loadRain(m)) return;
  uint8_t* raw = m.bits;
  // four 25 KB grids on the heap for the duration of the draw only (static would eat the ESP32's 124 KB of static RAM)
  uint8_t* mem = (uint8_t*)malloc(4 * GN * GN);
  if (!mem) { free(raw); return; }
  Grid& A = *(Grid*)mem; Grid& B = *(Grid*)(mem + GN * GN); Grid& G = *(Grid*)(mem + 2 * GN * GN); Grid& H = *(Grid*)(mem + 3 * GN * GN);   // G,H: 0..255 fixed point
  for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) { float la, lo; viewUnproj(i * CELL + 1.5f, j * CELL + 1.5f, la, lo); A[j][i] = rainWet(m, la, lo); }
  morph(A, B, true); morph(B, A, false);                                          // closing: fill pinholes
  morph(A, B, false); morph(B, A, true);                                          // opening: drop specks
  for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) G[j][i] = A[j][i] ? 255 : 0;
  for (int pass = 0; pass < 2; pass++) {                                           // [1 2 1] blur twice -> smooth contour
    for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) H[j][i] = (G[j][max(i - 1, 0)] + 2 * G[j][i] + G[j][min(i + 1, GN - 1)] + 2) / 4;
    for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) G[j][i] = (H[max(j - 1, 0)][i] + 2 * H[j][i] + H[min(j + 1, GN - 1)][i] + 2) / 4;
  }
  if (g_grey) {
  for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) if (G[j][i] > 127) g.fillRect(i * CELL, j * CELL, CELL, CELL, GREY_D);   // solid dark grey rain, smooth edge from the blurred grid
  } else {
  for (int y = 0; y < VW; y++) for (int x = 0; x < VW; x++) {                      // dot fill: two staggered 2x2 dots on an 8x8 grid
    if (!A[y / CELL][x / CELL]) continue; int a = x & 7, b = y & 7;
    if ((a == 1 || a == 2) && (b == 1 || b == 2)) g.drawPixel(x, y, INK); else if ((a == 5 || a == 6) && (b == 5 || b == 6)) g.drawPixel(x, y, INK);
  }
  // dotted outer edge: marching squares at 0.5, a dot every ~6 px along the contour
  const float T = 0.5f, SP = 6.0f; float next = 0;
  for (int j = 0; j + 1 < GN; j++) for (int i = 0; i + 1 < GN; i++) {
    float a = G[j][i] / 255.0f, b = G[j][i + 1] / 255.0f, c = G[j + 1][i + 1] / 255.0f, d = G[j + 1][i] / 255.0f;
    int idx = (a > T) * 8 + (b > T) * 4 + (c > T) * 2 + (d > T);
    if (idx == 0 || idx == 15) continue;
    float x0 = (i + 0.5f) * CELL, y0 = (j + 0.5f) * CELL;
    auto lerp = [&](float u, float v) { return (T - u) / (v - u); };
    float tx = x0 + lerp(a, b) * CELL, ty = y0;                                    // top edge
    float rx = x0 + CELL, ry = y0 + lerp(b, c) * CELL;                             // right
    float bx = x0 + lerp(d, c) * CELL, by = y0 + CELL;                             // bottom
    float lx = x0, ly = y0 + lerp(a, d) * CELL;                                    // left
    float S[4][4]; int ns = 0; auto seg = [&](float ax, float ay, float bx2, float by2) { S[ns][0] = ax; S[ns][1] = ay; S[ns][2] = bx2; S[ns][3] = by2; ns++; };
    switch (idx) {
      case 1: case 14: seg(lx, ly, bx, by); break;   case 2: case 13: seg(bx, by, rx, ry); break;
      case 3: case 12: seg(lx, ly, rx, ry); break;   case 4: case 11: seg(tx, ty, rx, ry); break;
      case 6: case 9: seg(tx, ty, bx, by); break;    case 7: case 8: seg(lx, ly, tx, ty); break;
      case 5: seg(tx, ty, rx, ry); seg(lx, ly, bx, by); break;   case 10: seg(lx, ly, tx, ty); seg(bx, by, rx, ry); break;
    }
    for (int k = 0; k < ns; k++) {
      float dx = S[k][2] - S[k][0], dy = S[k][3] - S[k][1], L = hypotf(dx, dy);
      while (next <= L) { float t = L > 0 ? next / L : 0; g.fillRect((int)(S[k][0] + dx * t), (int)(S[k][1] + dy * t), 2, 2, INK); next += SP; }
      next -= L;
    }
  }
  }
  free(mem); free(raw);
}

void mapDraw(Adafruit_GFX& g) {
  if (g_grey) g.fillRect(0, 0, VW, VW, GREY_L);                                                           // sea: light grey, land stays white
  else for (int y = 3; y < VW; y += 6) for (int x = 0; x < VW; x++) if ((x & 7) < 3) g.drawPixel(x, y, INK);   // B/W radar: sea = 3-on 5-off dashes every 6 rows
  std::vector<Edge> E; E.reserve(EDGE_CAP); landEdges(E); Serial.printf("map edges %u\n", (unsigned)E.size());
  fillLand(g, E);
  if (cfg.rain) drawRain(g);
  outlineLand(g, E);
}

// ---------- airfields ----------
struct AptRaw { char code[5]; bool large; uint8_t n; float la[6][2], lo[6][2]; };
static AptRaw aptCache[10]; static int aptCount = 0; static float aptLat = 1e9f, aptLon = 1e9f, aptRad = -1; static bool aptReady = false;
static void aptLoad() {
  aptCount = 0; aptLat = cfg.lat; aptLon = cfg.lon; aptRad = cfg.radiusMi; aptReady = true;
  File f = SD.open("/map/airports.bin"); if (!f) return;
  uint8_t h[8]; if (f.read(h, 8) != 8 || memcmp(h, "FFA1", 4)) { f.close(); return; }
  uint32_t n = (uint32_t)rd32(h + 4); float cl = cosf(cfg.lat * DEG_TO_RAD), lim = cfg.radiusMi * 1.4f + 3.0f;
  for (uint32_t i = 0; i < n; i++) {
    uint8_t r[14]; if (f.read(r, 14) != 14) break;
    float la = rd32(r) / 1e4f, lo = rd32(r + 4) / 1e4f, dy = (la - cfg.lat) * 69.05f, dx = (lo - cfg.lon) * 69.17f * cl; uint8_t nr = r[13];
    if (dx * dx + dy * dy > lim * lim) { f.seek(f.position() + (size_t)nr * 16); continue; }
    AptRaw a; memcpy(a.code, r + 8, 4); a.code[4] = 0; a.large = r[12] & 1; a.n = min<uint8_t>(nr, 6);
    for (uint8_t k = 0; k < nr; k++) { uint8_t q[16]; if (f.read(q, 16) != 16) { f.close(); return; } if (k < 6) { a.la[k][0] = rd32(q) / 1e4f; a.lo[k][0] = rd32(q + 4) / 1e4f; a.la[k][1] = rd32(q + 8) / 1e4f; a.lo[k][1] = rd32(q + 12) / 1e4f; } }
    if (aptCount < 10) aptCache[aptCount++] = a;
    else if (a.large) for (int k = 0; k < 10; k++) if (!aptCache[k].large) { aptCache[k] = a; break; }            // keep large airports when there are too many
  }
  f.close();
}
int aptList(Apt* out, int maxN) {
  if (!aptReady || aptLat != cfg.lat || aptLon != cfg.lon || aptRad != cfg.radiusMi) aptLoad();
  int m = 0;
  for (int i = 0; i < aptCount && m < maxN; i++) {
    const AptRaw& a = aptCache[i]; Apt& o = out[m]; memcpy(o.code, a.code, 5); o.large = a.large; o.n = a.n; float sx = 0, sy = 0;
    for (int k = 0; k < a.n; k++) { viewProj(a.la[k][0], a.lo[k][0], o.r[k].x0, o.r[k].y0); viewProj(a.la[k][1], a.lo[k][1], o.r[k].x1, o.r[k].y1); sx += o.r[k].x0 + o.r[k].x1; sy += o.r[k].y0 + o.r[k].y1; }
    o.cx = sx / (2 * a.n); o.cy = sy / (2 * a.n);
    for (int k = 0; k < a.n; k++) { o.r[k].x0 = o.cx + (o.r[k].x0 - o.cx) * 1.5f; o.r[k].y0 = o.cy + (o.r[k].y0 - o.cy) * 1.5f; o.r[k].x1 = o.cx + (o.r[k].x1 - o.cx) * 1.5f; o.r[k].y1 = o.cy + (o.r[k].y1 - o.cy) * 1.5f; }
    if (o.cx < -12 || o.cx > VW + 12 || o.cy < -12 || o.cy > VW + 12) continue;
    m++;
  }
  return m;
}
