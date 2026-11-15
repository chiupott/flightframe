#include "portal.h"
#include "config.h"
#include "store.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
extern const char page_start[] asm("_binary_src_portal_page_html_start");
void portalRun() {
  WiFi.mode(WIFI_AP_STA); WiFi.softAP(AP_NAME);
  DNSServer dns; dns.start(53, "*", WiFi.softAPIP());
  WebServer web(80);
  bool saved = false;
  web.on("/", HTTP_GET, [&] { web.send_P(200, "text/html; charset=utf-8", page_start); });
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
    cfg.km = String(d["unit"] | "mi") == "km";
    float r = d["radius"] | 50.0f; if (cfg.km) r /= 1.609344f;
    cfg.radiusMi = constrain(r, 3.0f, 100.0f);
    if (d["lat"].is<float>() && d["lon"].is<float>()) { cfg.lat = d["lat"]; cfg.lon = d["lon"]; }    // optional manual coordinates, skips the postcode lookup
    cfg.rows = constrain((int)(d["rows"] | 5), 3, 6); cfg.view = (int)(d["view"] | 0) == 1 ? 1 : 0;
    cfg.heading = (int)(d["head"] | 0) % 360; cfg.flip = d["flip"] | false; cfg.rain = d["rain"] | true; cfg.h24 = d["h24"] | true;
    storeSave(); saved = true; web.send(200, "text/plain", "ok");
  });
  web.onNotFound([&] { web.sendHeader("Location", "http://" PORTAL_IP "/"); web.send(302); });   // captive portal redirect
  web.begin();
  while (!saved) { dns.processNextRequest(); web.handleClient(); delay(2); }
  delay(800); ESP.restart();
}
