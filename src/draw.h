#pragma once
#include <stdint.h>
enum class Feed : uint8_t { Live, Stale, None };
extern bool g_grey;                                       // true while the picture is drawn in greys (false: B/W radar view)
void drawInit();
void drawMessage(const char* l1, const char* l2 = "");   // setup / error screens
void drawScreen(Feed feed, bool wifiOk);                  // radar + list from planes[] and trails[]
void drawForceFull();                                    // make the next redraw a full refresh
void drawSwatches();
