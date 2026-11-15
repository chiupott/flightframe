#include "storage.h"
#include "config.h"
#include <SD.h>
bool storInit() { return SD.begin(SD_CS); }
uint8_t* storLoad(const char* path, size_t& len) {
  File f = SD.open(path); if (!f) return nullptr;
  len = f.size(); uint8_t* b = (uint8_t*)malloc(len ? len : 1);
  if (!b || f.read(b, len) != (int)len) { free(b); f.close(); return nullptr; }
  f.close(); return b;
}
bool storReadRect(const char* path, size_t base, size_t srcStride, int row0, int rows, int byte0, int nbytes, uint8_t* out) {
  File f = SD.open(path); if (!f) return false;
  for (int r = 0; r < rows; r++) {
    if (!f.seek(base + (size_t)(row0 + r) * srcStride + byte0) || f.read(out + (size_t)r * nbytes, nbytes) != nbytes) { f.close(); return false; }
  }
  f.close(); return true;
}
