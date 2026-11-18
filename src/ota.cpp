#include "ota.h"
#include "config.h"
#include "net.h"
#include "store.h"
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/base64.h>
#include "ota_pubkey.h"
#include "draw.h"
#include <esp_ota_ops.h>
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_x509_crt_bundle_bin_start");
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3   // Arduino core 3.x (ESP32-C6) wants the bundle size too
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_src_x509_crt_bundle_bin_end");
#define SET_CA(c) ((c).setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start), (c).setTimeout(10), (c).setHandshakeTimeout(10))   /* a stuck TLS connection must not freeze the frame: the library default is 120 s */
#else
#define SET_CA(c) ((c).setCACertBundle(rootca_crt_bundle_start), (c).setTimeout(10), (c).setHandshakeTimeout(10))   /* a stuck TLS connection must not freeze the frame: the library default is 120 s */
#endif

void otaBootCheck() {
  if (prefGet("probation") != "1") return;
  int tries = prefGet("tries").toInt() + 1; prefSet("tries", String(tries));
  if (tries > 3) {                                    // new image never proved itself: go back
    prefSet("skipver", prefGet("pendingver")); prefSet("probation", "0"); prefSet("tries", "0");
    const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);   // the slot we did not boot from
    if (other && esp_ota_set_boot_partition(other) == ESP_OK) esp_restart();
  }
}
void otaMarkGood() { if (prefGet("probation") == "1") { prefSet("probation", "0"); prefSet("tries", "0"); } }

static bool newer(const String& a, const String& b) {      // "v1.2.3" > "1.2.0"
  int x[3] = {0,0,0}, y[3] = {0,0,0};
  sscanf(a.c_str()[0] == 'v' ? a.c_str() + 1 : a.c_str(), "%d.%d.%d", &x[0], &x[1], &x[2]);
  sscanf(b.c_str()[0] == 'v' ? b.c_str() + 1 : b.c_str(), "%d.%d.%d", &y[0], &y[1], &y[2]);
  for (int i = 0; i < 3; i++) if (x[i] != y[i]) return x[i] > y[i];
  return false;
}
static bool sigOk(const uint8_t hash[32], const String& b64) {                // true only if the hash is signed by the key built into this firmware
  uint8_t der[96]; size_t n = 0;
  if (mbedtls_base64_decode(der, sizeof der, &n, (const uint8_t*)b64.c_str(), b64.length()) != 0 || n < 8) return false;
  mbedtls_pk_context pk; mbedtls_pk_init(&pk);
  bool ok = mbedtls_pk_parse_public_key(&pk, (const uint8_t*)OTA_PUBKEY_PEM, sizeof OTA_PUBKEY_PEM) == 0 &&
            mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, der, n) == 0;
  mbedtls_pk_free(&pk); return ok;
}
static bool checkDone = false;
bool otaChecked() { return checkDone; }                  // did the last check really reach GitHub? (if not, try again soon, not in 6 hours)
bool otaCheckAndUpdate() {
  checkDone = false; String body;
  if (!httpsGet(String("https://api.github.com/repos/" GH_OWNER "/" GH_REPO "/releases/latest"), body)) return false;
  JsonDocument d; if (deserializeJson(d, body)) return false;
  checkDone = true;
  String tag = d["tag_name"] | "";
  if (!tag.length() || !newer(tag, FW_VERSION) || tag == prefGet("skipver")) return false;
  String url, sigUrl;
  for (JsonObject a : d["assets"].as<JsonArray>()) { String n = a["name"] | ""; if (n == GH_ASSET) url = String(a["browser_download_url"] | ""); else if (n == GH_ASSET_SIG) sigUrl = String(a["browser_download_url"] | ""); }
  d.clear(); body = "";
  if (!url.length() || !sigUrl.length()) { Serial.println("ota: release has no signed firmware, ignored"); return false; }
  String sig; if (!httpsGet(sigUrl, sig)) return false; sig.trim();
  WiFiClientSecure c; SET_CA(c);
  HTTPClient h; h.setTimeout(20000); h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); h.setUserAgent("FlightFrame/" FW_VERSION);
  if (!h.begin(c, url)) return false;
  int code = h.GET(), len = h.getSize();
  if (code != 200 || len < 200000 || len > 1900000) { Serial.printf("ota: download refused (http %d, %d bytes)\n", code, len); h.end(); return false; }
  drawMessage("Updating the frame", "Please do not unplug it. About a minute.");
  if (!Update.begin(len)) { h.end(); return false; }
  mbedtls_md_context_t md; mbedtls_md_init(&md); mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0); mbedtls_md_starts(&md);
  WiFiClient* s = h.getStreamPtr(); static uint8_t buf[1024]; int left = len; uint32_t last = millis(); bool ok = true;
  while (left > 0 && (h.connected() || s->available()) && millis() - last < 20000) {
    int n = s->readBytes(buf, min(left, (int)sizeof buf));
    if (n <= 0) { delay(5); continue; }
    mbedtls_md_update(&md, buf, n);
    if (Update.write(buf, n) != (size_t)n) { ok = false; break; }
    left -= n; last = millis();
  }
  h.end();
  uint8_t hash[32]; mbedtls_md_finish(&md, hash); mbedtls_md_free(&md);
  if (!ok || left != 0) { Update.abort(); Serial.printf("ota: download incomplete (%d bytes missing)\n", left); return false; }
  if (!sigOk(hash, sig)) { Update.abort(); prefSet("skipver", tag); Serial.println("ota: SIGNATURE REJECTED, update discarded"); return false; }
  prefSet("pendingver", tag); prefSet("probation", "1"); prefSet("tries", "0");
  if (!Update.end(true)) { prefSet("probation", "0"); Serial.println("ota: could not finish the update"); return false; }
  Serial.printf("ota: %s installed and verified, restarting\n", tag.c_str());
  delay(300); ESP.restart(); return true;
}
