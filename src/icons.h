#pragma once
#include <Adafruit_GFX.h>
#include "icons_data.h"
const Sprite& spriteFor(const char* type, const char* cat);
// Draw a top-down aircraft sprite centred on (cx,cy), rotated clockwise by deg (0 = nose up), with a white halo.
void drawSprite(Adafruit_GFX& g, int cx, int cy, float deg, const Sprite& s, float scale = 1.0f);
