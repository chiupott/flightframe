#include "net.h"
#include "store.h"
#include "clock.h"
#include <WiFi.h>
#include <HTTPClient.h>
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_x509_crt_bundle_bin_start");
bool wifiConnect(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA); WiFi.begin(cfg.ssid.c_str(), cfg.pw.c_str());
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < timeoutMs) delay(250);
  if (WiFi.status() != WL_CONNECTED) return false;
  clockInit();                                   // TLS certificates are date-checked, so the clock must be right before any HTTPS call
  return true;
}
bool httpsGet(const String& url, String& body, uint32_t timeoutMs) {
  WiFiClientSecure c; c.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient h; h.setTimeout(timeoutMs); h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  h.setUserAgent("FlightFrame/" FW_VERSION);
  if (!h.begin(c, url)) return false;
  int code = h.GET();
  if (code == 200) body = h.getString();
  h.end();
  return code == 200;
}
