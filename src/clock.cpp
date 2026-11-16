#include "clock.h"
#include "config.h"
#include "store.h"
#include <time.h>
#define UK_TZ "GMT0BST,M3.5.0/1,M10.5.0"       // UK + Ireland; the frame switches to CET by location (see zoneFor)
#define CET_TZ "CET-1CEST,M3.5.0,M10.5.0/3"     // continental Europe: same switch dates as the UK, one hour ahead
static bool started = false;
static const char* zoneFor(float lat, float lon) {   // rough: UK + Ireland on UK time, everything else in the supported map area on CET
  if (isnan(lat) || isnan(lon)) return UK_TZ;
  bool uk = lon <= 1.8f && lon > -11.0f && lat >= 50.0f && !(lon > 1.0f && lat < 50.85f);
  return uk ? UK_TZ : CET_TZ;
}
static void applyZone() {
  static const char* cur = nullptr; const char* z = zoneFor(cfg.lat, cfg.lon);
  if (z != cur) { cur = z; setenv("TZ", z, 1); tzset(); }
}
bool clockSet() { return time(nullptr) > 1700000000; }   // after Nov 2023 = real time, not the 1970 default
void clockInit(uint32_t waitMs) {
  if (!started) { configTzTime(zoneFor(cfg.lat, cfg.lon), "pool.ntp.org", "time.google.com", "time.cloudflare.com"); started = true; }
  uint32_t t = millis();
  while (!clockSet() && millis() - t < waitMs) delay(100);
}
uint32_t clockNow() { return clockSet() ? (uint32_t)time(nullptr) : 0; }
String clockStr() {
  if (!clockSet()) return "--:--";
  applyZone(); struct tm t; time_t n = time(nullptr); localtime_r(&n, &t);
  char b[12];
  if (cfg.h24) snprintf(b, sizeof b, "%02d:%02d", t.tm_hour, t.tm_min);
  else snprintf(b, sizeof b, "%d:%02d %s", t.tm_hour % 12 ? t.tm_hour % 12 : 12, t.tm_min, t.tm_hour >= 12 ? "pm" : "am");
  return b;
}
