#include "gfx.h"
#include "views.h"
#include "fonts.h"
void drawWelcome() {
#ifdef FF_GREY
  g_grey = true; pageBegin(false, true);
  do {
    epd.fillScreen(PAPER);
    // radar motif, top right: rings, cross hairs, a few planes
    const int cx = 596, cy = 128;
    for (int r = 34; r <= 118; r += 42) for (int a = 0; a < 720; a++) if ((a / 6) % 2 == 0) { float t = a * 0.5f * DEG_TO_RAD; epd.drawPixel(cx + lroundf(r * cosf(t)), cy + lroundf(r * sinf(t)), GREY_D); }
    epd.drawFastHLine(cx - 128, cy, 256, GREY_L); epd.drawFastVLine(cx, cy - 106, 212, GREY_L);
    const int PX[4][3] = {{-62, -48, 40}, {52, -20, 160}, {-24, 54, 290}, {84, 52, 100}};
    for (auto& p : PX) { float t = p[2] * DEG_TO_RAD; int x = cx + p[0], y = cy + p[1], fx = lroundf(9 * cosf(t)), fy = lroundf(9 * sinf(t)), sx = lroundf(5 * -sinf(t)), sy = lroundf(5 * cosf(t));
      epd.fillTriangle(x + fx, y + fy, x - fx / 2 + sx, y - fy / 2 + sy, x - fx / 2 - sx, y - fy / 2 - sy, INK); }
    // header tag
    epd.fillRoundRect(48, 36, 142, 30, 15, INK); aaDraw(&AA14, 48 + 71 - aaWidth(&AA14, "FLIGHT FRAME") / 2, 56, "FLIGHT FRAME", 0xFFFF, false);
    // greeting
    aaDraw(&AAT72, 44, 168, "Hi, Josh.", INK, false);
    aaDraw(&AA30, 48, 224, "Let's see what's out the window.", INK, false);
    // the one thing to do: join the Wi-Fi
    epd.fillRoundRect(48, 262, 704, 118, 16, INK);
    aaDraw(&AA17, 78, 302, "Connect your phone to this Wi-Fi to find out:", 0xFFFF, false);
    aaDraw(&AA36, 78, 358, AP_NAME, 0xFFFF, false);
    for (int r = 8; r <= 40; r += 12) for (int a = -44; a <= 44; a++) { float t = (a - 90) * DEG_TO_RAD; for (int w = 0; w < 4; w++) epd.drawPixel(704 + lroundf((r + w * 0.5f) * cosf(t)), 358 + lroundf((r + w * 0.5f) * sinf(t)) + 18, 0xFFFF); }
    epd.fillCircle(704, 376, 3, 0xFFFF);
    // what happens next
    const char* ST[3] = {"Join the Wi-Fi", "Pick your home Wi-Fi", "Choose what to see"};
    for (int i = 0; i < 3; i++) { int x = 48 + i * 250; epd.fillCircle(x + 14, 418, 14, GREY_D); aaDraw(&AA16, x + 14 - aaWidth(&AA16, String(i + 1)) / 2, 424, String(i + 1), 0xFFFF, false); aaDraw(&AA16, x + 38, 424, ST[i], INK, false); }
    // footer
    aaDraw(&AA14, 48, 466, "No password. If the setup page does not open by itself, go to 192.168.4.1", GREY_D, false);
    const char* hm = "Hand made for Josh"; aaDraw(&AA14, 752 - aaWidth(&AA14, hm), 466, hm, GREY_D, false);
  } while (pageNext());
#else
  drawMessage("Welcome", "Join Wi-Fi: " AP_NAME);
#endif
}
#ifdef FF_SWATCH
void drawSwatches() {   // test card: which grey is which on this panel
  pageBegin(false, true);
  do { epd.fillScreen(PAPER);
    const uint16_t C[4] = {PAPER, GREY_L, GREY_D, INK}; const char* N[4] = {"WHITE", "GREY_L", "GREY_D", "BLACK"};
    for (int i = 0; i < 4; i++) { epd.fillRect(20 + i * 195, 40, 170, 300, C[i]); epd.drawRect(20 + i * 195, 40, 170, 300, INK); put(&Nico16, 30 + i * 195, 380, N[i]); }
    epd.fillRect(20, 400, 760, 60, GREY_L); epd.fillRect(20 + 380, 410, 380, 40, GREY_D);
  } while (pageNext());
}
#endif
