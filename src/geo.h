#pragma once
#include <Arduino.h>
// Postcode -> lat/lon via postcodes.io. Called once after joining Wi-Fi; result stays on the device.
bool geocodePostcode(const String& postcode, float& lat, float& lon);
