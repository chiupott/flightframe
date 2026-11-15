#pragma once
// Engines view (cfg.view == 2): planes in range flow through a 3D-rendered turbofan into engine makers; table of engines and people below.
// Included from draw.cpp (uses its epd, put(), tw(), box(), commas(), dU(), uName()). Design notes: tools/bake_engine.py, README.
#include "engines.h"
#include "engine_img.h"
#include "fonts/Nico12.h"
#include "fonts/Nico13.h"
#include "fonts/Nico15.h"
#include "fonts/Nico17.h"

static bool engPat(uint8_t p, int x, int y) {                               // hatch tiles for the lanes
  switch (p) {
    case 0: return true;                          case 1: return (x + y) % 5 < 2;               case 2: return (((x - y) % 5) + 5) % 5 < 2;
    case 3: return (x & 3) < 2 && (y & 3) < 2;    case 4: return (y & 3) < 2;                   case 5: return x % 6 < 1 || y % 6 < 1;
    case 6: return (x & 3) < 2;                   case 7: return (x & 3) < 1 && (y & 3) < 1;    case 8: return (x + y) % 9 < 3;
    default: return (((x >> 2) + (y >> 2)) & 1) == 0;
  }
}
static void engPoly(const float* px, const float* py, int n, uint8_t pat) {      // scan-line fill with a hatch tile, then a 2 px outline
  float lo = 1e9f, hi = -1e9f; for (int i = 0; i < n; i++) { lo = min(lo, py[i]); hi = max(hi, py[i]); }
  for (int y = max(0, (int)ceilf(lo)); y <= min(479, (int)floorf(hi)); y++) {
    float xs[24]; int k = 0; float yc = y + 0.5f;
    for (int i = 0; i < n && k < 24; i++) { int j = (i + 1) % n; if ((py[i] <= yc && py[j] > yc) || (py[j] <= yc && py[i] > yc)) xs[k++] = px[i] + (yc - py[i]) * (px[j] - px[i]) / (py[j] - py[i]); }
    for (int a = 1; a < k; a++) { float v = xs[a]; int b = a - 1; while (b >= 0 && xs[b] > v) { xs[b + 1] = xs[b]; b--; } xs[b + 1] = v; }
    for (int a = 0; a + 1 < k; a += 2) {
      int x0 = max(0, (int)ceilf(xs[a])), x1 = min(799, (int)floorf(xs[a + 1])); if (x1 < x0) continue;
      epd.drawFastHLine(x0, y, x1 - x0 + 1, PAPER);
      if (pat == 0) epd.drawFastHLine(x0, y, x1 - x0 + 1, INK); else for (int x = x0; x <= x1; x++) if (engPat(pat, x, y)) epd.drawPixel(x, y, INK);
    }
  }
  for (int i = 0; i < n; i++) { int j = (i + 1) % n; bool flat = fabsf(px[j] - px[i]) >= fabsf(py[j] - py[i]);
    epd.drawLine(px[i], py[i], px[j], py[j], INK); epd.drawLine(px[i] + (flat ? 0 : 1), py[i] + (flat ? 1 : 0), px[j] + (flat ? 0 : 1), py[j] + (flat ? 1 : 0), INK); }
}
static void bez(float* ox, float* oy, float p0x, float p0y, float p1x, float p1y, float p2x, float p2y, float p3x, float p3y, int n) {
  for (int i = 0; i <= n; i++) { float t = (float)i / n, u = 1 - t; ox[i] = u * u * u * p0x + 3 * u * u * t * p1x + 3 * u * t * t * p2x + t * t * t * p3x; oy[i] = u * u * u * p0y + 3 * u * u * t * p1y + 3 * u * t * t * p2y + t * t * t * p3y; }
}
// one lane: both edges are cubic curves that start/end along the given tangents (this is what bends the flows to the engine's angle)
static void engRibbon(float a0x, float a0y, float a1x, float a1y, float tax, float tay, float b0x, float b0y, float b1x, float b1y, float tbx, float tby, float l1, float l2, uint8_t pat) {
  const int N = 36; float tx[N + 1], ty[N + 1], bx[N + 1], by[N + 1], X[2 * N + 2], Y[2 * N + 2];
  bez(tx, ty, a0x, a0y, a0x + tax * l1, a0y + tay * l1, b0x - tbx * l2, b0y - tby * l2, b0x, b0y, N);
  bez(bx, by, a1x, a1y, a1x + tax * l1, a1y + tay * l1, b1x - tbx * l2, b1y - tby * l2, b1x, b1y, N);
  for (int i = 0; i <= N; i++) { X[i] = tx[i]; Y[i] = ty[i]; X[N + 1 + i] = bx[N - i]; Y[N + 1 + i] = by[N - i]; }
  engPoly(X, Y, 2 * N + 2, pat);
}
// text whose baseline follows the engine axis (vertical shear, pixel-crisp); yc = vertical centre, anchor 'r' = right-aligned
static void engText(const GFXfont* f, int asc, int x, int yc, const String& s, float slope, char anchor = 'l') {
  String t = fx(s); int16_t x1, y1; uint16_t w, h; epd.setTextSize(1); epd.setFont(f); epd.getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
  const int B = 18; GFXcanvas1 cv(x1 + w + 2, 26); cv.fillScreen(0); cv.setFont(f); cv.setTextColor(1); cv.setCursor(0, B); cv.print(t);
  int x0 = anchor == 'r' ? x - (int)w : x;
  for (int cx = x1; cx < x1 + (int)w; cx++) { int off = (int)lroundf((cx - x1 - w / 2.0f) * slope);
    for (int cyy = 0; cyy < 26; cyy++) if (cv.getPixel(cx, cyy)) epd.drawPixel(x0 + cx - x1, yc - 14 + asc + off + (cyy - B), INK); }
}
static int engBig(const GFXfont* f, int x, int y, const String& s, int sc) {            // text blown up with square pixels; returns the height used
  String t = fx(s); GFXcanvas1 cv(150, 22); cv.fillScreen(0); cv.setFont(f); cv.setTextColor(1); cv.setCursor(0, 17); cv.print(t);
  int x0 = 999, x1 = -1, y0 = 999, y1 = -1; for (int j = 0; j < 22; j++) for (int i = 0; i < 150; i++) if (cv.getPixel(i, j)) { x0 = min(x0, i); x1 = max(x1, i); y0 = min(y0, j); y1 = max(y1, j); }
  if (x1 < 0) return 0;
  for (int j = y0; j <= y1; j++) for (int i = x0; i <= x1; i++) if (cv.getPixel(i, j)) epd.fillRect(x + (i - x0) * sc, y + (j - y0) * sc, sc, sc, INK);
  return (y1 - y0 + 1) * sc;
}
static void putRight(const GFXfont* f, int xr, int y, const String& s, uint16_t col = INK) { put(f, xr - tw(f, s), y, s, col); }

static void drawEngines(const char* clock) {
  Tally T; tallyPlanes(T);
  const int cyE = 166, exLeft = 190, eyTop = cyE - ENG_H / 2;
  const float cfx = exLeft + ENG_FRONT[0], cfy = eyTop + ENG_FRONT[1], crx = exLeft + ENG_REAR[0], cry = eyTop + ENG_REAR[1];
  float al = hypotf(crx - cfx, cry - cfy), axx = (crx - cfx) / al, axy = (cry - cfy) / al, SL = axy / axx * 1.35f;
  // ---- rows on the left: plane families, biggest first; more than nine families are folded into "Other" ----
  struct Row { const char* name; uint16_t planes, eng; uint8_t pat; };
  Row rows[9]; int nr = 0; { uint8_t ord[N_FAMS]; int nf = 0; for (uint8_t f = 0; f < N_FAMS; f++) if (T.famPlanes[f]) ord[nf++] = f;
    std::sort(ord, ord + nf, [&](uint8_t a, uint8_t b) { return T.famEng[a] != T.famEng[b] ? T.famEng[a] > T.famEng[b] : a < b; });
    static const uint8_t PATS[9] = {1, 2, 0, 8, 9, 3, 5, 4, 6};
    for (int i = 0; i < nf && nr < 9; i++) {
      if (nf > 9 && nr == 8) { Row o{"Other", 0, 0, 7}; for (int j = 8; j < nf; j++) { o.planes += T.famPlanes[ord[j]]; o.eng += T.famEng[ord[j]]; } rows[nr++] = o; break; }
      rows[nr++] = Row{FAM_NAME[ord[i]], T.famPlanes[ord[i]], T.famEng[ord[i]], PATS[nr]};
    } }
  uint8_t mk[N_MAKERS]; int nm = 0; for (uint8_t m = 0; m < N_MAKERS; m++) if (T.makerEng[m]) mk[nm++] = m;
  const float tot = T.engines;
  // ---- the flows ----
  if (tot > 0) {
    float k = 3.1f, ys[9][2], ysR[N_MAKERS][2]; int minL = 26, minR = 64;
    auto layout = [&](const uint16_t* v, int n, int mins, float kk, float top, float out[][2]) { float y = top; for (int i = 0; i < n; i++) { float h = max(v[i] * kk, (float)mins); out[i][0] = y + h / 2 - v[i] * kk / 2; out[i][1] = y + h / 2 + v[i] * kk / 2; y += h; } return y - top; };
    uint16_t lv[9], rv[N_MAKERS]; for (int i = 0; i < nr; i++) lv[i] = rows[i].eng; for (int i = 0; i < nm; i++) rv[i] = T.makerEng[mk[i]];
    float hl = layout(lv, nr, minL, k, 0, ys); while (hl > 262 && k > 0.4f) { k *= 0.93f; hl = layout(lv, nr, minL, k, 0, ys); }
    layout(lv, nr, minL, k, cyE - hl / 2 + 6, ys);
    float kr = 3.1f, hr = layout(rv, nm, minR, kr, 0, ysR); while (hr > 262 && kr > 0.4f) { kr *= 0.93f; hr = layout(rv, nm, minR, kr, 0, ysR); }
    layout(rv, nm, minR, kr, cyE - hr / 2, ysR);
    int yt = eyTop + ENG_RIM_Y0, yb = yt + ENG_RIM_N - 1; float span = (yb - yt) * 0.74f, ym = (yt + yb) / 2.0f;
    auto rimx = [&](float y) { int i = constrain((int)lroundf(y) - yt, 0, ENG_RIM_N - 1); return exLeft + (float)(int16_t)pgm_read_word(&ENG_RIMX[i]); };
    float tbx = 0.5f * axx + 1, tby = 0.5f * axy, tl = hypotf(tbx, tby); tbx /= tl; tby /= tl;
    float y = ym - span / 2;
    for (int i = 0; i < nr; i++) {
      float hh = rows[i].eng / tot * span;
      engRibbon(98, ys[i][0], 98, ys[i][1], 1, 0, rimx(y), y, rimx(y + hh), y + hh, tbx, tby, 62, 40, rows[i].pat); y += hh;
      float yy = (ys[i][0] + ys[i][1]) / 2;
      engText(&Nico17, 13, 8, yy + 4, rows[i].name, SL); engText(&Nico15, 12, 92, yy + 4, String(rows[i].planes), SL, 'r');
    }
    float px = -axy, py = axx, yy0 = -8; int exr = (int)crx;
    for (int j = 0; j < nm; j++) {
      float hh = T.makerEng[mk[j]] / tot * 16, mid = (ysR[j][0] + ysR[j][1]) / 2;
      uint8_t pat = cfg.hl == 0 ? 3 : (cfg.hl - 1 == mk[j] ? 0 : 7);
      engRibbon(crx + px * yy0 + axx * 2, cry + py * yy0 + axy * 2, crx + px * (yy0 + hh) + axx * 2, cry + py * (yy0 + hh) + axy * 2, axx, axy, exr + 96, ysR[j][0], exr + 96, ysR[j][1], 1, 0, 60, 50, pat); yy0 += hh;
      engText(&Nico13, 10, exr + 106, mid - 9, MAKER_NAME[mk[j]], SL); engText(&Nico12, 9, exr + 106, mid + 8, String(T.makerEng[mk[j]]) + " engines", SL);
    }
  } else { String m = String("No airliners within ") + (int)lroundf(dU(cfg.radiusMi)) + " " + uName(); put(&Nico16, 8, 150, m); put(&Nico14, 8, 172, "Nothing big is flying in range right now."); }
  // ---- the engine itself: clear what is under it (row by row), then ink ----
  { int stride = (ENG_W + 7) / 8;
    for (int r = 0; r < ENG_H; r++) { int lo = -1, hi = -1;
      for (int b = 0; b < stride; b++) { uint8_t v = pgm_read_byte(&ENG_BITS[r * stride + b]); if (!v) continue; if (lo < 0) { int c = 0; while (!(v & (0x80 >> c))) c++; lo = b * 8 + c; } int c = 7; while (!(v & (0x80 >> c))) c--; hi = b * 8 + c; }
      if (lo >= 0) epd.drawFastHLine(exLeft + lo, eyTop + r, hi - lo + 1, PAPER); }
    epd.drawBitmap(exLeft, eyTop, ENG_BITS, ENG_W, ENG_H, INK); }
  // ---- bottom third: engines and people ----
  epd.fillRect(0, 318, 800, 3, INK);
  put(&Nico16, 8, 340, "IN THE AIR"); put(&Nico14, 104, 341, String("within ") + (int)lroundf(dU(cfg.radiusMi)) + " " + uName());
  putRight(&Nico16, 792, 340, clock); putRight(&Nico14, 630, 339, "x = engines \xC2\xB7 number = people on board");
  static const int colx[4] = {8, 166, 324, 482}, cw = 150;
  for (uint8_t m = 0; m < N_MAKERS; m++) {
    int x = colx[m]; bool hlm = cfg.hl == m + 1;
    box(x, 350, cw + 1, 21); if (hlm) epd.fillRect(x, 350, cw + 1, 21, INK);
    uint16_t fg = hlm ? PAPER : INK;
    put(&Nico14, x + 6, 364, MAKER_NAME[m], fg); putRight(&Nico14, x + cw - 5, 364, commas(T.makerPeople[m]), fg);
    uint8_t ord[N_EMS]; int ne = 0; for (uint8_t e = 0; e < N_EMS; e++) if (EMS[e].maker == m && T.emEng[e]) ord[ne++] = e;
    std::sort(ord, ord + ne, [&](uint8_t a, uint8_t b) { return T.emEng[a] > T.emEng[b]; });
    for (int i = 0; i < ne && i < 5; i++) { int yy = 387 + i * 20;
      put(&Nico14, x + 6, yy, EMS[ord[i]].name); putRight(&Nico14, x + 101, yy, String("x") + T.emEng[ord[i]]); putRight(&Nico14, x + cw - 3, yy, commas(T.emPeople[ord[i]])); }
  }
  epd.fillRect(640, 326, 3, 151, INK);
  int bh = engBig(&Nico16, 656, 352, commas(T.people), 3);
  put(&Nico14, 656, 352 + bh + 19, "people in the air");
  String th = T.thrustK >= 1000 ? String(T.thrustK / 1000.0f, 1) + " million lbf" : commas(T.thrustK * 1000) + " lbf";
  put(&Nico14, 656, 352 + bh + 41, th); put(&Nico14, 656, 352 + bh + 59, "of thrust overhead");
}
