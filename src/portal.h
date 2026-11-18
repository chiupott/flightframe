#pragma once
#include <Arduino.h>
// First-run / reset setup: open AP "FlightFrame-Setup", captive DNS, serves the setup page.
//   GET  /            setup page (src/portal_page.html)
//   GET  /api/scan    [{"n":"ssid","s":0-4,"b":"2.4 GHz"}]   (ESP32 only sees 2.4 GHz)
//   POST /api/save    {"ssid","pw","postcode","radius","unit","head","flip","rows","rain","h24"}
// Blocks until saved, then reboots. The phone has no internet while on the AP, so the postcode is
// only stored here; the frame geocodes it after joining the home Wi-Fi.
void portalRun();
// Settings on the home Wi-Fi: http://flightframe.local (same page, without the Wi-Fi and location steps). Lets the people at home change view,
// radius, units, facing etc. without the 3-power-cycle reset. Display settings only; Wi-Fi and location still need the reset.
//   GET  /api/state   current display settings    POST /api/apply   same fields as /api/save minus ssid/pw/postcode, applied at once
void settingsPoll();           // call often from loop(): starts the server once Wi-Fi is up, then serves requests
void settingsSuspend();        // free the server and mDNS before a redraw or download (they need the memory); settingsPoll() brings them back
String settingsTakePostcode();  // a new postcode sent from the settings page, to be looked up by the main loop (empty if none)
bool settingsChanged();        // true once after the page applied new settings (caller redraws)
