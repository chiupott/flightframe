"""Bake airline logos to 44x44 1-bit files for the SD card (/logos/<ICAO>.lg: byte w, byte h, then packed rows MSB first, 1 = ink). Private: never commit to the public repo."""
import os, sys, numpy as np
from PIL import Image
SRC = "/home/claude/jxck-s/airline-logos/flightaware_logos"
OUT = sys.argv[1]; S = 44
os.makedirs(OUT, exist_ok=True)
def bake(p):
    im = Image.open(p).convert("RGBA"); bg = Image.new("RGBA", im.size, "white"); bg.alpha_composite(im)
    im = bg.convert("RGB"); im.thumbnail((S, S), Image.LANCZOS)
    can = Image.new("RGB", (S, S), "white"); can.paste(im, ((S - im.width) // 2, (S - im.height) // 2)); im = can
    hsv = np.array(im.convert("HSV")).astype(float); lum = np.array(im.convert("L")).astype(float)
    g = 255 - np.clip(np.maximum(255 - lum, hsv[..., 1] * 0.85), 0, 255)
    h = np.bincount(g.astype("uint8").ravel(), minlength=256).astype(float); pr = h / h.sum(); w = np.cumsum(pr); m = np.cumsum(pr * np.arange(256))
    t = int(np.argmax((m[-1] * w - m) ** 2 / (w * (1 - w) + 1e-9)))
    return Image.fromarray(((g >= t) * 255).astype("uint8")).convert("1")
n = 0
for f in sorted(os.listdir(SRC)):
    if f.endswith(".png"):
        im = bake(os.path.join(SRC, f)); a = np.asarray(im.convert("L")) < 128          # ink = dark
        open(os.path.join(OUT, f[:-4] + ".lg"), "wb").write(bytes([a.shape[1], a.shape[0]]) + np.packbits(a, axis=1).tobytes()); n += 1
print(n, "logos")
