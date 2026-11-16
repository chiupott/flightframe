"""Natural Earth 10m land (public domain) -> /map/land2.bin (FFL2), north-west Europe: lon -11..16, lat 42..62.
Format: "FFL2", u32 rings; per ring: u32 npts, i32 minLon, minLat, maxLon, maxLat (1e-4 deg), then npts x (i32 lon, i32 lat).
Whole-region map: reveals nothing about where a frame lives. Even-odd fill handles lakes/holes.
Usage: bake_land2.py ne_10m_land.geojson land2.bin [tolerance_deg=0.0005]"""
import json, struct, sys
from shapely.geometry import shape, box
from shapely.ops import unary_union
src, out = sys.argv[1], sys.argv[2]; tol = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0005
g = json.load(open(src))
land = unary_union([shape(f["geometry"]) for f in g["features"]]).intersection(box(-11, 42, 16, 62)).simplify(tol, preserve_topology=True)
rings = []
for poly in getattr(land, "geoms", [land]):
    if poly.geom_type != "Polygon": continue
    for r in [poly.exterior, *poly.interiors]:
        pts = [(int(round(x * 1e4)), int(round(y * 1e4))) for x, y in r.coords]
        if len(pts) >= 4: rings.append(pts)
with open(out, "wb") as f:
    f.write(b"FFL2" + struct.pack("<I", len(rings)))
    for pts in rings:
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        f.write(struct.pack("<Iiiii", len(pts), min(xs), min(ys), max(xs), max(ys)))
        for x, y in pts: f.write(struct.pack("<ii", x, y))
print(len(rings), "rings,", sum(len(r) for r in rings), "points ->", out)
