#include "ota.h"
#include "config.h"
#include "net.h"
#include "store.h"
#include <ArduinoJson.h>
#include <HTTPUpdate.h>
#include <esp_ota_ops.h>
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_src_x509_crt_bundle_bin_start");
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3   // Arduino core 3.x (ESP32-C6) wants the bundle size too
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_src_x509_crt_bundle_bin_end");
#define SET_CA(c) (c).setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start)
#else
#define SET_CA(c) (c).setCACertBundle(rootca_crt_bundle_start)
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
bool otaCheckAndUpdate() {
  String body;
  if (!httpsGet(String("https://api.github.com/repos/" GH_OWNER "/" GH_REPO "/releases/latest"), body)) return false;
  JsonDocument d; if (deserializeJson(d, body)) return false;
  String tag = d["tag_name"] | "";
  if (!tag.length() || !newer(tag, FW_VERSION) || tag == prefGet("skipver")) return false;
  String url;
  for (JsonObject a : d["assets"].as<JsonArray>()) if (String(a["name"] | "") == GH_ASSET) url = String(a["browser_download_url"] | "");
  if (!url.length()) return false;
  prefSet("pendingver", tag); prefSet("probation", "1"); prefSet("tries", "0");
  WiFiClientSecure c; SET_CA(c);
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(true);
  t_httpUpdate_return r = httpUpdate.update(c, url);
  if (r != HTTP_UPDATE_OK) { prefSet("probation", "0"); return false; }
  return true;
}
