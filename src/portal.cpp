#include "portal.h"
#include "config.h"
#include "store.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
extern const char page_start[] asm("_binary_src_portal_page_html_start");
void portalRun() {
  WiFi.mode(WIFI_AP_STA); WiFi.softAP(AP_NAME);
  DNSServer dns; dns.start(53, "*", WiFi.softAPIP());
  WebServer web(80);
  bool saved = false;
  web.on("/", HTTP_GET, [&] { web.send_P(200, "text/html; charset=utf-8", page_start); });
  web.on("/api/state", HTTP_GET, [&] { web.send(200, "application/json", "{\"lan\":false,\"ver\":\"" FW_VERSION "\"}"); });   // setup mode: just the version for the page footer
  web.on("/api/scan", HTTP_GET, [&] {
    int n = WiFi.scanNetworks(); JsonDocument d; JsonArray a = d.to<JsonArray>();
    for (int i = 0; i < n; i++) {
      JsonObject o = a.add<JsonObject>(); o["n"] = WiFi.SSID(i);
      int r = WiFi.RSSI(i); o["s"] = r > -55 ? 4 : r > -65 ? 3 : r > -75 ? 2 : 1; o["b"] = "2.4 GHz";
    }
    String out; serializeJson(d, out); web.send(200, "application/json", out);
  });
  web.on("/api/save", HTTP_POST, [&] {
    JsonDocument d; if (deserializeJson(d, web.arg("plain"))) { web.send(400, "text/plain", "bad json"); return; }
    cfg.ssid = String(d["ssid"] | ""); cfg.pw = String(d["pw"] | ""); cfg.postcode = String(d["postcode"] | "");
    cfg.lat = NAN; cfg.lon = NAN;                                   // resolved after joining Wi-Fi
    String un = String(d["unit"] | "mi"); cfg.km = un == "km"; cfg.nm = un == "nm";
    float r = d["radius"] | 50.0f; if (cfg.km) r /= 1.609344f; else if (cfg.nm) r /= 0.868976f;
    cfg.radiusMi = constrain(r, 3.0f, 100.0f);
    if (d["lat"].is<float>() && d["lon"].is<float>()) { cfg.lat = d["lat"]; cfg.lon = d["lon"]; }    // optional manual coordinates, skips the postcode lookup
    cfg.rows = constrain((int)(d["rows"] | 5), 3, 6); cfg.view = constrain((int)(d["view"] | 0), 0, 2); cfg.bw = d["bw"] | false; cfg.hl = constrain((int)(d["hl"] | 3), 0, 4);
    cfg.heading = (int)(d["head"] | 0) % 360; cfg.headingWin = (int)(d["headw"] | (int)cfg.heading) % 360; cfg.flip = d["flip"] | false; cfg.rain = d["rain"] | true; cfg.h24 = d["h24"] | true;
    storeSave(); saved = true; web.send(200, "text/plain", "ok");
  });
  web.onNotFound([&] { web.sendHeader("Location", "http://" PORTAL_IP "/"); web.send(302); });   // captive portal redirect
  web.begin();
  while (!saved) { dns.processNextRequest(); web.handleClient(); delay(2); }
  delay(800); ESP.restart();
}

// ---- settings server on the home Wi-Fi ----
static WebServer* lan = nullptr; static bool lanChanged = false; static String lanPostcode;
static int viewMode() { return cfg.view == 0 ? (cfg.bw ? 1 : 0) : cfg.view + 1; }          // 0 grey radar, 1 B/W radar, 2 window, 3 engines (as the page numbers them)
static void applyDisplay(JsonDocument& d) {
  String un = String(d["unit"] | "mi"); cfg.km = un == "km"; cfg.nm = un == "nm";
  float r = d["radius"] | 50.0f; if (cfg.km) r /= 1.609344f; else if (cfg.nm) r /= 0.868976f;
  cfg.radiusMi = constrain(r, 3.0f, 100.0f);
  cfg.rows = constrain((int)(d["rows"] | 5), 3, 6); cfg.view = constrain((int)(d["view"] | 0), 0, 2); cfg.bw = d["bw"] | false; cfg.hl = constrain((int)(d["hl"] | 3), 0, 4);
  cfg.heading = (int)(d["head"] | 0) % 360; cfg.headingWin = (int)(d["headw"] | (int)cfg.heading) % 360; cfg.flip = d["flip"] | false; cfg.rain = d["rain"] | true; cfg.h24 = d["h24"] | true;
  if (d["lat"].is<float>() && d["lon"].is<float>()) { cfg.lat = d["lat"]; cfg.lon = d["lon"]; cfg.postcode = ""; }       // new location as coordinates: used straight away
  else { String pc = String(d["postcode"] | ""); pc.trim(); if (pc.length() >= 5) lanPostcode = pc; }                      // new postcode: looked up by the main loop (needs the memory the server uses)
}
void settingsPoll() {
  if (!lan) {
    if (WiFi.status() != WL_CONNECTED) return;
    MDNS.begin("flightframe"); MDNS.addService("http", "tcp", 80);
    lan = new WebServer(80);
    lan->on("/", HTTP_GET, [] { lan->send_P(200, "text/html; charset=utf-8", page_start); });
    lan->on("/api/state", HTTP_GET, [] {
      JsonDocument d; d["lan"] = true; d["unit"] = cfg.km ? "km" : cfg.nm ? "nm" : "mi";
      d["radius"] = (int)lroundf(cfg.km ? cfg.radiusMi * 1.609344f : cfg.nm ? cfg.radiusMi * 0.868976f : cfg.radiusMi);
      d["head"] = cfg.heading; d["headw"] = cfg.headingWin; d["flip"] = cfg.flip; d["mode"] = viewMode(); d["hl"] = cfg.hl; d["rows"] = cfg.rows; d["rain"] = cfg.rain; d["h24"] = cfg.h24; d["ver"] = FW_VERSION;
      String out; serializeJson(d, out); lan->send(200, "application/json", out);
    });
    lan->on("/api/apply", HTTP_POST, [] {
      JsonDocument d; if (deserializeJson(d, lan->arg("plain"))) { lan->send(400, "text/plain", "bad json"); return; }
      applyDisplay(d); storeSave(); lanChanged = true; lan->send(200, "text/plain", "ok");
    });
    lan->onNotFound([] { lan->send(404, "text/plain", "not found"); });
    lan->begin(); Serial.printf("settings server up, heap %u\n", ESP.getFreeHeap());
  }
  lan->handleClient();
}
void settingsSuspend() { if (lan) { lan->stop(); delete lan; lan = nullptr; MDNS.end(); } }
String settingsTakePostcode() { String p = lanPostcode; lanPostcode = ""; return p; }
bool settingsChanged() { bool c = lanChanged; lanChanged = false; return c; }
