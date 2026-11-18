#include "net.h"
#include "diag.h"
#include "store.h"
#include "clock.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_x509_crt_bundle_bin_start");
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3   // Arduino core 3.x (ESP32-C6) wants the bundle size too
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_src_x509_crt_bundle_bin_end");
#define SET_CA(c) ((c).setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start), (c).setTimeout(10), (c).setHandshakeTimeout(10))   /* a stuck TLS connection must not freeze the frame: the library default is 120 s */
#else
#define SET_CA(c) ((c).setCACertBundle(rootca_crt_bundle_start), (c).setTimeout(10), (c).setHandshakeTimeout(10))   /* a stuck TLS connection must not freeze the frame: the library default is 120 s */
#endif
bool wifiConnect(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA); WiFi.setSleep(false); WiFi.begin(cfg.ssid.c_str(), cfg.pw.c_str());   // no modem sleep: it makes the first connections after boot miss their replies
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < timeoutMs) delay(250);
  if (WiFi.status() != WL_CONNECTED) return false;
  static bool settled = false;                   // right after joining, the router can take ~20 s before traffic flows: wait until a connection to the internet really works (once per boot)
  if (!settled) { settled = true; for (uint32_t w = millis(); millis() - w < 30000; ) { WiFiClient p; if (p.connect(IPAddress(1, 1, 1, 1), 443, 1500)) { p.stop(); break; } delay(500); } }
  clockInit();                                   // TLS certificates are date-checked, so the clock must be right before any HTTPS call
  return true;
}
static void netDiag(const String& url) {                                      // why did that request fail? (shown in the serial log only)
  String h = url.substring(8); int sl = h.indexOf('/'); if (sl > 0) h = h.substring(0, sl);
  IPAddress ip; uint32_t t = millis(); bool ok = WiFi.hostByName(h.c_str(), ip);
  Serial.printf("net: rssi %d, ip %s, gw %s, dns %s, lookup %s -> %s (%lu ms), clock %s\n", WiFi.RSSI(), WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(), WiFi.dnsIP().toString().c_str(), h.c_str(), ok ? ip.toString().c_str() : "FAILED", (unsigned long)(millis() - t), clockSet() ? "set" : "NOT SET");
}
bool httpsGet(const String& url, String& body, uint32_t timeoutMs) {
  WiFiClientSecure c; SET_CA(c);
  HTTPClient h; h.setTimeout(timeoutMs); h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  h.setUserAgent("FlightFrame/" FW_VERSION);
  if (!h.begin(c, url)) return false;
  uint32_t t0 = millis(); int code = h.GET(); g_dHttp = code; if (code != 200) { Serial.printf("http fail %d after %lu ms: %.60s\n", code, (unsigned long)(millis() - t0), url.c_str()); netDiag(url); }
  if (code == 200) body = h.getString();
  h.end();
  return code == 200;
}

bool httpsGetJson(const String& url, JsonDocument& doc, JsonDocument& filter, uint32_t timeoutMs) {
  WiFiClientSecure c; SET_CA(c);
  HTTPClient h; h.setTimeout(timeoutMs); h.useHTTP10(true); h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  h.setUserAgent("FlightFrame/" FW_VERSION);
  if (!h.begin(c, url)) return false;
  uint32_t t0 = millis(); int code = h.GET(); g_dHttp = code; if (code != 200) { Serial.printf("http fail %d after %lu ms: %.60s\n", code, (unsigned long)(millis() - t0), url.c_str()); netDiag(url); }
  bool ok = false;
  if (code == 200) {
    // Parse straight from the socket through the filter: no big receive buffer (the grey build has little spare RAM).
    // ArduinoJson treats a momentarily empty socket as end-of-input, so this reader waits for data instead. HTTP/1.0 = no chunking.
    struct NetReader {
      WiFiClient& c; uint32_t to;
      int read() { uint8_t b; return readBytes((char*)&b, 1) == 1 ? b : -1; }
      size_t readBytes(char* buf, size_t n) {
        size_t got = 0;
        while (got < n) {
          uint32_t t = millis();
          while (!c.available()) { if (!c.connected() || millis() - t > to) return got; delay(1); }
          int k = c.read((uint8_t*)buf + got, n - got); if (k > 0) got += k;
        }
        return got;
      }
    } rd{*h.getStreamPtr(), timeoutMs};
    DeserializationError err = deserializeJson(doc, rd, DeserializationOption::Filter(filter));
    ok = !err; if (err) g_dHttp = -1000 - (int)err.code();
  }
  h.end();
  return ok;
}
