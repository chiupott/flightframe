"""Anti-aliased big digits for the engines view (people total): digits, comma, point in 4 grey levels, 2 bits per pixel.
Font: Barlow Condensed SemiBold (SIL Open Font License, Google Fonts). Usage: bake_bignum.py BarlowCondensed-SemiBold.ttf src/fonts/BigNum.h"""
import sys
from PIL import Image, ImageDraw, ImageFont
CH = "0123456789,."
out = ["// Big anti-aliased digits (tools/bake_bignum.py). Pixels are 2 bits: 0 paper, 1 light grey, 2 dark grey, 3 black.", "#pragma once", "#include <stdint.h>",
       "struct BGlyph { uint16_t off; uint8_t w, h; int8_t xo, yo; uint8_t adv; };", "struct BigFont { const uint8_t* bits; const BGlyph* g; uint8_t capH; };"]
for name, px in (("BN0", 66), ("BN1", 50)):
    f = ImageFont.truetype(sys.argv[1], px); bits = bytearray(); gl = []
    cap = f.getbbox("0", anchor="ls")  # (l, t, r, b) relative to baseline
    for c in CH:
        im = Image.new("L", (px * 2, px * 2), 0); d = ImageDraw.Draw(im); d.text((px // 2, px + px // 2), c, font=f, fill=255, anchor="ls")
        bb = im.getbbox(); adv = int(round(f.getlength(c)))
        if not bb: gl.append((len(bits), 0, 0, 0, 0, adv)); continue
        x0, y0, x1, y1 = bb; w, h = x1 - x0, y1 - y0; off = len(bits); cur = 0; n = 0
        for y in range(y0, y1):
            for x in range(x0, x1):
                v = im.getpixel((x, y)) / 255.0; q = 3 if v > 0.78 else 2 if v > 0.5 else 1 if v > 0.18 else 0
                cur = (cur << 2) | q; n += 1
                if n == 4: bits.append(cur); cur = 0; n = 0
        if n: bits.append(cur << (2 * (4 - n)))
        gl.append((off, w, h, x0 - px // 2, y0 - (px + px // 2), adv))
    out.append(f"static const uint8_t {name}_BITS[] = {{" + ",".join(map(str, bits)) + "};")
    out.append(f"static const BGlyph {name}_G[] = {{" + ",".join("{%d,%d,%d,%d,%d,%d}" % g for g in gl) + "};")
    out.append(f"static const BigFont {name} = {{{name}_BITS, {name}_G, {-cap[1]}}};")
    print(name, px, "bytes", len(bits), "capH", -cap[1])
open(sys.argv[2], "w").write("\n".join(out) + "\n")
