#include "geo.h"
#include "net.h"
#include "diag.h"
#include "store.h"
#include <ArduinoJson.h>
static String urlEnc(const String& s) {
  String o; for (size_t i = 0; i < s.length(); i++) { char c = s[i]; if (isalnum((uint8_t)c) || c == '-' || c == '.' || c == '_' || c == '~') o += c; else { char b[4]; snprintf(b, sizeof b, "%%%02X", (uint8_t)c); o += b; } }
  return o;
}
static GeoResult lookupUK(String q, float& lat, float& lon) {       // postcodes.io, one try
  q.replace(" ", ""); String body;
  if (!httpsGet("https://api.postcodes.io/postcodes/" + urlEnc(q), body)) return g_dHttp == 404 ? GEO_NOTFOUND : GEO_NET;
  JsonDocument d; if (deserializeJson(d, body) || (d["status"] | 0) != 200) return GEO_NOTFOUND;
  lat = d["result"]["latitude"]; lon = d["result"]["longitude"];
  return (lat == 0 && lon == 0) ? GEO_NOTFOUND : GEO_OK;
}
static GeoResult lookupOSM(const String& pc, String cc, float& lat, float& lon) {   // OpenStreetMap Nominatim (a name for the "no match" case is an empty list), one try
  cc.toLowerCase(); String url = "https://nominatim.openstreetmap.org/search?format=jsonv2&limit=1";
  if (cc.length() == 2 && cc != "xx") url += "&countrycodes=" + cc + "&postalcode=" + urlEnc(pc); else url += "&q=" + urlEnc(pc);
  String body; if (!httpsGet(url, body)) return GEO_NET;
  JsonDocument filter; filter[0]["lat"] = true; filter[0]["lon"] = true;
  JsonDocument d; if (deserializeJson(d, body, DeserializationOption::Filter(filter)) || !d.is<JsonArray>() || d.size() == 0) return GEO_NOTFOUND;
  lat = String(d[0]["lat"] | "0").toFloat(); lon = String(d[0]["lon"] | "0").toFloat();
  return (lat == 0 && lon == 0) ? GEO_NOTFOUND : GEO_OK;
}
GeoResult geocodeEx(const String& pc, const String& country, float& lat, float& lon) {
  String t = pc; t.trim(); String cc = country.length() ? country : String("GB");
  for (int attempt = 0; attempt < 4; attempt++) {
    GeoResult r = GEO_NET;
    if (cc == "GB") { r = lookupUK(t, lat, lon); if (r == GEO_NOTFOUND) r = lookupOSM(t, "gb", lat, lon); }
    else r = lookupOSM(t, cc, lat, lon);
    if (r != GEO_NET) return r;
    delay(3000);                                                // timeout, TLS or DNS: the network. Wait and try again.
  }
  return GEO_NET;
}
bool geocodePostcode(const String& pc, float& lat, float& lon) { return geocodeEx(pc, cfg.country, lat, lon) == GEO_OK; }
