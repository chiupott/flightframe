#!/usr/bin/env python3
"""Pre-render every plane picture the window view needs into one file for the SD card (/sprites.bin).
Classes x viewing angle (0..180 deg in 15 steps; left-flying planes are mirrored by the frame) x looking-up angle (0/15/30) x 5 sizes.
File: 'FFSP', count u16, then count x [key u16, offset u32, w u8, h u8] sorted by key, then the pictures (1 bit/pixel, rows MSB first, 1 = ink).
key = ((cls*13 + yaw/15)*3 + pitch/15... see KEY below)   Usage: bake_planes.py out/sprites.bin"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import plane3d

CLASSES = ["narrow", "wide", "quad", "jumbo", "prop", "biz"]
YAWS = list(range(0, 181, 15))          # 13
PITCHES = [0, 15, 30]                   # 3
SIZES = [36, 52, 72, 96, 120]           # plane length in px

def key(c, y, p, s): return ((c * len(YAWS) + y) * len(PITCHES) + p) * len(SIZES) + s

def main(out):
    items = []
    for c, cls in enumerate(CLASSES):
        for yi, yaw in enumerate(YAWS):
            for pi, pit in enumerate(PITCHES):
                for si, L in enumerate(SIZES):
                    im = plane3d.sprite(cls, yaw, pit, L)
                    a = np.asarray(im) > 110
                    ys, xs = np.nonzero(a)
                    if not len(xs): a = np.zeros((1, 1), bool)
                    else: a = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
                    h, w = a.shape
                    items.append((key(c, yi, pi, si), w, h, np.packbits(a, axis=1).tobytes()))
        print("[bake]", cls, flush=True)
    items.sort()
    head = 6 + 8 * len(items); off = head; table = b""; data = b""
    for k, w, h, d in items:
        table += struct.pack("<HIBB", k, off + len(data), min(w, 255), min(h, 255)); data += d
    open(out, "wb").write(b"FFSP" + struct.pack("<H", len(items)) + table + data)
    print(f"[bake] {len(items)} pictures, {os.path.getsize(out) / 1024:.0f} KB -> {out}")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "sprites.bin")
