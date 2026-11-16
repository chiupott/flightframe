#!/usr/bin/env python3
"""Packs a folder of <ICAO>.lg airline logos into one file for the private assets repo.
Format: 'FFLP', count u16, then count x [code 4 bytes NUL-padded, length u16, data]. The frame unpacks it to /logos/<code>.lg on its SD card.
The pack is uploaded to the 'sdpack' release; the frame downloads it on first boot.   Usage: pack_logos.py <logos dir> <out file>"""
import os, struct, sys
src, out = sys.argv[1], sys.argv[2]
files = sorted(f for f in os.listdir(src) if f.endswith(".lg") and len(f) <= 8)
with open(out, "wb") as o:
    o.write(b"FFLP" + struct.pack("<H", len(files)))
    for f in files:
        d = open(os.path.join(src, f), "rb").read()
        o.write(f[:-3].encode().ljust(4, b"\0") + struct.pack("<H", len(d)) + d)
print(len(files), "logos ->", out, os.path.getsize(out), "bytes")
