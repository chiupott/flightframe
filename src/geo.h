#pragma once
#include <Arduino.h>
// Postcode -> lat/lon. UK postcodes: postcodes.io. Everything else (and UK misses): OpenStreetMap Nominatim, narrowed by country. Called once after joining Wi-Fi; the result stays on the device.
enum GeoResult { GEO_OK, GEO_NOTFOUND, GEO_NET };            // NOTFOUND = the service answered "no such postcode"; NET = we could not reach it (try again later)
GeoResult geocodeEx(const String& postcode, const String& country, float& lat, float& lon);   // country = ISO code ("GB", "NL", ...) or "XX" for free text; retries on network errors
bool geocodePostcode(const String& postcode, float& lat, float& lon);                          // uses the saved country; true only for GEO_OK
