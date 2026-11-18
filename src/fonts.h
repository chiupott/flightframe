#pragma once
// Font tables live in fonts.cpp (one copy in flash); everyone else just refers to them.
#include <Adafruit_GFX.h>
#include "fonttypes.h"
extern const GFXfont Nico12, Nico13, Nico14, Nico15, Nico16, Nico17, NicoItalic14;
#ifdef FF_GREY
extern const AFont AA12, AA13, AA14, AA15, AA16, AA17, AAI14, AA30, AA36, AAT72;
#endif
