#pragma once
#include <Arduino.h>
// Postcode -> lat/lon via postcodes.io. Called once after joining Wi-Fi; result stays on the device.
enum GeoResult { GEO_OK, GEO_NOTFOUND, GEO_NET };            // NOTFOUND = the service answered "no such postcode"; NET = we could not reach it (try again later)
GeoResult geocodeEx(const String& postcode, float& lat, float& lon);   // retries a few times on network errors
bool geocodePostcode(const String& postcode, float& lat, float& lon);  // true only for GEO_OK
