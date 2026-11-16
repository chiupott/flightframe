"""Airline marks in 4 grey levels for the grey build, from the soaring-symbols SVG collection (code MIT; the marks stay the airlines' trademarks).
Output: /logos/<ICAO>.lg files, 44x44 max: byte w, byte h|0x80 (high bit = 2 bits per pixel), then rows packed 4 px/byte MSB first, values 0 paper, 1 light grey, 2 dark grey, 3 black.
Usage: bake_logos2.py <soaring-symbols checkout> <out dir> [contact-sheet.png]"""
import sys, os, io, json
import numpy as np
from PIL import Image
import resvg_py
SRC, OUT = sys.argv[1], sys.argv[2]; SHEET = sys.argv[3] if len(sys.argv) > 3 else None
S = 44; SS = 4
os.makedirs(OUT, exist_ok=True)
air = json.load(open(os.path.join(SRC, "airlines.json")))
def render(svg):
    png = bytes(resvg_py.svg_to_bytes(svg_path=svg, width=S * SS * 2))          # resvg scales to this width
    im = Image.open(io.BytesIO(png)).convert("RGBA"); bg = Image.new("RGBA", im.size, "white"); bg.alpha_composite(im); return bg.convert("RGB")
def grey4(im):
    im.thumbnail((S, S), Image.LANCZOS)
    can = Image.new("RGB", (S, S), "white"); can.paste(im, ((S - im.width) // 2, (S - im.height) // 2))
    lum = np.asarray(can.convert("L")).astype(float) / 255.0
    sat = np.asarray(can.convert("HSV"))[..., 1].astype(float) / 255.0
    v = np.minimum(lum, 1 - 0.55 * sat * (1 - lum * 0.2))                  # saturated colours (red, blue) read darker than their grey value
    q = np.where(v < 0.22, 3, np.where(v < 0.50, 2, np.where(v < 0.80, 1, 0))).astype(np.uint8)
    ys, xs = np.where(q > 0)
    if len(xs) == 0: return None
    q = q[ys.min():ys.max() + 1, xs.min():xs.max() + 1]; return q
def pack(q):
    h, w = q.shape; rows = bytearray()
    for r in range(h):
        row = list(q[r]) + [0] * ((-w) % 4)
        for i in range(0, len(row), 4): rows.append((row[i] << 6) | (row[i + 1] << 4) | (row[i + 2] << 2) | row[i + 3])
    return bytes([w, h | 0x80]) + bytes(rows)
n = 0; tiles = []
for a in air:
    svg = os.path.join(SRC, "assets", a["slug"], "icon.svg")
    if not os.path.exists(svg) or not a.get("icao"): continue
    try: q = grey4(render(svg))
    except Exception as e: print("skip", a["slug"], e); continue
    if q is None or q.shape[0] < 12: print("skip", a["slug"], "empty/small"); continue
    open(os.path.join(OUT, a["icao"] + ".lg"), "wb").write(pack(q)); n += 1; tiles.append((a["icao"], q))
print(n, "marks")
if SHEET:
    PAL = {0: 255, 1: 173, 2: 82, 3: 0}; cols = 12; rows_ = (len(tiles) + cols - 1) // cols
    sh = Image.new("L", (cols * 50, rows_ * 50), 255)
    for i, (ic, q) in enumerate(tiles):
        im = Image.fromarray(np.vectorize(PAL.get)(q).astype(np.uint8)); sh.paste(im, ((i % cols) * 50 + 3, (i // cols) * 50 + 3))
    sh.resize((sh.width * 2, sh.height * 2), Image.NEAREST).save(SHEET)
