#pragma once
#include "config.h"
struct Aircraft {
  char hex[7] = "";  char cs[9] = "";  char type[5] = "";  char reg[9] = "";
  char fn[9] = "";                       // flight number as passengers know it (AF98E, not AFR98E)
  char cat[3] = "";                      // ADS-B category, fallback for icon class
  float lat = 0, lon = 0, track = 0;
  int32_t alt = 0;  int16_t gs = 0;      // feet, knots
  float distMi = 0;
  char from[5] = "", to[5] = "";         // IATA, filled by routeLookup()
  bool routeKnown = false;
  bool routePending = false;             // not looked up yet (shown as "..." until the lookup has run)
};
struct Trail { char hex[7] = ""; float lat[MAX_TRAIL_PTS], lon[MAX_TRAIL_PTS]; uint8_t n = 0; uint32_t seen = 0; };

extern Aircraft planes[MAX_AIRCRAFT]; extern uint8_t nPlanes;   // sorted nearest first
extern Trail trails[MAX_AIRCRAFT];
bool adsbPoll();                         // false = feed unreachable (status icon: no feed)
void routeLookupAll();                    // route for every plane in range (cached by callsign, a few new requests per cycle)
void routeLookup(Aircraft& a);           // adsbdb, cached by callsign
const Trail* trailFor(const char* hex);
