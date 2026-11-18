#pragma once
// One drawing function per screen; draw.cpp picks which one runs.
#include "draw.h"
void drawRadar(Feed feed, bool wifiOk, const char* clock);    // map, planes, labels (left 480 px)
void drawList(const char* clock);                              // nearest-planes list (right 320 px)
void drawWindow(Feed feed, bool wifiOk, const char* clock);
void winLoad();                                                // read the skyline header + sprite index once
bool winReady();                                               // skyline file found and valid
void winRetry();                                               // forget what winLoad found (after the skyline was rebuilt)
void drawEngines(const char* clock);
