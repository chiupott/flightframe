"""Turn the real flights.json capture into adsb.fi-style snapshots every 30 s; KLM36E banks through a turn so trails curve."""
import json, math, sys
src, out = sys.argv[1], sys.argv[2]
import os
LAT0, LON0 = float(os.environ.get("SIM_LAT", 51.5074)), float(os.environ.get("SIM_LON", -0.1278))   # test spot; set SIM_LAT/SIM_LON to try another
fl = json.load(open(src)); routes = {}
st = []
for f in fl:
    d = math.radians(f["dir"]); lat = LAT0 + f["nm"] * math.cos(d) / 60; lon = LON0 + f["nm"] * math.sin(d) / (60 * math.cos(math.radians(LAT0)))
    st.append(dict(f=f, lat=lat, lon=lon, trk=float(f["trk"])))
    if f["route"]: routes[f["cs"]] = f["route"]
json.dump(routes, open(out + "/routes.json", "w"))
for k in range(12):
    ac = []
    for s in st:
        f = s["f"]
        if f["cs"] == "KLM36E": s["trk"] = (s["trk"] + 4.5) % 360          # 4.5 deg per 30 s step => visible banking turn
        ac.append({"hex": f["hex"], "flight": f["cs"].ljust(8), "lat": s["lat"], "lon": s["lon"], "alt_baro": f["alt"], "gs": f["gs"], "track": s["trk"], "t": f["t"], "r": f["reg"]})
        nm = f["gs"] * 30 / 3600; a = math.radians(s["trk"])
        s["lat"] += nm * math.cos(a) / 60; s["lon"] += nm * math.sin(a) / (60 * math.cos(math.radians(LAT0)))
    json.dump({"ac": ac, "now": 0}, open(f"{out}/snap_{k}.json", "w"))
print("snapshots ok")
