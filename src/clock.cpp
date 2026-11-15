#include "clock.h"
#include "config.h"
#include "store.h"
#include <time.h>
#define UK_TZ "GMT0BST,M3.5.0/1,M10.5.0"       // the frame lives in the UK; summer time handled by the rule
static bool started = false;
bool clockSet() { return time(nullptr) > 1700000000; }   // after Nov 2023 = real time, not the 1970 default
void clockInit(uint32_t waitMs) {
  if (!started) { configTzTime(UK_TZ, "pool.ntp.org", "time.google.com", "time.cloudflare.com"); started = true; }
  uint32_t t = millis();
  while (!clockSet() && millis() - t < waitMs) delay(100);
}
uint32_t clockNow() { return clockSet() ? (uint32_t)time(nullptr) : 0; }
String clockStr() {
  if (!clockSet()) return "--:--";
  struct tm t; time_t n = time(nullptr); localtime_r(&n, &t);
  char b[12];
  if (cfg.h24) snprintf(b, sizeof b, "%02d:%02d", t.tm_hour, t.tm_min);
  else snprintf(b, sizeof b, "%d:%02d %s", t.tm_hour % 12 ? t.tm_hour % 12 : 12, t.tm_min, t.tm_hour >= 12 ? "pm" : "am");
  return b;
}
