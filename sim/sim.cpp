// Host simulator glue: fake clock, scripted network, in-memory flash. Runs the REAL firmware sources.
#include "Arduino.h"
#include "Preferences.h"
#include "WiFi.h"
#include "SPI.h"
#include "config.h"
#include "net.h"
#include "ota.h"
#include "portal.h"
#include "storage.h"
#include <ArduinoJson.h>
#include <fstream>
#include <sstream>
SerialShim Serial; EspShim ESP; WiFiShim WiFi; SPIShim SPI;
std::map<std::string, std::string> g_nvs; int g_drawCount = 0; const char* g_outPath = "sim_out.pbm";
static unsigned long now_ms = 3000; unsigned long millis() { return now_ms; } void delay(unsigned long ms) { now_ms += ms; }
extern void setup(); extern void loop();

static std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream s; s << f.rdbuf(); return s.str(); }
static int pollNo = 0; static std::string DIR_ = ".";
bool wifiConnect(uint32_t) { return true; }
bool httpsGet(const String& url, String& body, uint32_t) {
  std::string u = url.c_str();
  if (u.find("adsb.fi") != std::string::npos) {
    char p[256]; snprintf(p, sizeof p, "%s/snap_%d.json", DIR_.c_str(), pollNo < 12 ? pollNo : 11); pollNo++;
    body = slurp(p).c_str(); printf("[net] adsb.fi -> snapshot %d (%u bytes)\n", pollNo - 1, body.length()); return body.length() > 0;
  }
  if (u.find("adsbdb") != std::string::npos) {
    std::string cs = u.substr(u.rfind('/') + 1);
    JsonDocument r; deserializeJson(r, slurp(DIR_ + "/routes.json"));
    if (!r[cs.c_str()].is<JsonArray>()) return false;
    JsonDocument o; o["response"]["flightroute"]["origin"]["iata_code"] = r[cs.c_str()][0]; o["response"]["flightroute"]["destination"]["iata_code"] = r[cs.c_str()][1];
    serializeJson(o, body); return true;
  }
  if (u.find("postcodes.io") != std::string::npos) { const char* la = getenv("SIM_LAT"); const char* lo = getenv("SIM_LON"); body = (std::string("{\"status\":200,\"result\":{\"latitude\":") + (la ? la : "51.5074") + ",\"longitude\":" + (lo ? lo : "-0.1278") + "}}").c_str(); return true; }
  return false;
}
bool storInit() { return true; }
bool rainRefresh() { return false; }   // the real one downloads from GitHub
#include "clock.h"
uint32_t clockNow() { return 0; } bool clockSet() { return true; } void clockInit(uint32_t) {}
#include <ctime>
String clockStr() {                                       // fixed 13:35 for reproducible test pictures; SIM_CLOCK=live uses this computer's time
  if (!getenv("SIM_CLOCK")) return "13:35";
  time_t n = time(nullptr); tm t; localtime_r(&n, &t); char b[8]; snprintf(b, sizeof b, "%02d:%02d", t.tm_hour, t.tm_min); return b;
}
bool storReadRect(const char* path, size_t base, size_t srcStride, int row0, int rows, int byte0, int nbytes, uint8_t* out) {
  std::string p = DIR_ + "/../sd" + path; FILE* f = fopen(p.c_str(), "rb"); if (!f) return false;
  for (int r = 0; r < rows; r++) { if (fseek(f, base + (size_t)(row0 + r) * srcStride + byte0, SEEK_SET) || fread(out + (size_t)r * nbytes, 1, nbytes, f) != (size_t)nbytes) { fclose(f); return false; } }
  fclose(f); return true;
}
uint8_t* storLoad(const char* path, size_t& len) {
  std::string p = DIR_ + "/../sd" + path; FILE* f = fopen(p.c_str(), "rb"); if (!f) return nullptr;
  fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET); uint8_t* b = (uint8_t*)malloc(len ? len : 1); fread(b, 1, len, f); fclose(f); return b;
}
void otaBootCheck() {} void otaMarkGood() { printf("[ota] marked good\n"); } bool otaCheckAndUpdate() { return false; }
void portalRun() { printf("[portal] would open setup AP\n"); exit(2); }

int main(int argc, char** argv) {
  DIR_ = argc > 1 ? argv[1] : "."; int polls = argc > 2 ? atoi(argv[2]) : 12; if (argc > 3) g_outPath = argv[3];
  g_nvs["ssid"] = "SimWiFi"; g_nvs["pc"] = "SIM 1AA";               // as if setup page had been completed
  if (getenv("SIM_HEAD")) g_nvs["head"] = getenv("SIM_HEAD"); if (getenv("SIM_FLIP")) g_nvs["flip"] = "1"; if (getenv("SIM_VIEW")) g_nvs["view"] = getenv("SIM_VIEW");
  setup();
  while (g_drawCount < polls) { loop(); }
  printf("[sim] %d screens drawn, last at t=%us, final -> %s\n", g_drawCount, millis() / 1000, g_outPath);
}

// libc pieces the Arduino String needs on AVR/ESP but not glibc
extern "C" {
char* itoa(int v, char* b, int base) { if (base == 16) sprintf(b, "%x", v); else sprintf(b, "%d", v); return b; }
char* ltoa(long v, char* b, int base) { if (base == 16) sprintf(b, "%lx", v); else sprintf(b, "%ld", v); return b; }
char* utoa(unsigned v, char* b, int base) { if (base == 16) sprintf(b, "%x", v); else sprintf(b, "%u", v); return b; }
char* ultoa(unsigned long v, char* b, int base) { if (base == 16) sprintf(b, "%lx", v); else sprintf(b, "%lu", v); return b; }
char* dtostrf(double v, signed char w, unsigned char p, char* b) { sprintf(b, "%*.*f", (int)w, (int)p, v); return b; }
}
