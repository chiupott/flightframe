"""OurAirports (public domain) -> airports.bin (FFA1): large airports, plus medium ones with scheduled service, with their runways. Worldwide, ~110 KB.
Format: "FFA1", u32 n; per airport: i32 lat, i32 lon (1e-4 deg), char[4] code (IATA, else ICAO), u8 flags (bit0 = large), u8 nRunways, then nRunways x (i32 leLat, leLon, heLat, heLon).
Usage: bake_airports.py airports.csv runways.csv airports.bin   (csv files: https://davidmegginson.github.io/ourairports-data/)"""
import csv, struct, sys
ap = {r["ident"]: r for r in csv.DictReader(open(sys.argv[1], encoding="utf-8"))}
rw = {}
for r in csv.DictReader(open(sys.argv[2], encoding="utf-8")):
    if r["closed"] == "1" or not (r["le_latitude_deg"] and r["he_latitude_deg"] and r["le_longitude_deg"] and r["he_longitude_deg"]): continue
    rw.setdefault(r["airport_ident"], []).append(r)
recs = []
for ident, a in ap.items():
    big = a["type"] == "large_airport"
    if not (big or (a["type"] == "medium_airport" and a["scheduled_service"] == "yes")) or ident not in rw: continue
    code = (a["iata_code"] or ident)[:4].ljust(4, "\0").encode("ascii", "replace")
    runs = rw[ident][:6]
    b = struct.pack("<ii4sBB", round(float(a["latitude_deg"]) * 1e4), round(float(a["longitude_deg"]) * 1e4), code, 1 if big else 0, len(runs))
    for r in runs: b += struct.pack("<iiii", *(round(float(r[k]) * 1e4) for k in ("le_latitude_deg", "le_longitude_deg", "he_latitude_deg", "he_longitude_deg")))
    recs.append(b)
with open(sys.argv[3], "wb") as f: f.write(b"FFA1" + struct.pack("<I", len(recs)) + b"".join(recs))
print(len(recs), "airports ->", sys.argv[3], sum(map(len, recs)) + 8, "bytes")
