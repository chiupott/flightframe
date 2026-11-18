#include "gfx.h"
#include "views.h"
#include "fonts.h"
static const int W_EYE = 290, W_TOP = 34, W_YB = 426;              // y of eye level, of the top of the sky, of the compass strip
static float winY(float ang) {                                      // height angle -> pixel row; same curve as tools/window_view.py
  if (ang >= 0) return W_EYE - (W_EYE - W_TOP) * sqrtf(fminf(ang, 35.0f) / 35.0f);
  return W_EYE + (W_YB - W_EYE) * sqrtf(fminf(-ang, 14.0f) / 14.0f);
}
struct WinPeak { int16_t x, y; uint16_t hm; char nm[25]; };
static struct { bool tried = false, ok = false, v2 = false; uint16_t fov10 = 1750, head = 0; int16_t h0 = 0; uint8_t np = 0; WinPeak pk[6]; size_t bmp = 0; } wp;
static uint8_t* spTbl = nullptr; static int spN = 0; static bool spTried = false;
static const char* WIN_TERRAIN = "/window/terrain.bin"; static const char* WIN_SPRITES = "/sprites.bin";
static const int SP_YAWS = 13, SP_PITCH = 3, SP_SIZES = 5; static const int SP_LEN[SP_SIZES] = {36, 52, 72, 96, 120};

void winLoad() {
  if (wp.tried) return; wp.tried = true;
  uint8_t h[220]; if (!storReadRect(WIN_TERRAIN, 0, 0, 0, 1, 0, sizeof h, h) || memcmp(h, "FFWT", 4) || (h[4] != 1 && h[4] != 2)) return;
  wp.v2 = h[4] == 2;
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
struct WinP { uint8_t i; float dist, rel, el, yaw; int16_t x, y, px, py, w, h; };

void drawWindow(Feed feed, bool wifiOk, const char* clock) {
  (void)feed; (void)wifiOk;
  // background: skyline + guides + compass, streamed from the SD card 16 rows at a time
  { uint8_t* rows = (uint8_t*)malloc(1600); if (!rows) return;
    for (int y = 0; y < 480; y += 16) {
      if (!wp.v2) { if (storReadRect(WIN_TERRAIN, wp.bmp, 100, y, 16, 0, 100, rows)) epd.drawBitmap(0, y, rows, 800, 16, INK); continue; }
      for (int h = 0; h < 2; h++) {                                                  // 4-grey skyline: 8 rows (1600 bytes) at a time
        if (!storReadRect(WIN_TERRAIN, wp.bmp, 200, y + h * 8, 8, 0, 200, rows)) break;
        for (int r = 0; r < 8; r++) for (int bx = 0; bx < 200; bx++) { uint8_t v = rows[r * 200 + bx]; if (!v) continue;
          for (int k = 0; k < 4; k++) { int q = (v >> (6 - 2 * k)) & 3; if (!q) continue;
            if (g_grey) epd.drawPixel(bx * 4 + k, y + h * 8 + r, q == 3 ? INK : q == 2 ? GREY_D : GREY_L); else if (q >= 2) epd.drawPixel(bx * 4 + k, y + h * 8 + r, INK); } } } }
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


bool winReady() { return wp.ok; }
void winRetry() { wp.tried = false; wp.ok = false; }
