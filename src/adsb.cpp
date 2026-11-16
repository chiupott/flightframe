#include "adsb.h"
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
struct R { char from[5], to[5], fn[9]; bool ok; };
static std::map<String, R> cache;
void routeLookup(Aircraft& a) {
  if (!a.cs[0]) return;
  auto it = cache.find(a.cs);
  if (it == cache.end()) {
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
