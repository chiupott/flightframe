#include "msg.h"
#include "msg_keys.h"
#include "net.h"
#include "diag.h"
#include "store.h"
#include "clock.h"
#include "gfx.h"
#include "fonts.h"
#include "storage.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/gcm.h>
#include <mbedtls/base64.h>
extern bool g_grey;
// Packed message (base64 in the mailbox): ver 1 | nonce 12 | ctLen 1 | ciphertext | GCM tag 16 | ECDSA-P256 signature (DER) over everything before it.
// Plaintext: sent time (4 bytes, big endian) | picture-name length 1 | picture name | text.
static bool unpack(const String& b64, Msg& m) {
  uint8_t raw[640]; size_t n = 0;
  if (mbedtls_base64_decode(raw, sizeof raw, &n, (const uint8_t*)b64.c_str(), b64.length()) != 0 || n < 40 || raw[0] != 1) return false;
  size_t ctl = raw[13], body = 14 + ctl + 16; if (ctl < 5 || n <= body + 8) return false;
  uint8_t hash[32]; if (mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), raw, body, hash) != 0) return false;
  mbedtls_pk_context pk; mbedtls_pk_init(&pk);
  bool ok = mbedtls_pk_parse_public_key(&pk, (const uint8_t*)MSG_PUBKEY_PEM, sizeof MSG_PUBKEY_PEM) == 0 && mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, raw + body, n - body) == 0;
  mbedtls_pk_free(&pk); if (!ok) { Serial.println("msg: signature rejected"); return false; }
  uint8_t pt[256]; mbedtls_gcm_context g; mbedtls_gcm_init(&g);
  ok = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, MSG_AESKEY, 256) == 0 && mbedtls_gcm_auth_decrypt(&g, ctl, raw + 1, 12, nullptr, 0, raw + 14 + ctl, 16, raw + 14, pt) == 0;
  mbedtls_gcm_free(&g); if (!ok) return false;
  m.sent = ((uint32_t)pt[0] << 24) | ((uint32_t)pt[1] << 16) | ((uint32_t)pt[2] << 8) | pt[3];
  size_t pl = pt[4]; if (5 + pl > ctl) return false;
  m.pic = ""; for (size_t i = 0; i < pl; i++) { char c = pt[5 + i]; if (isalnum((uint8_t)c) || c == '_' || c == '-') m.pic += c; else { m.pic = ""; break; } }       // names only: no paths
  m.text = ""; for (size_t i = 5 + pl; i < ctl; i++) m.text += (char)pt[i];
  return m.text.length() > 0;
}
bool msgPoll(Msg& out) {
  String last = prefGet("msgid"), url = String("https://ntfy.sh/") + MSG_TOPIC + "/json?poll=1&since=" + (last.length() ? last : String("6h"));
  String body; if (!httpsGet(url, body, 8000)) { if (last.length() && g_dHttp == 404) prefSet("msgid", ""); return false; }
  uint32_t seen = (uint32_t)prefGet("msgt").toInt(), now = clockNow(); bool got = false; String newest = last; int p = 0;
  while (p < (int)body.length()) {
    int e = body.indexOf('\n', p); if (e < 0) e = body.length();
    String line = body.substring(p, e); p = e + 1; line.trim(); if (!line.length()) continue;
    JsonDocument f; f["id"] = true; f["event"] = true; f["message"] = true; JsonDocument d;
    if (deserializeJson(d, line, DeserializationOption::Filter(f))) continue;
    if (String(d["event"] | "") != "message") continue;
    newest = String(d["id"] | "");
    Msg m; if (!unpack(String(d["message"] | ""), m)) continue;
    if (m.sent <= seen || (now && m.sent + 48UL * 3600UL < now)) continue;          // already shown, or too old to matter
    out = m; got = true; seen = m.sent;
  }
  if (newest != last) prefSet("msgid", newest);
  if (got) prefSet("msgt", String(seen));
  Serial.printf("msg: %s\n", got ? "new message" : "none");
  return got;
}
// Picture file /msg/<name>.mg: u16 width, u16 height (little endian), then rows of 4 greys, 4 px per byte, MSB first (tools/img2lg.py).
static const int PIC_MAXW = 250, PIC_MAXH = 296;
static bool picSize(const String& name, int& w, int& h) {
  w = h = 0; if (!name.length()) return false;
  File f = SD.open((String("/msg/") + name + ".mg").c_str()); if (!f) return false;
  uint8_t hd[4]; bool ok = f.read(hd, 4) == 4; f.close(); if (!ok) return false;
  w = hd[0] | (hd[1] << 8); h = hd[2] | (hd[3] << 8); if (w < 1 || h < 1 || w > PIC_MAXW || h > PIC_MAXH) { w = h = 0; return false; } return true;
}
static void drawPic(const String& name, int x, int y) {
  size_t n = 0; uint8_t* b = storLoad((String("/msg/") + name + ".mg").c_str(), n); if (!b || n < 5) { free(b); return; }
  int w = b[0] | (b[1] << 8), h = b[2] | (b[3] << 8), stride = (w + 3) / 4; if (w < 1 || h < 1 || w > PIC_MAXW || h > PIC_MAXH || (size_t)(4 + h * stride) > n) { free(b); return; }
  for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) { int q = (b[4 + r * stride + (c >> 2)] >> (6 - 2 * (c & 3))) & 3; if (q != 3) epd.drawPixel(x + c, y + r, q == 0 ? INK : q == 1 ? GREY_D : GREY_L); }   // file values: 0 black, 1 dark grey, 2 light grey, 3 paper
  free(b);
}
// ---- Unix-desktop look: weave background, Motif-style bevelled windows ----
static void bevel(int x0, int y0, int x1, int y1, bool raised, int w) {
  uint16_t hi = raised ? PAPER : GREY_D, lo = raised ? GREY_D : PAPER;
  for (int i = 0; i < w; i++) { epd.drawFastHLine(x0 + i, y0 + i, x1 - x0 - 2 * i + 1, hi); epd.drawFastVLine(x0 + i, y0 + i, y1 - y0 - 2 * i + 1, hi); epd.drawFastHLine(x0 + i, y1 - i, x1 - x0 - 2 * i + 1, lo); epd.drawFastVLine(x1 - i, y0 + i, y1 - y0 - 2 * i + 1, lo); }
}
static void windowFrame(int x0, int y0, int x1, int y1, const char* title, int& cx0, int& cy0, int& cx1, int& cy1) {
  epd.fillRect(x0 + 7, y0 + 7, x1 - x0 + 1, y1 - y0 + 1, GREY_D);                        // shadow
  epd.fillRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1, GREY_L); bevel(x0, y0, x1, y1, true, 4);
  int a = x0 + 6, b = y0 + 6, c = x1 - 6, d = y0 + 40;                                    // title bar
  epd.fillRect(a, b, c - a + 1, d - b + 1, GREY_L); bevel(a, b, c, d, true, 2);
  epd.fillRect(a + 40, b + 3, (c - 90) - (a + 40) + 1, d - b - 5, GREY_D);
  put(&Nico14, (a + 40 + c - 90) / 2 - tw(&Nico14, title) / 2, (b + d) / 2 + 5, title, PAPER);
  int bx[3] = {a + 3, c - 84, c - 42};
  for (int k = 0; k < 3; k++) { epd.fillRect(bx[k], b + 3, 37, d - b - 5, GREY_L); bevel(bx[k], b + 3, bx[k] + 36, d - 3, true, 2); }
  epd.fillRect(a + 12, b + 14, 16, 6, INK); epd.drawRect(c - 72, b + 11, 18, 12, INK); epd.drawRect(c - 33, b + 8, 21, 19, INK); epd.drawRect(c - 32, b + 9, 19, 17, INK);
  cx0 = x0 + 10; cy0 = y0 + 46; cx1 = x1 - 10; cy1 = y1 - 10;
  epd.fillRect(cx0, cy0, cx1 - cx0 + 1, cy1 - cy0 + 1, PAPER); bevel(cx0, cy0, cx1, cy1, false, 2);
}
void msgDraw(const Msg& m) {
#ifdef FF_GREY
  g_grey = true; pageBegin(false, true);
  const AFont* BIG = &AA30; auto W = [&](const String& s) { return aaWidth(BIG, s); }; auto D = [&](int x, int y, const String& s) { aaDraw(BIG, x, y, s, INK, false); }; int LH = BIG->yAdv + 8;
#else
  pageBegin(true, true);
  auto W = [&](const String& s) { return tw(&Nico17, s); }; auto D = [&](int x, int y, const String& s) { put(&Nico17, x, y, s); }; int LH = 28;
#endif
  int pw = 0, ph = 0; picSize(m.pic, pw, ph);
  do {
    epd.fillScreen(GREY_L);
    for (int k = 0; k < 800 + 480; k += 4) epd.drawLine(k, 0, k - 479, 479, GREY_D);            // the classic X desktop weave
    int a, b, c, d;
    windowFrame(14, 10, 610, 300, "xclock", a, b, c, d);
    windowFrame(56, 40, 650, 330, "files", a, b, c, d);
    put(&Nico13, a + 10, b + 20, "drwxr-xr-x  park", INK); put(&Nico13, a + 10, b + 42, "-rw-r--r--  door.sys", INK); put(&Nico13, a + 10, b + 64, "-rwxr-xr-x  run.sh", INK);
    windowFrame(100, 70, 770, 456, "message", a, b, c, d);
    int sb = d - 26; epd.fillRect(a, sb, c - a + 1, 27, GREY_L); bevel(a, sb, c, d, false, 2);       // status bar
    put(&Nico14, a + 10, sb + 18, String("sent ") + clockFmt(m.sent), INK); String ex = "press BOOT to close"; put(&Nico14, c - 10 - tw(&Nico14, ex), sb + 18, ex, INK);
    if (pw) drawPic(m.pic, a + 18, b + (sb - b - ph) / 2);
    int tx = pw ? a + 18 + pw + 28 : a + 24, y = b + 52, lines = 0, maxLines = 6;
    String line = "", word = "", t = m.text + " ";
    auto flush = [&]() { if (lines < maxLines && line.length()) { D(tx, y, line); y += LH; lines++; } line = ""; };
    for (size_t i = 0; i < t.length(); i++) {
      char ch = t[i];
      if (ch == ' ' || ch == '\n') { if (word.length()) { String tryL = line.length() ? line + " " + word : word; if (W(tryL) > c - tx - 14 && line.length()) { flush(); line = word; } else line = tryL; word = ""; } if (ch == '\n') flush(); }
      else word += ch;
    }
    flush();
  } while (pageNext());
}
