#pragma once
// Host shim for GxEPD2_BW: 800x480 framebuffer, firstPage/nextPage draw once, nextPage writes sim_out.pbm
#include "Arduino.h"
#include "Adafruit_GFX.h"
#include <vector>
#define GxEPD_BLACK 0x0000
#define GxEPD_WHITE 0xFFFF
struct GxEPD2_750_T7 { static const int WIDTH = 800, HEIGHT = 480; GxEPD2_750_T7(int, int, int, int) {} };
extern int g_drawCount; extern const char* g_outPath;
template <typename D, int PAGE_H> class GxEPD2_BW : public Adafruit_GFX {
  std::vector<uint8_t> fb; bool full = true;
 public:
  GxEPD2_BW(D) : Adafruit_GFX(D::WIDTH, D::HEIGHT), fb(D::WIDTH * D::HEIGHT, 1) {}
  void init(uint32_t, bool, int, bool) {}
  void setFullWindow() { full = true; } void setPartialWindow(int, int, int, int) { full = false; }
  void firstPage() {}
  bool nextPage() {
    FILE* f = fopen(g_outPath, "wb"); fprintf(f, "P5\n%d %d\n255\n", D::WIDTH, D::HEIGHT);
    for (uint8_t p : fb) fputc(p ? 255 : 0, f); fclose(f); g_drawCount++; return false;
  }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override { if (x >= 0 && y >= 0 && x < D::WIDTH && y < D::HEIGHT) fb[y * D::WIDTH + x] = c != GxEPD_BLACK; }
};
