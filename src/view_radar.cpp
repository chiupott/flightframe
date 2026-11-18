#include "gfx.h"
#include "views.h"
#include "fonts.h"
static void drawAirports();
// airline logo from the SD card (/logos/<ICAO>.lg); airline = first 3 letters of an airline-style callsign (3 letters + digit)
static bool airlineOf(const char* cs, char* out) { if (strlen(cs) < 4 || !isalpha(cs[0]) || !isalpha(cs[1]) || !isalpha(cs[2]) || !isdigit(cs[3])) return false; memcpy(out, cs, 3); out[3] = 0; for (int i = 0; i < 3; i++) out[i] = toupper(out[i]); return true; }
static void drawLogo(const char* cs, int x, int y) {
  char al[4]; if (!airlineOf(cs, al)) return;
  size_t n = 0; uint8_t* b = storLoad((String("/logos/") + al + ".lg").c_str(), n);
  if (b && n > 2 && (b[1] & 0x80)) {                                               // 4-grey mark (tools/bake_logos2.py): 2 bits per pixel; the B/W radar keeps the two darker levels
    int w = b[0], h = b[1] & 0x7F, stride = (w + 3) / 4;
    for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) { int q = (b[2 + r * stride + (c >> 2)] >> (6 - 2 * (c & 3))) & 3; if (q) { if (g_grey) epd.drawPixel(x + c, y + r, q == 3 ? INK : q == 2 ? GREY_D : GREY_L); else if (q >= 2) epd.drawPixel(x + c, y + r, INK); } }
    free(b); return; }
  if (b && n > 2) { int w = b[0], h = b[1], stride = (w + 7) / 8; for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) if (b[2 + r * stride + (c >> 3)] & (0x80 >> (c & 7))) epd.drawPixel(x + c, y + r, INK); free(b); return; }
  free(b);
  box(x, y + 9, 44, 26); put(&Nico14, x + 22 - tw(&Nico14, al) / 2, y + 27, al);        // no logo on the card: code badge
}
static const int LX = 483, RH = 88;
void drawList(const char* clock) {
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
  if (g_grey) { for (float s = 0; s < circ; s += 1) { if (((int)s % 8) >= 4) continue; float a = s / r; epd.fillRect(VCX + (int)lroundf(r * sinf(a)) - 1, VCY - (int)lroundf(r * cosf(a)) - 1, 3, 3, GREY_D); }   // grey dots, no halo
    return; }
  for (int pass = 0; pass < 2; pass++) for (float s = 0; s < circ; s += 1) {
    if (pass == 1 && ((int)s % 8) >= 3) continue;
    float a = s / r; int x = VCX + (int)lroundf(r * sinf(a)), y = VCY - (int)lroundf(r * cosf(a));
    if (pass == 0) epd.fillRect(x - 2, y - 2, 5, 5, PAPER); else epd.fillRect(x, y, 2, 2, INK);
  }
}
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
void drawRadar(Feed feed, bool wifiOk, const char* clock) {
  mapDraw(epd); drawAirports();
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
// Airfields on the radar: dark grey footprint (runways + taxiway margin) with a paper outline and paper runway lines; code label at 30 mi or less, or for large airports.
static void capsule(float x0, float y0, float x1, float y1, int r, uint16_t col) {
  int n = max(1, (int)ceilf(hypotf(x1 - x0, y1 - y0))); for (int i = 0; i <= n; i++) { float t = (float)i / n; epd.fillCircle(lroundf(x0 + (x1 - x0) * t), lroundf(y0 + (y1 - y0) * t), r, col); } }
static void drawAirports() {
  static Apt A[10]; int n = aptList(A, 10); if (!n) return;
  int r = constrain((int)lroundf(0.9f * 0.621f * (235.0f / cfg.radiusMi) * 1.5f / 2), 4, 8);
  for (int i = 0; i < n; i++) for (int k = 0; k < A[i].n; k++) capsule(A[i].r[k].x0, A[i].r[k].y0, A[i].r[k].x1, A[i].r[k].y1, r + 2, PAPER);
  for (int i = 0; i < n; i++) for (int k = 0; k < A[i].n; k++) capsule(A[i].r[k].x0, A[i].r[k].y0, A[i].r[k].x1, A[i].r[k].y1, r, GREY_D);
  for (int i = 0; i < n; i++) for (int k = 0; k < A[i].n; k++) { const AptRwy& q = A[i].r[k]; epd.drawLine(q.x0, q.y0, q.x1, q.y1, PAPER); }
  if (cfg.radiusMi <= 30.0f) for (int i = 0; i < n; i++) { int w = tw(&Nico14, A[i].code), x = constrain((int)A[i].cx + r + 6, 2, 478 - w); putHalo(&Nico14, x, (int)A[i].cy - r - 2, A[i].code, 2); }
  else for (int i = 0; i < n; i++) if (A[i].large) { int w = tw(&Nico14, A[i].code), x = constrain((int)A[i].cx + r + 6, 2, 478 - w); putHalo(&Nico14, x, (int)A[i].cy - r - 2, A[i].code, 2); }
}
