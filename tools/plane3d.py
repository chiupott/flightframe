#!/usr/bin/env python3
"""Tiny 3D aircraft models -> 1-bit silhouette sprites, seen from any direction (used offline; the frame only blits the baked pictures).
Model axes: X right wing, Y nose, Z up; length of the plane is 1.0.
View: yaw = angle between the plane's heading and the line of sight (0 = flying away from you, 90 = flying across to your right,
180 = flying straight at you); pitch = how far you look up at it (0..45).  Left-flying planes are the mirror image."""
import math
import numpy as np
from PIL import Image, ImageDraw

def ring(y, rx, rt, rb, zc, n=16):
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n; s = math.sin(a)
        pts.append((rx * math.cos(a), y, zc + (rt if s >= 0 else rb) * s))
    return pts

def body(stations, n=16):
    rings = [ring(*s, n=n) for s in stations]; faces = []
    for a, b in zip(rings, rings[1:]):
        for i in range(n): faces.append([a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]])
    return faces

def quad(*p): return [list(v) for v in p]

def wing(side, root_x, root_le, root_c, tip_x, tip_le, tip_c, z0, dz, t=0.012):
    rx, tx = side * root_x, side * tip_x
    top = quad((rx, root_le, z0 + t), (tx, tip_le, z0 + dz + t), (tx, tip_le - tip_c, z0 + dz + t), (rx, root_le - root_c, z0 + t))
    bot = quad((rx, root_le, z0 - t), (tx, tip_le, z0 + dz - t), (tx, tip_le - tip_c, z0 + dz - t), (rx, root_le - root_c, z0 - t))
    le = quad((rx, root_le, z0 + t), (tx, tip_le, z0 + dz + t), (tx, tip_le, z0 + dz - t), (rx, root_le, z0 - t))
    return [top, bot, le]

def fin(y_base_le, base_c, height, tip_c, sweep, z0, t=0.010):
    zt = z0 + height; yt = y_base_le - sweep
    return [quad((-t, y_base_le, z0), (-t, y_base_le - base_c, z0), (-t, yt - tip_c, zt), (-t, yt, zt)),
            quad((t, y_base_le, z0), (t, y_base_le - base_c, z0), (t, yt - tip_c, zt), (t, yt, zt))]

def pod(x, y0, y1, r, z, n=10):
    st = [(y0, .8), (y0 + .02, 1.0), (y1 - .06, 1.0), (y1, .55)]
    rings = [[(x + rr * r * math.cos(2 * math.pi * i / n), y, z + rr * r * math.sin(2 * math.pi * i / n)) for i in range(n)] for y, rr in st]
    return [[a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]] for a, b in zip(rings, rings[1:]) for i in range(n)]

def airliner(L, r, span, sweep_deg, root_c, tip_c, n_eng, eng_r, wing_z, fin_h, hump=False, upsweep=0.03):
    F = []
    F += body([(0.5, 0.012, 0.012, 0.012, 0.0), (0.46, r * .55, r * .6, r * .55, 0.0), (0.40, r * .85, r * .9, r * .85, 0.0), (0.30, r, r, r, 0.0),
               (-0.10, r, r, r, 0.0), (-0.32, r * .85, r * .85, r * .8, upsweep * .3), (-0.44, r * .5, r * .5, r * .45, upsweep), (-0.50, r * .18, r * .18, r * .18, upsweep * 1.3)])
    if hump:
        F += body([(0.36, r * .2, r * .3, 0, r * .8), (0.30, r * .7, r * 1.1, 0, r * .55), (0.12, r * .7, r * 1.0, 0, r * .55), (0.0, r * .4, r * .6, 0, r * .55)], n=12)
    half = span / 2; sw = math.tan(math.radians(sweep_deg))
    root_le = 0.10
    for s in (-1, 1):
        F += wing(s, r * .8, root_le, root_c, half, root_le - sw * (half - r * .8), tip_c, wing_z, 0.045 * span)
        F += wing(s, 0.0, -0.40, 0.12, 0.17 * span, -0.40 - 0.32 * sw * 0.17 * span, 0.05, upsweep + 0.012, 0.0)          # tailplane
    F += fin(-0.30, 0.16, fin_h, 0.07, 0.10, upsweep * .6 + r * .5)
    if n_eng == 2: ex = [0.27 * half * 2 * 0.5]
    elif n_eng == 4: ex = [0.19 * half * 2 * .5, 0.40 * half * 2 * .5]
    else: ex = []
    ex_all = [-e for e in ex] + ex
    for e in ex_all:
        le = root_le - sw * (abs(e) - r * .8) + 0.05
        F += pod(e, le - 0.15, le + 0.03, eng_r, wing_z - eng_r * 0.8)
    return F

def bizjet():
    F = body([(0.5, 0.01, 0.01, 0.01, 0), (0.44, .04, .045, .04, 0), (0.34, .068, .07, .066, 0), (-0.1, .07, .07, .066, 0), (-0.34, .05, .05, .045, 0.012), (-0.5, .012, .012, .012, 0.02)], n=12)
    for s in (-1, 1):
        F += wing(s, .06, 0.06, .17, .43, -0.06, .06, -0.045, .02)
        F += wing(s, 0.0, -.43, .1, .13, -.50, .05, .22, 0.0)
        F += pod(s * .115, -.42, -.18, .045, .035)
    F += fin(-0.38, .14, .20, .08, .10, .03)
    return F

def prop(L=1.0):
    F = body([(0.5, .01, .01, .01, 0), (0.42, .05, .055, .05, 0), (0.30, .072, .075, .07, 0), (-0.2, .07, .07, .066, 0), (-0.45, .03, .03, .028, .02), (-0.5, .01, .01, .01, .025)], n=12)
    for s in (-1, 1):
        F += wing(s, .065, 0.07, .15, .50, 0.05, .10, .06, .0)
        F += wing(s, 0.0, -.42, .1, .17, -.46, .06, .26, 0)
        F += pod(s * .2, -.02, .20, .045, .01)
    F += fin(-0.34, .15, .22, .08, .10, .03)
    return F

MODELS = {
    "narrow": lambda: airliner(1, .065, .90, 25, .17, .05, 2, .033, -.045, .16),
    "wide": lambda: airliner(1, .072, 1.0, 31, .22, .06, 2, .046, -.052, .17),
    "jumbo": lambda: airliner(1, .075, 0.97, 37, .24, .07, 4, .035, -.05, .17, hump=True),
    "quad": lambda: airliner(1, .068, .98, 32, .22, .06, 4, .033, -.05, .17),
    "prop": prop,
    "biz": bizjet,
}

def sprite(cls, yaw, pitch, length_px, ss=6):
    """Return an 'L' mask image (255 = ink) of the plane, or None."""
    F = MODELS[cls]()
    th, p = math.radians(abs(yaw)), math.radians(pitch)
    c, s = math.cos(th), math.sin(th); cp, sp = math.cos(p), math.sin(p)
    P = []
    for f in F:
        q = []
        for (mx, my, mz) in f:
            wx = mx * c + my * s; wy = -mx * s + my * c; wz = mz
            q.append((wx, -wy * sp + wz * cp, wy * cp + wz * sp))     # screen x, screen up, depth
        P.append(q)
    sc = length_px * ss
    xs = [v[0] for q in P for v in q]; ys = [v[1] for q in P for v in q]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    pad = 2 * ss
    W = int((x1 - x0) * sc + 2 * pad) + 1; H = int((y1 - y0) * sc + 2 * pad) + 1
    im = Image.new("L", (W, H), 0); d = ImageDraw.Draw(im)
    order = sorted(range(len(P)), key=lambda i: -sum(v[2] for v in P[i]) / len(P[i]))
    for i in order:
        d.polygon([((v[0] - x0) * sc + pad, (y1 - v[1]) * sc + pad) for v in P[i]], fill=255)
    out = im.resize((max(1, W // ss), max(1, H // ss)), Image.LANCZOS)
    if yaw < 0: out = out.transpose(Image.FLIP_LEFT_RIGHT)
    return out

if __name__ == "__main__":
    import sys
    cols = [(0, 0), (30, 0), (60, 0), (90, 0), (120, 15), (150, 15), (180, 15), (90, 30)]
    rows = list(MODELS)
    cell = 120; sheet = Image.new("L", (cell * len(cols), cell * len(rows)), 255)
    for r, cls in enumerate(rows):
        for c, (yaw, pit) in enumerate(cols):
            m = sprite(cls, yaw, pit, 90)
            bm = m.point(lambda v: 0 if v > 110 else 255)
            sheet.paste(bm, (c * cell + (cell - bm.width) // 2, r * cell + (cell - bm.height) // 2))
    sheet.convert("1").save(sys.argv[1] if len(sys.argv) > 1 else "planes_sheet.png")
