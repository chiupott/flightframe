#!/usr/bin/env python3
"""Live preview: runs the real firmware drawing code on this computer with live plane and rain data and serves the
800x480 screen at http://localhost:8765 (refreshes every 30 s). Nothing leaves this computer except the data requests.

  python3 tools/live_preview.py --lat 51.5074 --lon -0.1278 --assets ~/Documents/JoshProject/preview

--assets must hold arduino/ArduinoCore-API/api and sd/{logos,map/land.bin}; the Adafruit GFX and ArduinoJson sources are taken
from the PlatformIO libraries of this project (.pio/libdeps). Your location is only used for the requests and is never stored."""
import argparse, json, math, os, shutil, subprocess, sys, threading, time, urllib.request, http.server, functools
from datetime import datetime
FW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UA = {"User-Agent": "FlightFrame-preview/1.0 (+https://github.com/chiupott/flightframe)"}
def get(url, timeout=20):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout) as r: return r.read()
def dist_nm(a, b, c, d):
    p = math.radians; x = (p(c) - p(a)) * math.cos(p((a + c) / 2)); y = p(d) - p(b); return 3440.065 * math.hypot(x, y)
def build(A, work):
    libs = os.path.join(FW, ".pio", "libdeps", "esp32"); gfx = os.path.join(libs, "Adafruit GFX Library"); aj = os.path.join(libs, "ArduinoJson", "src")
    core = os.path.join(A, "arduino", "ArduinoCore-API"); api = os.path.join(core, "api")
    srcs = [os.path.join(FW, "sim", "sim.cpp")] + [os.path.join(FW, "src", f + ".cpp") for f in "store geo adsb draw main icons view map".split()] + \
           [os.path.join(gfx, "Adafruit_GFX.cpp")] + [os.path.join(api, f) for f in ("String.cpp", "Print.cpp", "Common.cpp")]
    out = os.path.join(work, "sim")
    if os.path.exists(out) and os.path.getmtime(out) > max(os.path.getmtime(s) for s in srcs): return out
    cmd = ["c++", "-std=c++17", "-O1", "-w", "-I" + os.path.join(FW, "sim"), "-I" + os.path.join(FW, "include"), "-I" + os.path.join(FW, "src"), "-I" + core, "-I" + aj, "-I" + gfx,
           *srcs, "-DFW_VERSION=\"sim\"", "-DARDUINO=10819", "-o", out]
    print("building the simulator ..."); subprocess.run(cmd, check=True); return out
PAGE = """<!doctype html><meta charset=utf-8><title>Flight Frame live preview</title><meta name=viewport content="width=device-width,initial-scale=1">
<style>body{margin:0;background:#1d1f1c;color:#cfd3cc;font:15px system-ui;display:grid;place-items:center;min-height:100vh}
main{padding:24px;max-width:860px}.frame{background:#111;padding:22px;border-radius:10px;box-shadow:0 10px 40px #0008}
img{display:block;width:100%;image-rendering:pixelated;background:#e6e6df}p{margin:14px 2px 0;line-height:1.5}b{color:#fff}</style>
<main><div class=frame><img id=s src=screen.png></div><p id=t>Starting…</p>
<p style="opacity:.6">Exactly what the firmware draws (800×480, black and white). It cannot show the real panel's refresh flash, ghosting or contrast. Planes: adsb.fi · Rain: RainViewer.</p></main>
<script>async function u(){try{const j=await (await fetch('status.json?'+Date.now())).json();
document.getElementById('s').src='screen.png?'+j.t;document.getElementById('t').innerHTML='<b>'+j.planes+' aircraft</b> within '+j.radius+' mi · updated '+j.time+' · rain radar '+j.rain+(j.err?' · <span style=color:#f88>'+j.err+'</span>':'')}catch(e){}}
u();setInterval(u,10000)</script>"""
def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--lan', action='store_true', help='also serve on the local network (for opening on a phone)')
    ap.add_argument("--lat", type=float, required=True); ap.add_argument("--lon", type=float, required=True)
    ap.add_argument("--assets", required=True); ap.add_argument("--port", type=int, default=8765); ap.add_argument("--radius", type=float, default=50)
    ap.add_argument("--head", default="0"); ap.add_argument("--once", action="store_true")
    a = ap.parse_args(); A = os.path.expanduser(a.assets); work = os.path.join(A, "work"); www = os.path.join(A, "www"); sd = os.path.join(A, "sd")
    for d in (work, www, os.path.join(sd, "map")): os.makedirs(d, exist_ok=True)
    sim = build(A, work); open(os.path.join(www, "index.html"), "w").write(PAGE)
    if not a.once:
        h = functools.partial(http.server.SimpleHTTPRequestHandler, directory=www); h.log_message = lambda *x: None
        threading.Thread(target=http.server.ThreadingHTTPServer(("0.0.0.0" if a.lan else "127.0.0.1", a.port), h).serve_forever, daemon=True).start()
        print(f"open http://localhost:{a.port}")
    hist, routes, rain_t, rain_age = [], {}, 0, "none"
    env = dict(os.environ, SIM_LAT=str(a.lat), SIM_LON=str(a.lon), SIM_CLOCK="live", SIM_HEAD=a.head)
    from PIL import Image
    while True:
        err = ""; n = 0
        try:
            nm = int(a.radius * 0.868976) + 1
            body = get(f"https://opendata.adsb.fi/api/v3/lat/{a.lat:.4f}/lon/{a.lon:.4f}/dist/{nm}").decode()
            ac = json.loads(body).get("ac", []); hist = (hist + [body])[-10:]
            near = sorted([x for x in ac if x.get("lat") is not None and str(x.get("flight", "")).strip()], key=lambda x: dist_nm(a.lat, a.lon, x["lat"], x["lon"]))[:5]
            for x in near:                                        # routes for the rows shown, cached by callsign
                cs = x["flight"].strip()
                if cs in routes: continue
                try:
                    r = json.loads(get("https://api.adsbdb.com/v0/callsign/" + cs, 10))["response"]["flightroute"]; routes[cs] = [r["origin"]["iata_code"], r["destination"]["iata_code"]]
                except Exception: routes[cs] = None
            json.dump({k: v for k, v in routes.items() if v}, open(os.path.join(work, "routes.json"), "w")); n = len(ac)
            for i, b in enumerate(hist): open(os.path.join(work, f"snap_{i}.json"), "w").write(b)
        except Exception as e: err = f"plane data: {e}"
        if time.time() - rain_t > 600:
            try:
                open(os.path.join(sd, "map", "rain.bin"), "wb").write(get("https://github.com/chiupott/flightframe/releases/download/rain/rain.bin", 30)); rain_t = time.time()
            except Exception as e: err = (err + " " if err else "") + f"rain: {e}"
        if rain_t:
            import struct; d = open(os.path.join(sd, "map", "rain.bin"), "rb").read(14); rain_age = f"{int((time.time() - struct.unpack('<I', d[10:14])[0]) / 60)} min old"
        if hist:
            r = subprocess.run([sim, work, str(len(hist)), os.path.join(work, "out.pbm")], cwd=work, env=env, capture_output=True, text=True)
            if r.returncode == 0: Image.open(os.path.join(work, "out.pbm")).convert("L").save(os.path.join(www, "screen.png"))
            else: err += " sim failed"
        json.dump(dict(t=int(time.time()), planes=n, radius=int(a.radius), time=datetime.now().strftime("%H:%M:%S"), rain=rain_age, err=err), open(os.path.join(www, "status.json"), "w"))
        print(datetime.now().strftime("%H:%M:%S"), n, "aircraft", err or "ok")
        if a.once: return
        time.sleep(30)
if __name__ == "__main__": main()
