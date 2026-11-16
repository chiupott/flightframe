#include "store.h"
#include <Preferences.h>
Config cfg;
static Preferences p;
void storeLoad() {
  p.begin("ff", true);
  cfg.ssid = p.getString("ssid"); cfg.pw = p.getString("pw"); cfg.postcode = p.getString("pc");
  cfg.lat = p.getFloat("lat", NAN); cfg.lon = p.getFloat("lon", NAN);
  cfg.radiusMi = p.getFloat("radf", 50); cfg.km = p.getBool("km", false); cfg.nm = p.getBool("nm", false); cfg.rows = p.getUChar("rows", 5);
  cfg.heading = p.getUShort("head", 0); cfg.view = p.getUChar("view", 0); cfg.bw = p.getBool("bw", false); cfg.hl = p.getUChar("hl", 3); cfg.flip = p.getBool("flip", false);
  cfg.rain = p.getBool("rain", true); cfg.h24 = p.getBool("h24", true);
  p.end();
}
void storeSave() {
  p.begin("ff", false);
  p.putString("ssid", cfg.ssid); p.putString("pw", cfg.pw); p.putString("pc", cfg.postcode);
  p.putFloat("lat", cfg.lat); p.putFloat("lon", cfg.lon);
  p.putFloat("radf", cfg.radiusMi); p.putBool("km", cfg.km); p.putBool("nm", cfg.nm); p.putUChar("rows", cfg.rows);
  p.putUShort("head", cfg.heading); p.putUChar("view", cfg.view); p.putBool("bw", cfg.bw); p.putUChar("hl", cfg.hl); p.putBool("flip", cfg.flip);
  p.putBool("rain", cfg.rain); p.putBool("h24", cfg.h24);
  p.end();
}
void storeWipe() { p.begin("ff", false); p.clear(); p.end(); cfg = Config(); }
uint8_t bootCount() { p.begin("ff", false); uint8_t n = p.getUChar("boots", 0) + 1; p.putUChar("boots", n); p.end(); return n; }
void bootCountClear() { p.begin("ff", false); p.putUChar("boots", 0); p.end(); }
String prefGet(const char* k) { p.begin("ff", true); String v = p.getString(k); p.end(); return v; }
void prefSet(const char* k, const String& v) { p.begin("ff", false); p.putString(k, v); p.end(); }
