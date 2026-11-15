#include "icons.h"
#include <string.h>
#include <math.h>
const Sprite& spriteFor(const char* type, const char* cat) {
  for (uint16_t i = 0; i < TYPE_N; i++) if (!strcmp(TYPE_TAB[i].code, type)) return SPRITES[TYPE_TAB[i].spr];
  for (uint8_t i = 0; i < CAT_N; i++) if (!strcmp(CAT_TAB[i].cat, cat)) return SPRITES[CAT_TAB[i].spr];
  return SPRITES[DEFAULT_SPR];
}
void drawSprite(Adafruit_GFX& g, int cx, int cy, float deg, const Sprite& s, float scale) {
  const int n2 = s.n2, stride = (n2 + 7) / 8, side = (int)ceilf(s.side * scale);
  static uint8_t cov[84 * 84];
  if (side > 84) return;
  float th = deg * (float)DEG_TO_RAD, c = cosf(th), sn = sinf(th), h = side / 2.0f;
  for (int dy = 0; dy < side; dy++) for (int dx = 0; dx < side; dx++) {
    float rx = dx + 0.5f - h, ry = dy + 0.5f - h;
    float sx = (rx * c + ry * sn) * 2 / scale + n2 / 2.0f, sy = (-rx * sn + ry * c) * 2 / scale + n2 / 2.0f;   // inverse rotation into the 2x mask
    int n = 0;
    for (int oy = 0; oy < 2; oy++) for (int ox = 0; ox < 2; ox++) {
      int ix = (int)floorf(sx + ox - 0.5f), iy = (int)floorf(sy + oy - 0.5f);
      if (ix >= 0 && iy >= 0 && ix < n2 && iy < n2 && (s.bits[iy * stride + (ix >> 3)] & (0x80 >> (ix & 7)))) n++;
    }
    cov[dy * side + dx] = n >= 2;
  }
  int x0 = cx - side / 2, y0 = cy - side / 2;
  for (int dy = 0; dy < side; dy++) for (int dx = 0; dx < side; dx++) {             // white halo, radius 2 (no corners)
    if (cov[dy * side + dx]) continue;
    bool near = false;
    for (int oy = -2; oy <= 2 && !near; oy++) for (int ox = -2; ox <= 2; ox++) {
      if (abs(ox) + abs(oy) > 3) continue;
      int yy = dy + oy, xx = dx + ox;
      if (yy >= 0 && xx >= 0 && yy < side && xx < side && cov[yy * side + xx]) { near = true; break; }
    }
    if (near) g.drawPixel(x0 + dx, y0 + dy, 0xFFFF);
  }
  for (int dy = 0; dy < side; dy++) for (int dx = 0; dx < side; dx++) if (cov[dy * side + dx]) g.drawPixel(x0 + dx, y0 + dy, 0x0000);
}
