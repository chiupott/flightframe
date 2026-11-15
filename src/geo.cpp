#include "geo.h"
#include "net.h"
#include <ArduinoJson.h>
bool geocodePostcode(const String& pc, float& lat, float& lon) {
  String q = pc; q.trim(); q.replace(" ", "");
  String body;
  if (!httpsGet("https://api.postcodes.io/postcodes/" + q, body)) return false;
  JsonDocument d;
  if (deserializeJson(d, body) || (d["status"] | 0) != 200) return false;
  lat = d["result"]["latitude"]; lon = d["result"]["longitude"];
  return !(lat == 0 && lon == 0);
}
