#include "sdpack.h"
#include "config.h"
#include "store.h"
#include "draw.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <SD.h>
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_x509_crt_bundle_bin_start");
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_src_x509_crt_bundle_bin_end");
#define SET_CA(c) (c).setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start)
#else
#define SET_CA(c) (c).setCACertBundle(rootca_crt_bundle_start)
#endif
#define PACK_URL "https://github.com/" GH_OWNER "/" GH_REPO "/releases/download/sdpack/"
struct PackFile { const char* name; const char* path; const char* magic; };
static const PackFile FILES[] = {
  { "land.bin",    "/map/land.bin", "FFL1" },     // UK + Ireland coastline (Natural Earth, public domain)
  { "sprites.bin", "/sprites.bin",  "FFSP" },     // pre-rendered 3D plane pictures for the window view
};
static bool fetchOne(const PackFile& pf) {
  WiFiClientSecure c; SET_CA(c);
  HTTPClient h; h.setTimeout(20000); h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); h.setUserAgent("FlightFrame/" FW_VERSION);
  if (!h.begin(c, String(PACK_URL) + pf.name)) { Serial.println("sdpack: begin failed"); return false; }
  int code = h.GET(); int len = h.getSize();
  Serial.printf("sdpack: %s http %d len %d heap %u\n", pf.name, code, len, (unsigned)ESP.getFreeHeap());
  if (code != 200 || len < 16 || len > 1500000) { h.end(); return false; }
  SD.mkdir("/map");
  String tmp = String(pf.path) + ".tmp"; SD.remove(tmp.c_str());
  File f = SD.open(tmp.c_str(), FILE_WRITE); if (!f) { Serial.println("sdpack: cannot create file on SD"); h.end(); return false; }
  WiFiClient* s = h.getStreamPtr(); static uint8_t buf[1024]; int left = len; uint32_t last = millis(); bool head = false, ok = true;
  while (left > 0 && h.connected() && millis() - last < 20000) {
    int n = s->readBytes(buf, min(left, (int)sizeof buf));
    if (n <= 0) { delay(5); continue; }
    if (!head) { if (n < 4 || memcmp(buf, pf.magic, 4)) { ok = false; break; } head = true; }
    if (f.write(buf, n) != (size_t)n) { ok = false; break; }
    left -= n; last = millis();
  }
  f.close(); h.end();
  if (!ok || left != 0) { Serial.printf("sdpack: %s failed ok=%d left=%d\n", pf.name, ok, left); SD.remove(tmp.c_str()); return false; }
  SD.remove(pf.path);
  return SD.rename(tmp.c_str(), pf.path);
}
// Airline logos: one packed file (tools/pack_logos.py) in the sdpack release; the frame unpacks it to /logos/<ICAO>.lg and leaves a marker so it only does this once.
#define LOGOS_URL PACK_URL "logos.pack"
static bool logosFetchAndUnpack() {
  WiFiClientSecure c; SET_CA(c);
  HTTPClient h; h.setTimeout(20000); h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); h.setUserAgent("FlightFrame/" FW_VERSION);
  if (!h.begin(c, LOGOS_URL)) return false;
  int code = h.GET(); int len = h.getSize();
  Serial.printf("logos: http %d len %d\n", code, len);
  if (code != 200 || len < 16 || len > 2000000) { h.end(); return false; }
  SD.remove("/logos.tmp");
  File f = SD.open("/logos.tmp", FILE_WRITE); if (!f) { h.end(); return false; }
  WiFiClient* s = h.getStreamPtr(); static uint8_t buf[1024]; int left = len; uint32_t last = millis(); bool ok = true;
  while (left > 0 && h.connected() && millis() - last < 20000) {
    int n = s->readBytes(buf, min(left, (int)sizeof buf));
    if (n <= 0) { delay(5); continue; }
    if (f.write(buf, n) != (size_t)n) { ok = false; break; }
    left -= n; last = millis();
  }
  f.close(); h.end();
  if (!ok || left != 0) { SD.remove("/logos.tmp"); return false; }
  File in = SD.open("/logos.tmp"); if (!in) return false;
  uint8_t hd[6]; bool good = in.read(hd, 6) == 6 && !memcmp(hd, "FFLP", 4); int count = good ? hd[4] | (hd[5] << 8) : 0, wrote = 0;
  SD.mkdir("/logos");
  for (int i = 0; i < count && good; i++) {
    uint8_t eh[6]; if (in.read(eh, 6) != 6) { good = false; break; }
    char name[16]; snprintf(name, sizeof name, "/logos/%.4s.lg", (const char*)eh);
    int n = eh[4] | (eh[5] << 8); if (n <= 0 || n > 1024) { good = false; break; }
    if (in.read(buf, n) != n) { good = false; break; }
    File o = SD.open(name, FILE_WRITE); if (!o) { good = false; break; }
    bool w = o.write(buf, n) == (size_t)n; o.close(); if (!w) { good = false; break; }
    wrote++;
  }
  in.close(); SD.remove("/logos.tmp");
  Serial.printf("logos: unpacked %d of %d\n", wrote, count);
  if (!good || wrote != count) return false;
  File m = SD.open("/logos/.ok", FILE_WRITE); if (m) { m.print("ok"); m.close(); }
  return true;
}
int sdpackEnsure() {
  int missing = 0; for (auto& pf : FILES) if (!SD.exists(pf.path)) missing++;
  bool needLogos = !SD.exists("/logos/.ok");
  if (!missing && !needLogos) return 0;
  drawMessage("Setting up the SD card", needLogos ? "Downloading map, planes and logos..." : "Downloading map and planes...");
  int got = 0; for (auto& pf : FILES) if (!SD.exists(pf.path) && fetchOne(pf)) got++;
  if (needLogos && logosFetchAndUnpack()) got++;
  Serial.printf("sdpack: %d fetched\n", got);
  return got;
}
