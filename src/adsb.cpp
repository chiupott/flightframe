#include "adsb.h"
#include "net.h"
#include "store.h"
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
  float nm = cfg.radiusMi * 0.868976f;
  String url = "https://opendata.adsb.fi/api/v3/lat/" + String(cfg.lat, 4) + "/lon/" + String(cfg.lon, 4) + "/dist/" + String(nm, 0);
  String body; if (!httpsGet(url, body, 15000)) return false;
  JsonDocument d; if (deserializeJson(d, body)) return false;
  JsonArray ac = d["ac"].is<JsonArray>() ? d["ac"].as<JsonArray>() : d["aircraft"].as<JsonArray>();
  nPlanes = 0;
  for (JsonObject o : ac) {
    if (nPlanes >= MAX_AIRCRAFT) break;
    if (!o["lat"].is<float>() || !o["lon"].is<float>()) continue;
    if (o["alt_baro"].is<const char*>()) continue;                 // "ground"
    Aircraft& a = planes[nPlanes];
    a = Aircraft();
    strlcpy(a.hex, o["hex"] | "", sizeof a.hex); strlcpy(a.type, o["t"] | "", sizeof a.type);
    strlcpy(a.reg, o["r"] | "", sizeof a.reg);   strlcpy(a.cat, o["category"] | "", sizeof a.cat);
    String cs = o["flight"] | ""; cs.trim(); strlcpy(a.cs, cs.c_str(), sizeof a.cs);
    a.lat = o["lat"]; a.lon = o["lon"]; a.track = o["track"] | 0.0f;
    a.alt = o["alt_baro"] | 0; a.gs = (int16_t)(o["gs"] | 0.0f);
    a.distMi = milesBetween(cfg.lat, cfg.lon, a.lat, a.lon);
    if (a.distMi > cfg.radiusMi) continue;
    nPlanes++;
  }
  std::sort(planes, planes + nPlanes, [](const Aircraft& x, const Aircraft& y) { return x.distMi < y.distMi; });
  for (uint8_t i = 0; i < nPlanes; i++) trailAdd(planes[i]);
  for (auto& t : trails) if (t.hex[0] && millis() - t.seen > TRAIL_KEEP_MS) t = Trail();   // left range: drop history
  return true;
}

// ---- routes (adsbdb), cached by callsign for the session ----
struct R { char from[5], to[5]; bool ok; };
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
        r.ok = r.from[0] && r.to[0];
      }
    }
    it = cache.emplace(a.cs, r).first;       // cache misses too: one request per new callsign
  }
  strlcpy(a.from, it->second.from, sizeof a.from); strlcpy(a.to, it->second.to, sizeof a.to); a.routeKnown = it->second.ok;
}
