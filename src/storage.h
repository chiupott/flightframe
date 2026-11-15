#pragma once
#include <stddef.h>
#include <stdint.h>
// SD card access: map data (/map/land.bin, /map/rain.bin) and airline logos (/logos/<ICAO>.bmp). Never in the public repo.
bool storInit();
uint8_t* storLoad(const char* path, size_t& len);   // malloc'd; caller free()s; nullptr if missing
// Reads a rectangle of bytes from a row-major table inside a file (rain mask) without loading the whole file.
bool storReadRect(const char* path, size_t base, size_t srcStride, int row0, int rows, int byte0, int nbytes, uint8_t* out);
