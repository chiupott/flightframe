#pragma once
#include <stdint.h>
struct AGlyph { uint16_t off; uint8_t w, h; int8_t xo, yo; uint8_t adv; };                 // anti-aliased glyph: 2 bits per pixel, MSB first (0 paper, 1 light grey, 2 dark grey, 3 black)
struct AFont { const uint8_t* bits; const AGlyph* g; uint8_t first, last, yAdv; };
