"""Natural Earth 10m land (public domain) -> compact coastline file for the SD card: /map/land.bin
Region: UK + Ireland. Rings of int16 pairs: x=(lon+12)*2000, y=(lat-49)*2500. Even-odd fill handles lakes/holes.
This is a whole-region map, so it reveals nothing about where the frame lives."""
import json, struct, sys
from shapely.geometry import shape, box
src, out, tol = "/tmp/claude-0/-home-claude/87ee4205-97d5-5eb5-9b0e-5c095f8ffc16/scratchpad/live/land10.geojson", sys.argv[1], float(sys.argv[2])
g = json.load(open(src)); land = None
from shapely.ops import unary_union
land = unary_union([shape(f["geometry"]) for f in g["features"]]).intersection(box(-12, 49, 3, 61.5))
land = land.simplify(tol, preserve_topology=True)
rings = []
for poly in getattr(land, "geoms", [land]):
    if poly.geom_type != "Polygon": continue
    for r in [poly.exterior, *poly.interiors]:
        pts = [(int(round((x + 12) * 2000)), int(round((y - 49) * 2500))) for x, y in r.coords]
        if len(pts) >= 4: rings.append(pts)
with open(out, "wb") as f:
    f.write(b"FFL1" + struct.pack("<H", len(rings)))
    for pts in rings:
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        f.write(struct.pack("<HhhhhH", len(pts), min(xs), min(ys), max(xs), max(ys), 0))
        for x, y in pts: f.write(struct.pack("<hh", x, y))
print(len(rings), "rings,", sum(len(r) for r in rings), "points ->", out)
