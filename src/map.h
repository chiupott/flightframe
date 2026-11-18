#pragma once
#include <Adafruit_GFX.h>
// Background layers, drawn in this order: sea pattern, land (paper), rain, coastline outline.
void mapDraw(Adafruit_GFX& g);
// Airfields in view (from /map/airports.bin on the SD card), projected to view pixels and scaled 1.5x about their own centre.
struct AptRwy { float x0, y0, x1, y1; };
struct Apt { char code[5]; bool large; uint8_t n; AptRwy r[6]; float cx, cy; };
int aptList(Apt* out, int maxN);   // returns how many
