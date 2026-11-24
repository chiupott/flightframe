#include "adsb.h"
#include <vector>
#include <SD.h>
#include <math.h>
#include "diag.h"
#include "net.h"
#include "store.h"
#include "airline_iata.h"
#include <ctype.h>
#include <ArduinoJson.h>
#include <map>
#include <algorithm>
Aircraft planes[MAX_AIRCRAFT]; uint8_t nPlanes = 0;
Trail trails[MAX_AIRCRAFT];

static float milesBetween(float la1, float lo1, float la2, float lo2) {
  const float R = 3958.8f, d2r = PI / 180;
  float dla = (la2 - la1) * d2r, dlo = (lo2 - lo1) * d2r;
  float a = sinf(dla / 2) * sinf(dla / 2) + cosf(la1 * d2r) * cosf(la2 * d2r) * sinf(dlo / 2) * sinf(dlo / 2);
  return 2 * R * asinf(sqrtf(a));
}
// ICAO airline callsign (AFR98E) -> short flight number (AF98E). Anything that is not "3 letters + digit" (G-ABCD, N123AB) stays as is.
static void setFlightNo(Aircraft& a) {
  strlcpy(a.fn, a.cs, sizeof a.fn);
  const char* c = a.cs;
  if (strlen(c) < 4 || !isalpha(c[0]) || !isalpha(c[1]) || !isalpha(c[2]) || !isdigit(c[3])) return;
  int lo = 0, hi = AIRLINE_N - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2, cmp = strncmp(c, AIRLINE_TBL[mid], 3);
    if (!cmp) { a.fn[0] = AIRLINE_TBL[mid][3]; a.fn[1] = AIRLINE_TBL[mid][4]; strlcpy(a.fn + 2, c + 3, sizeof a.fn - 2); return; }
    if (cmp < 0) hi = mid - 1; else lo = mid + 1;
  }
}
static Trail* trailSlot(const char* hex) {
  Trail* free = nullptr; Trail* oldest = &trails[0];
  for (auto& t : trails) {
    if (!strcmp(t.hex, hex)) return &t;
    if (!t.hex[0] && !free) free = &t;
    if (t.seen < oldest->seen) oldest = &t;
  }
  Trail* t = free ? free : oldest; *t = Trail(); strlcpy(t->hex, hex, sizeof t->hex); return t;
}
const Trail* trailFor(const char* hex) { for (auto& t : trails) if (!strcmp(t.hex, hex)) return &t; return nullptr; }

static void trailAdd(const Aircraft& a) {
  Trail* t = trailSlot(a.hex); t->seen = millis();
  if (t->n && milesBetween(t->lat[t->n - 1], t->lon[t->n - 1], a.lat, a.lon) < 0.15f) return;   // not moved enough
  if (t->n == MAX_TRAIL_PTS) { memmove(t->lat, t->lat + 1, sizeof(float) * (MAX_TRAIL_PTS - 1)); memmove(t->lon, t->lon + 1, sizeof(float) * (MAX_TRAIL_PTS - 1)); t->n--; }
  t->lat[t->n] = a.lat; t->lon[t->n] = a.lon; t->n++;
}

bool adsbPoll() {
  // adsb.fi open API v3. radius in nautical miles. Path confirmed against the live API on 15 Nov.
  const float rMi = cfg.view == 1 ? 92.0f : cfg.radiusMi;                // window view: planes out to about 80 nm
  float nm = rMi * 0.868976f;
  String url = "https://opendata.adsb.fi/api/v3/lat/" + String(cfg.lat, 4) + "/lon/" + String(cfg.lon, 4) + "/dist/" + String(nm, 0);
  // The London feed is ~85 KB: stream it through a filter that keeps only the fields we use.
  JsonDocument filter; for (const char* k : {"hex", "flight", "lat", "lon", "alt_baro", "gs", "track", "t", "r", "category"}) { filter["ac"][0][k] = true; filter["aircraft"][0][k] = true; }
  JsonDocument d; if (!httpsGetJson(url, d, filter, 20000)) return false;
  JsonArray ac = d["ac"].is<JsonArray>() ? d["ac"].as<JsonArray>() : d["aircraft"].as<JsonArray>();
  nPlanes = 0;
  static Aircraft cand;
  for (JsonObject o : ac) {
    if (!o["lat"].is<float>() || !o["lon"].is<float>()) continue;
    if (o["alt_baro"].is<const char*>()) continue;                 // "ground"
    Aircraft& a = cand;
    a = Aircraft();
    strlcpy(a.hex, o["hex"] | "", sizeof a.hex); strlcpy(a.type, o["t"] | "", sizeof a.type);
    strlcpy(a.reg, o["r"] | "", sizeof a.reg);   strlcpy(a.cat, o["category"] | "", sizeof a.cat);
    String cs = o["flight"] | ""; cs.trim(); strlcpy(a.cs, cs.c_str(), sizeof a.cs);
    a.lat = o["lat"]; a.lon = o["lon"]; a.track = o["track"] | 0.0f;
    a.alt = o["alt_baro"] | 0; a.gs = (int16_t)(o["gs"] | 0.0f);
    setFlightNo(a);
    a.distMi = milesBetween(cfg.lat, cfg.lon, a.lat, a.lon);
    if (a.distMi > rMi) continue;
    if (nPlanes < MAX_AIRCRAFT) { planes[nPlanes++] = a; continue; }
    uint8_t far = 0; for (uint8_t i = 1; i < nPlanes; i++) if (planes[i].distMi > planes[far].distMi) far = i;   // full: keep the nearest ones
    if (a.distMi < planes[far].distMi) planes[far] = a;
  }
  std::sort(planes, planes + nPlanes, [](const Aircraft& x, const Aircraft& y) { return x.distMi < y.distMi; });
  for (uint8_t i = 0; i < nPlanes; i++) trailAdd(planes[i]);
  for (auto& t : trails) if (t.hex[0] && millis() - t.seen > TRAIL_KEEP_MS) t = Trail();   // left range: drop history
  return true;
}

// ---- routes (adsbdb), cached by callsign for the session ----
struct R { char from[5], to[5], fn[9]; bool ok; float oLa, oLo, dLa, dLo; uint8_t geo; };   // geo: 0 airports not placed yet, 1 placed, 2 not found
static std::map<String, R> cache;
static int routeBudget = 0;                  // new (uncached) lookups still allowed in this cycle
void routeLookup(Aircraft& a) {
  a.routePending = false;
  if (!a.cs[0]) return;
  auto it = cache.find(a.cs);
  if (it == cache.end()) {
    if (routeBudget <= 0) { a.routeKnown = false; a.routePending = true; return; }            // over this cycle's allowance: leave it for the next cycle (the label shows "?" until then)
    routeBudget--;
    if (cache.size() >= 200) cache.clear();  // the cache lives in RAM: never let it grow without limit over days
    R r{}; String body;
    if (httpsGet(String("https://api.adsbdb.com/v0/callsign/") + a.cs, body)) {
      JsonDocument d;
      if (!deserializeJson(d, body) && d["response"]["flightroute"].is<JsonObject>()) {
        strlcpy(r.from, d["response"]["flightroute"]["origin"]["iata_code"] | "", sizeof r.from);
        strlcpy(r.to, d["response"]["flightroute"]["destination"]["iata_code"] | "", sizeof r.to);
        strlcpy(r.fn, d["response"]["flightroute"]["callsign_iata"] | "", sizeof r.fn);   // the real flight number, when adsbdb knows it
        r.ok = r.from[0] && r.to[0];
      }
    }
    it = cache.emplace(a.cs, r).first;       // cache misses too: one request per new callsign
  }
  strlcpy(a.from, it->second.from, sizeof a.from); strlcpy(a.to, it->second.to, sizeof a.to); a.routeKnown = it->second.ok;
  if (it->second.fn[0]) strlcpy(a.fn, it->second.fn, sizeof a.fn);
}
// ---- is the listed route believable for where this plane is and which way it flies? ----
static void resolveAirports() {            // one pass over the SD airport file for any airport codes not placed yet
  struct W { char c[5]; float la, lo; bool f; }; std::vector<W> w;
  for (auto& kv : cache) { R& r = kv.second; if (!r.ok || r.geo) continue;
    for (const char* c : {r.from, r.to}) { bool have = false; for (auto& x : w) if (!strcmp(x.c, c)) have = true; if (!have && w.size() < 40) { W x{}; strlcpy(x.c, c, sizeof x.c); w.push_back(x); } } }
  if (w.empty()) return;
  File f = SD.open("/map/airports.bin"); uint8_t h[8];
  if (f && f.read(h, 8) == 8 && !memcmp(h, "FFA1", 4)) {
    uint32_t n = h[4] | (h[5] << 8) | (h[6] << 16) | ((uint32_t)h[7] << 24); static uint8_t buf[512]; size_t bl = 0, bp = 0;
    auto get = [&](uint8_t* d, size_t k) -> bool { while (k) { if (bp == bl) { int r = f.read(buf, sizeof buf); if (r <= 0) return false; bl = r; bp = 0; } size_t m = min(k, bl - bp); if (d) { memcpy(d, buf + bp, m); d += m; } bp += m; k -= m; } return true; };
    for (uint32_t i = 0; i < n; i++) {     // record: i32 lat, i32 lon (1e-4 deg), char[4] code, u8 flags, u8 runways, runways x 16 bytes
      uint8_t rec[14]; if (!get(rec, 14)) break; int32_t la, lo; memcpy(&la, rec, 4); memcpy(&lo, rec + 4, 4);
      for (auto& x : w) if (!x.f && !strncmp((const char*)rec + 8, x.c, 4)) { x.f = true; x.la = la / 1e4f; x.lo = lo / 1e4f; }
      if (!get(nullptr, (size_t)rec[13] * 16)) break;
    }
  }
  if (f) f.close();
  auto find = [&](const char* c, float& la, float& lo) { for (auto& x : w) if (x.f && !strcmp(x.c, c)) { la = x.la; lo = x.lo; return true; } return false; };
  for (auto& kv : cache) { R& r = kv.second; if (!r.ok || r.geo) continue; bool a = false, b = false;
    for (auto& x : w) { if (!strcmp(x.c, r.from)) a = true; if (!strcmp(x.c, r.to)) b = true; }
    if (a && b) r.geo = (find(r.from, r.oLa, r.oLo) && find(r.to, r.dLa, r.dLo)) ? 1 : 2; }
}
static float distNm(float la1, float lo1, float la2, float lo2) {
  const float k = M_PI / 180.0f; float dla = (la2 - la1) * k, dlo = (lo2 - lo1) * k;
  float a = sinf(dla / 2) * sinf(dla / 2) + cosf(la1 * k) * cosf(la2 * k) * sinf(dlo / 2) * sinf(dlo / 2);
  return 2 * 3440.065f * asinf(sqrtf(a));
}
static float bearingDeg(float la1, float lo1, float la2, float lo2) {
  const float k = M_PI / 180.0f; float dlo = (lo2 - lo1) * k;
  float y = sinf(dlo) * cosf(la2 * k), x = cosf(la1 * k) * sinf(la2 * k) - sinf(la1 * k) * cosf(la2 * k) * cosf(dlo);
  float b = atan2f(y, x) / k; return b < 0 ? b + 360.0f : b;
}
// Judges only planes that are cruising (above 15,000 ft) and more than 60 nm from both airports: near an airport planes turn, hold and get vectored, so those routes are never rejected.
static bool believable(const R& r, const Aircraft& a) {
  if (r.geo != 1 || a.alt < 15000) return true;
  float dD = distNm(a.lat, a.lon, r.dLa, r.dLo), dO = distNm(a.lat, a.lon, r.oLa, r.oLo), dOD = distNm(r.oLa, r.oLo, r.dLa, r.dLo);
  if (dD < 60 || dO < 60) return true;
  if (dO + dD > 1.3f * dOD + 100) return false;                       // nowhere near the line between the two airports
  float d = fabsf(fmodf(a.track - bearingDeg(a.lat, a.lon, r.dLa, r.dLo) + 540.0f, 360.0f) - 180.0f);
  return d <= 75.0f;                                                  // and flying towards the destination, not away or across
}
void routeLookupAll() {                       // every plane in range, nearest first, at most 8 new requests per cycle (about 10 s) so a busy sky is filled in over a few cycles
  routeBudget = 8;
  for (uint8_t i = 0; i < nPlanes; i++) routeLookup(planes[i]);
  resolveAirports();
  for (uint8_t i = 0; i < nPlanes; i++) { Aircraft& a = planes[i]; if (!a.routeKnown) continue; auto it = cache.find(a.cs); if (it != cache.end() && !believable(it->second, a)) a.routeKnown = false; }
}
