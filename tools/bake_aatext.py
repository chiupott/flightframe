"""Anti-aliased UI text for the grey build: replaces the NicoClean pixel fonts with 2-bit (4 grey level) glyphs, same cap heights.
Font: Barlow Semi Condensed SemiBold (SIL Open Font License). Usage: bake_aatext.py BarlowSemiCondensed-SemiBold.ttf src/fonts/AAText.h [preview.png]
Glyph slots: 0x20..0x7E ASCII, 0x7F degree, 0x80 arrow, 0x81 en dash, 0x82 middle dot (same slots the Nico fonts + draw.cpp fx() use)."""
import sys
from PIL import Image, ImageDraw, ImageFont
TTF, OUT = sys.argv[1], sys.argv[2]
PREV = sys.argv[3] if len(sys.argv) > 3 else None
# name, target cap height (px, same as the Nico font it replaces), italic, yAdvance
# optional 5th field: only bake these characters (for big display text, keeps flash small)
FONTS = [("AA12", 9, 0, 14), ("AA13", 9, 0, 15), ("AA14", 11, 0, 17), ("AA15", 12, 0, 18), ("AA16", 12, 0, 18), ("AA17", 13, 0, 20), ("AAI14", 11, 1, 17), ("AA30", 22, 0, 34), ("AA36", 26, 0, 40), ("AAT72", 56, 0, 76, "Hi, Josh.")]
CAPR = 0.70   # Barlow cap height / em
CODES = list(range(0x20, 0x83)); CH = {c: chr(c) for c in range(0x20, 0x7F)}; CH.update({0x7F: "°", 0x81: "–", 0x82: "·"})
def q4(v): return 3 if v > 0.70 else 2 if v > 0.42 else 1 if v > 0.16 else 0
out = ["// Anti-aliased UI fonts (tools/bake_aatext.py). Pixels are 2 bits, MSB first: 0 paper, 1 light grey, 2 dark grey, 3 black.", "#pragma once", '#include "../fonttypes.h"']
prev = []
for name, cap, ital, yadv, *only in FONTS:
    px = round(cap / CAPR * 2) / 2; f = ImageFont.truetype(TTF, px); bits = bytearray(); gl = []; pad = 8
    for code in CODES:
        if only and code != 0x20 and chr(code) not in only[0]: gl.append((len(bits), 0, 0, 0, 0, 0)); continue
        S = 4   # supersample for smoother edges
        fb = ImageFont.truetype(TTF, px * S)
        if code == 0x80: ch = None
        elif code in CH: ch = CH[code]
        else: ch = None
        W, H = int(px * 3), int(px * 3); base = int(px * 2)
        im = Image.new("L", (W * S, H * S), 0); d = ImageDraw.Draw(im)
        adv = 0
        if code == 0x80:                                        # arrow: drawn, the font may not have one
            L = round(px * 0.72); ay = base - round(cap * 0.5)
            d.line([(pad * S, ay * S), ((pad + L) * S, ay * S)], fill=255, width=round(1.5 * S))
            hs = round(px * 0.26)
            d.polygon([((pad + L + 1) * S, ay * S), ((pad + L - hs) * S, (ay - hs) * S), ((pad + L - hs) * S, (ay + hs) * S)], fill=255)
            adv = L + 4
        elif ch is not None:
            d.text((pad * S, base * S), ch, font=fb, fill=255, anchor="ls"); adv = int(round(fb.getlength(ch) / S))
        if ital:
            im = im.transform(im.size, Image.AFFINE, (1, 0.2, -0.2 * base * S, 0, 1, 0), resample=Image.BICUBIC)
        im = im.resize((W, H), Image.BOX)
        bb = im.point(lambda v: 255 if v > 40 else 0).getbbox()
        if code == 0x20: adv = int(round(fb.getlength(" ") / S)) if True else 3
        if not bb: gl.append((len(bits), 0, 0, 0, 0, adv)); continue
        x0, y0, x1, y1 = bb; w, h = x1 - x0, y1 - y0; off = len(bits); cur = 0; n = 0
        for y in range(y0, y1):
            for x in range(x0, x1):
                cur = (cur << 2) | q4(im.getpixel((x, y)) / 255.0); n += 1
                if n == 4: bits.append(cur); cur = 0; n = 0
        if n: bits.append(cur << (2 * (4 - n)))
        gl.append((off, w, h, x0 - pad, y0 - base, adv))
    out.append(f"static const uint8_t {name}_BITS[] = {{" + ",".join(map(str, bits)) + "};")
    out.append(f"static const AGlyph {name}_G[] = {{" + ",".join("{%d,%d,%d,%d,%d,%d}" % g for g in gl) + "};")
    out.append(f"extern const AFont {name} = {{{name}_BITS, {name}_G, 0x20, 0x82, {yadv}}};")
    print(name, px, "bytes", len(bits))
    prev.append((name, gl, bits))
open(OUT, "w").write("\n".join(out) + "\n")
if PREV:   # preview on the panel's real grey levels, 3x
    PAL = {0: 255, 1: 173, 2: 82, 3: 0}; samples = ["BAW123  A320  → LHR  (12.4 mi)", "35,000 ft · 450 kt · G-EUUU", "Rolls-Royce 1,234  Window · facing 270° · 52 m up"]
    rows = []; Wd = 700
    img = Image.new("L", (Wd, 26 * len(prev) * 3 + 20), 255); y = 10
    for name, gl, bits in prev:
        for s in samples[:2] if name != "AA17" else samples:
            x = 6
            for chh in s:
                code = {"→": 0x80, "–": 0x81, "·": 0x82, "°": 0x7F}.get(chh, ord(chh))
                off, w, h, xo, yo, adv = gl[code - 0x20]; idx = 0
                for r in range(h):
                    for c in range(w):
                        q = (bits[off + (idx >> 2)] >> (6 - 2 * (idx & 3))) & 3; idx += 1
                        if q: img.putpixel((x + xo + c, y + 20 + yo + r), PAL[q])
                x += adv
            y += 24
    img = img.crop((0, 0, 400, y)); img.resize((img.width * 3, img.height * 3), Image.NEAREST).save(PREV)
