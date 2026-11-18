#include "geo.h"
#include "net.h"
#include "diag.h"
#include <ArduinoJson.h>
GeoResult geocodeEx(const String& pc, float& lat, float& lon) {
  String q = pc; q.trim(); q.replace(" ", "");
  for (int attempt = 0; attempt < 4; attempt++) {
    String body;
    if (httpsGet("https://api.postcodes.io/postcodes/" + q, body)) {
      JsonDocument d;
      if (deserializeJson(d, body) || (d["status"] | 0) != 200) return GEO_NOTFOUND;
      lat = d["result"]["latitude"]; lon = d["result"]["longitude"];
      return (lat == 0 && lon == 0) ? GEO_NOTFOUND : GEO_OK;
    }
    if (g_dHttp == 404) return GEO_NOTFOUND;                 // the service looked it up and does not know it
    delay(3000);                                               // anything else (timeout, TLS, DNS) is the network: wait and try again
  }
  return GEO_NET;
}
bool geocodePostcode(const String& pc, float& lat, float& lon) { return geocodeEx(pc, lat, lon) == GEO_OK; }
