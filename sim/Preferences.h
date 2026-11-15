#pragma once
#include "Arduino.h"
#include <map>
#include <string>
extern std::map<std::string, std::string> g_nvs;
class Preferences {
  bool ro = false;
  String g(const char* k, bool& has) { auto i = g_nvs.find(k); has = i != g_nvs.end(); return has ? String(i->second.c_str()) : String(); }
 public:
  bool begin(const char*, bool r = false) { ro = r; return true; }  void end() {}
  void clear() { g_nvs.clear(); }
  String getString(const char* k) { bool h; return g(k, h); }
  size_t putString(const char* k, const String& v) { g_nvs[k] = v.c_str(); return v.length(); }
  float getFloat(const char* k, float d) { bool h; String s = g(k, h); return h ? s.toFloat() : d; }
  size_t putFloat(const char* k, float v) { g_nvs[k] = String(v, 6).c_str(); return 4; }
  uint8_t getUChar(const char* k, uint8_t d) { bool h; String s = g(k, h); return h ? s.toInt() : d; }
  size_t putUChar(const char* k, uint8_t v) { g_nvs[k] = String((int)v).c_str(); return 1; }
  uint16_t getUShort(const char* k, uint16_t d) { bool h; String s = g(k, h); return h ? s.toInt() : d; }
  size_t putUShort(const char* k, uint16_t v) { g_nvs[k] = String((int)v).c_str(); return 2; }
  bool getBool(const char* k, bool d) { bool h; String s = g(k, h); return h ? s.toInt() != 0 : d; }
  size_t putBool(const char* k, bool v) { g_nvs[k] = v ? "1" : "0"; return 1; }
};
