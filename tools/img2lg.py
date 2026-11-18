#!/usr/bin/env python3
"""Turn a picture into a message picture for the frame (.mg: 4 greys, dithered, at most 250 x 296 px).
Usage: img2lg.py picture.png out.mg      Put out.mg on the SD card as /msg/<name>.mg, then: sendmsg.py "text" --pic <name>"""
import sys, numpy as np
from PIL import Image
src, out = sys.argv[1], sys.argv[2]
LV = np.array([0, 82, 173, 255], float)                              # black, dark grey, light grey, paper (as the panel shows them)
im = Image.open(src).convert("RGBA"); bg = Image.new("RGBA", im.size, "white"); bg.alpha_composite(im); im = bg.convert("L")
im.thumbnail((250, 296), Image.LANCZOS); a = np.asarray(im).astype(float)
lo, hi = np.percentile(a, 1), np.percentile(a, 99)
if hi > lo + 10: a = np.clip((a - lo) / (hi - lo) * 255, 0, 255)
h, w = a.shape; q = np.zeros((h, w), np.uint8)
for y in range(h):                                                   # Floyd-Steinberg error diffusion
    for x in range(w):
        o = a[y, x]; i = int(np.argmin(np.abs(LV - o))); q[y, x] = i; e = o - LV[i]
        if x + 1 < w: a[y, x + 1] += e * 7 / 16
        if y + 1 < h:
            if x > 0: a[y + 1, x - 1] += e * 3 / 16
            a[y + 1, x] += e * 5 / 16
            if x + 1 < w: a[y + 1, x + 1] += e / 16
rows = bytearray()
for r in range(h):
    row = list(q[r]) + [3] * ((-w) % 4)
    for i in range(0, len(row), 4): rows.append((row[i] << 6) | (row[i + 1] << 4) | (row[i + 2] << 2) | row[i + 3])
open(out, "wb").write(w.to_bytes(2, "little") + h.to_bytes(2, "little") + bytes(rows)); print("wrote", out, "%dx%d, %d bytes" % (w, h, 4 + len(rows)))
