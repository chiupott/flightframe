#pragma once
// Shared drawing layer used by every view: panel drivers, colours, text helpers, small primitives.
#include <Arduino.h>
#include <ctype.h>
#include <vector>
#include <algorithm>
#include <GxEPD2_BW.h>
#include "draw.h"
#include "config.h"
#include "adsb.h"
#include "icons.h"
#include "map.h"
#include "view.h"
#include "storage.h"
#include "status_data.h"
#include "clock.h"
#include "terrain.h"
#ifdef FF_GREY
#include "epd_grey.h"
#endif
#include "fonts.h"

typedef GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> EpdBW;
#ifndef FF_GREY
extern EpdBW eb;                                   // black-and-white driver: full frame buffer, partial (no-flash) refreshes
#endif
#ifdef FF_GREY
extern EpdGrey eg;                                 // 4-grey driver: full-screen flash every redraw
extern Adafruit_GFX* G;                            // whichever driver the current view draws with
#define epd (*G)
#else
#define epd eb
#endif
extern uint8_t partials;                           // redraws since the last full refresh (0 = next one is full)

#define INK GxEPD_BLACK
#define PAPER GxEPD_WHITE
#ifdef FF_GREY
#define GREY_L ((uint16_t)(g_grey ? 0xAD55 : 0x0000))
#define GREY_D ((uint16_t)(g_grey ? 0x528A : 0x0000))
#else
#define GREY_L INK    // black-and-white build: greys fall back to black
#define GREY_D INK
#endif

// text: NicoClean pixel fonts; in the grey views each has an anti-aliased twin that is picked automatically
int  tw(const GFXfont* f, const String& s);                                                              // width in px
int  put(const GFXfont* f, int x, int y, const String& s, uint16_t col = INK);                            // returns the pen x after the text
void putHalo(const GFXfont* f, int x, int y, const String& s, int r, uint16_t col = INK);                 // paper outline so text stays readable over the map
#ifdef FF_GREY
int  aaWidth(const AFont* a, const String& t);
int  aaDraw(const AFont* a, int x, int y, const String& t, uint16_t col, bool solid);
#endif
void hline(int x, int y, int w, int th);
void box(int x, int y, int w, int h);
void bmp(const Bmp1& b, int x, int y);
float dU(float mi);                                // miles -> the chosen unit
const char* uName();
String commas(int32_t v);

void pageBegin(bool bw, bool full);                // start a picture on the driver this view needs
bool pageNext();

struct Rect { int x0, y0, x1, y1; };                // label / icon keep-out boxes
static inline bool hit(const Rect& a, const Rect& b) { return !(a.x1 < b.x0 || b.x1 < a.x0 || a.y1 < b.y0 || b.y1 < a.y0); }
String fx(const String& s);                         // UTF-8 arrows / dots / degree sign -> the fonts' spare slots
