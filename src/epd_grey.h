#pragma once
// 4-level grey driver for the 7.5" 800x480 UC8179 panel (Waveshare "V2" family, screens made after Oct 2023).
// Drop-in for the few GxEPD2 calls draw.cpp uses. Colours (RGB565) are bucketed by brightness into
// black / dark grey / light grey / white. Every refresh is a full-screen flash (~2-4 s): the panel has no partial mode in 4-grey.
#include <Adafruit_GFX.h>
#include <SPI.h>
#include "config.h"

#ifndef GxEPD_BLACK
#define GxEPD_BLACK 0x0000
#define GxEPD_WHITE 0xFFFF
#endif

class EpdGrey : public Adafruit_GFX {
 public:
  EpdGrey() : Adafruit_GFX(800, 480) {}
  void init(int = 0, bool = false, int = 0, bool = false) {
    if (!p10) { p10 = (uint8_t*)malloc(48000); p13 = (uint8_t*)malloc(48000); }
    pinMode(EPD_CS, OUTPUT); digitalWrite(EPD_CS, HIGH);
    pinMode(EPD_DC, OUTPUT); pinMode(EPD_RST, OUTPUT); pinMode(EPD_BUSY, INPUT);
    fillScreen(0xFFFF);
  }
  void setFullWindow() {}
  void setPartialWindow(int, int, int, int) {}
  void firstPage() {}
  bool nextPage() {
    uint32_t h = 2166136261u; for (size_t i = 0; i < 48000; i += 4) { h = (h ^ *(uint32_t*)(p10 + i)) * 16777619u; h = (h ^ *(uint32_t*)(p13 + i)) * 16777619u; }
    if (h != lastHash || !shown) { refresh(); lastHash = h; shown = true; }          // identical picture: no flash
    return false;
  }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    switch (getRotation()) { case 1: { int16_t t = x; x = 800 - 1 - y; y = t; break; } case 2: x = 800 - 1 - x; y = 480 - 1 - y; break; case 3: { int16_t t = x; x = y; y = 480 - 1 - t; break; } }
    if (x < 0 || y < 0 || x >= 800 || y >= 480 || !p10) return;
    uint8_t lvl = level(c); size_t i = (size_t)y * 100 + (x >> 3); uint8_t m = 0x80 >> (x & 7);
    if (lvl == 0 || lvl == 2) p10[i] |= m; else p10[i] &= ~m;      // plane 0x10: black, light grey (confirmed with the test card on the real panel)
    if (lvl == 0 || lvl == 1) p13[i] |= m; else p13[i] &= ~m;      // plane 0x13: black, dark grey
  }
  void fillScreen(uint16_t c) override { if (!p10) return; uint8_t l = level(c); memset(p10, (l == 0 || l == 2) ? 0xFF : 0, 48000); memset(p13, (l == 0 || l == 1) ? 0xFF : 0, 48000); }
 private:
  uint8_t *p10 = nullptr, *p13 = nullptr; uint32_t lastHash = 0; bool shown = false;
  static uint8_t level(uint16_t c) {                 // 0 black, 1 dark grey, 2 light grey, 3 white
    int l = ((c >> 11) & 31) * 77 / 31 + ((c >> 5) & 63) * 150 / 63 + (c & 31) * 29 / 31;
    return l < 64 ? 0 : l < 128 ? 1 : l < 192 ? 2 : 3;
  }
  void cmd(uint8_t c) { digitalWrite(EPD_DC, LOW); digitalWrite(EPD_CS, LOW); SPI.transfer(c); digitalWrite(EPD_CS, HIGH); }
  void dat(uint8_t d) { digitalWrite(EPD_DC, HIGH); digitalWrite(EPD_CS, LOW); SPI.transfer(d); digitalWrite(EPD_CS, HIGH); }
  void wait(uint32_t ms = 20000) { uint32_t t = millis(); delay(5); while (!digitalRead(EPD_BUSY) && millis() - t < ms) delay(5); delay(5); }
  void refresh() {
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(EPD_RST, HIGH); delay(20); digitalWrite(EPD_RST, LOW); delay(4); digitalWrite(EPD_RST, HIGH); delay(20);
    cmd(0x00); dat(0x1F);
    cmd(0x50); dat(0x10); dat(0x07);
    cmd(0x04); delay(100); wait();
    cmd(0x06); dat(0x27); dat(0x27); dat(0x18); dat(0x17);
    cmd(0xE0); dat(0x02);
    cmd(0xE5); dat(0x5F);                              // selects the panel's built-in 4-grey waveform
    cmd(0x10); digitalWrite(EPD_DC, HIGH); digitalWrite(EPD_CS, LOW); SPI.writeBytes(p10, 48000); digitalWrite(EPD_CS, HIGH);
    cmd(0x13); digitalWrite(EPD_DC, HIGH); digitalWrite(EPD_CS, LOW); SPI.writeBytes(p13, 48000); digitalWrite(EPD_CS, HIGH);
    cmd(0x12); delay(100); wait();
    cmd(0x50); dat(0xF7); cmd(0x02); wait(); cmd(0x07); dat(0xA5);   // power off + deep sleep until the next redraw
    SPI.endTransaction();
  }
};
