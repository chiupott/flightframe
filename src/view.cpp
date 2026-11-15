#include "view.h"
#include "config.h"
#include <math.h>
static const float MI_LAT = 69.05f, MI_LON = 69.17f;
void viewProj(float lat, float lon, float& x, float& y) {
  float ppm = VR / (float)cfg.radiusMi;
  float e = (lon - cfg.lon) * MI_LON * cosf(cfg.lat * DEG_TO_RAD) * ppm, n = (lat - cfg.lat) * MI_LAT * ppm;
  float h = cfg.heading * DEG_TO_RAD, c = cosf(h), s = sinf(h);
  float e2 = e * c - n * s, n2 = n * c + e * s;
  if (cfg.flip) e2 = -e2;
  x = VCX + e2; y = VCY - n2;
}
void viewUnproj(float x, float y, float& lat, float& lon) {
  float ppm = VR / (float)cfg.radiusMi, e2 = x - VCX, n2 = VCY - y;
  if (cfg.flip) e2 = -e2;
  float h = cfg.heading * DEG_TO_RAD, c = cosf(h), s = sinf(h);
  float e = e2 * c + n2 * s, n = n2 * c - e2 * s;
  lat = cfg.lat + n / ppm / MI_LAT; lon = cfg.lon + e / ppm / MI_LON / cosf(cfg.lat * DEG_TO_RAD);
}
float viewAngle(float compassDeg) { float a = compassDeg - cfg.heading; return cfg.flip ? -a : a; }
