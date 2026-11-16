#pragma once
// Tiny status line printed on the radar screen while we bring the frame up (kit-day debugging, no serial cable needed).
inline int g_dHttp = -999;   // last HTTP status from the plane feed (200 = ok, negative = connection / TLS error)
inline int g_dSd = -1;       // 1 = SD card mounted, 0 = not
inline int g_dPack = -1;     // files fetched by the first-boot download (-1 = not tried)
inline int g_dPlanes = -1;   // aircraft parsed from the last feed
