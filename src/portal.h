#pragma once
// First-run / reset setup: open AP "FlightFrame-Setup", captive DNS, serves the setup page.
//   GET  /            setup page (src/portal_page.html)
//   GET  /api/scan    [{"n":"ssid","s":0-4,"b":"2.4 GHz"}]   (ESP32 only sees 2.4 GHz)
//   POST /api/save    {"ssid","pw","postcode","radius","unit","head","flip","rows","rain","h24"}
// Blocks until saved, then reboots. The phone has no internet while on the AP, so the postcode is
// only stored here; the frame geocodes it after joining the home Wi-Fi.
void portalRun();
