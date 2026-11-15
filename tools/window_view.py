#!/usr/bin/env python3
"""Window view v2 (mock-up): what you would see out of the window, drawn the way the frame will.
 - the compass bearing runs left-right, height angle up-down, 0 deg = eye level (so the sea sits BELOW the line when the house is high up)
 - terrain: real elevation data rendered as a contour-line surface (sea as wave lines)
 - planes: tiny 3D models pre-drawn from every viewing angle (tools/plane3d.py)
Run on a computer with internet:  python3 tools/window_view.py --lat .. --lon .. --head 255 --font NicoClean-Regular.ttf --out window.png [--demo]
In the real thing the terrain step runs in the setup page on Josh's phone, so the location never goes in the public repo."""
import argparse, json, math, os, sys, time, urllib.request
from concurrent.futures import ThreadPoolExecutor
import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import plane3d

W, H = 800, 480
EYE, TOP, YB = 290, 34, 426            # y of eye level, of the top of the sky, of the bottom of the picture (compass strip below)
UP_MAX, DN_MAX, GAMMA = 35.0, 14.0, 0.5
ZOOM, R_EARTH = 10, 6371000.0
MAX_KM = 70.0

def get(url, timeout=25):
    return urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "FlightFrame-proto"}), timeout=timeout).read()

def vy(ang):
    if ang >= 0: return EYE - (EYE - TOP) * (min(ang, UP_MAX) / UP_MAX) ** GAMMA
    return EYE + (YB - EYE) * (min(-ang, DN_MAX) / DN_MAX) ** GAMMA

def load_dem(lats, lons, cache):
    n = 2 ** ZOOM * 256.0
    X = (np.array(lons) + 180.0) / 360.0 * n; Y = (1 - np.arcsinh(np.tan(np.radians(lats))) / math.pi) / 2 * n
    tx0, tx1, ty0, ty1 = int(X.min() // 256), int(X.max() // 256), int(Y.min() // 256), int(Y.max() // 256)
    os.makedirs(cache, exist_ok=True)
    def tile(t):
        tx, ty = t; p = os.path.join(cache, f"{ZOOM}_{tx}_{ty}.png")
        if not os.path.exists(p): open(p, "wb").write(get(f"https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{ZOOM}/{tx}/{ty}.png"))
        a = np.asarray(Image.open(p).convert("RGB")).astype(np.float32)
        return t, a[..., 0] * 256 + a[..., 1] + a[..., 2] / 256 - 32768
    jobs = [(x, y) for y in range(ty0, ty1 + 1) for x in range(tx0, tx1 + 1)]
    mos = np.zeros(((ty1 - ty0 + 1) * 256, (tx1 - tx0 + 1) * 256), np.float32)
    with ThreadPoolExecutor(8) as ex:
        for (tx, ty), h in ex.map(tile, jobs): mos[(ty - ty0) * 256:(ty - ty0 + 1) * 256, (tx - tx0) * 256:(tx - tx0 + 1) * 256] = h
    def sample(lat, lon):
        x = (np.asarray(lon) + 180.0) / 360.0 * n - tx0 * 256; y = (1 - np.arcsinh(np.tan(np.radians(lat))) / math.pi) / 2 * n - ty0 * 256
        xi = np.clip(np.rint(x).astype(int), 0, mos.shape[1] - 1); yi = np.clip(np.rint(y).astype(int), 0, mos.shape[0] - 1)
        return np.maximum(mos[yi, xi], 0.0)
    print(f"[dem] {len(jobs)} tiles")
    return sample

def terrain(lat0, lon0, head, fov, eye_above_ground, ground_override, cache):
    """Returns (Hz, Dz, h0): per-pixel height/distance of the visible ground, NaN where it is sky."""
    az = head + (np.arange(W) / (W - 1) - 0.5) * fov
    d = 120.0 * 1.0035 ** np.arange(int(math.log(MAX_KM * 1000 / 120.0) / math.log(1.0035)))   # dense near, sparse far
    b = np.radians(az)[:, None]
    la = lat0 + d[None, :] * np.cos(b) / 111320.0; lo = lon0 + d[None, :] * np.sin(b) / (111320.0 * math.cos(math.radians(lat0)))
    dem = load_dem(np.r_[la.min(), la.max(), lat0], np.r_[lo.min(), lo.max(), lon0], cache)
    ground = float(dem(np.array(lat0), np.array(lon0))) if ground_override is None else ground_override
    h0 = ground + eye_above_ground
    print(f"[eye] ground {ground:.0f} m + {eye_above_ground:.0f} m = {h0:.0f} m above sea")
    h = dem(la, lo)
    ang = np.degrees(np.arctan2(h - h0 - d[None, :] ** 2 / (2 * R_EARTH) * 0.87, d[None, :]))
    run = np.maximum.accumulate(ang, axis=1); prev = np.concatenate([np.full((W, 1), -99.0), run[:, :-1]], axis=1)
    vis = ang > prev
    Hz = np.full((YB, W), np.nan, np.float32); Dz = np.full((YB, W), np.nan, np.float32)
    for x in range(W):
        for i in np.nonzero(vis[x])[0]:
            top = int(vy(ang[x, i])); bot = YB if prev[x, i] < -90 else int(math.ceil(vy(prev[x, i])))
            top = max(top, 0)
            if bot > top: Hz[top:bot, x] = h[x, i]; Dz[top:bot, x] = d[i]
    return Hz, Dz, h0

def shade(Hz, Dz):
    """1-bit image: contour lines on land, wave lines on sea, bold outline where the surface meets the sky or a nearer ridge."""
    im = np.ones((YB, W), bool)                               # True = paper
    ok = ~np.isnan(Hz); land = ok & (Hz > 1.0); sea = ok & ~land
    step = np.where(Dz < 12000, 25.0, np.where(Dz < 30000, 50.0, 100.0))
    k = np.floor(np.nan_to_num(Hz) / step)
    # contours: height band changes vertically or horizontally between two land pixels
    c = np.zeros_like(land)
    c[1:, :] |= land[1:, :] & land[:-1, :] & (k[1:, :] != k[:-1, :])
    c[:, 1:] |= land[:, 1:] & land[:, :-1] & (k[:, 1:] != k[:, :-1])
    far = np.nan_to_num(Dz) > 40000; c &= ~far & (np.nan_to_num(Hz) > 12)     # keep the far haze and the flat shore clean
    im[c] = False
    # sea: wave dashes, rows get tighter towards the horizon
    g = np.floor(np.log2(np.maximum(np.nan_to_num(Dz), 250) / 250.0) * 2.4)
    wv = np.zeros_like(sea); wv[1:, :] = sea[1:, :] & sea[:-1, :] & (g[1:, :] != g[:-1, :])
    xs = np.arange(W)[None, :]; wv &= ((xs + (g.astype(int) * 7)) % 11) < 6
    im[wv] = False
    # outlines
    edge = np.zeros_like(ok)
    edge[1:, :] |= ok[1:, :] & ~ok[:-1, :]                    # surface meets sky
    edge[0, :] |= ok[0, :]
    jump = np.zeros_like(ok); both = ok[1:, :] & ok[:-1, :]
    jump[1:, :] = both & (np.abs(np.nan_to_num(Dz)[1:, :] - np.nan_to_num(Dz)[:-1, :]) > 0.12 * np.nan_to_num(Dz)[1:, :]) & land[:-1, :] & land[1:, :]
    im[edge] = False
    thick = edge.copy(); thick[1:, :] |= edge[:-1, :]; im[thick & land & ~far] = False    # land tops a little bolder
    return Image.fromarray(im.astype(np.uint8) * 255).convert("1")

CLS = {"A319": "narrow", "A320": "narrow", "A321": "narrow", "A20N": "narrow", "A21N": "narrow", "B737": "narrow", "B738": "narrow", "B739": "narrow", "B38M": "narrow",
       "E190": "narrow", "E195": "narrow", "B763": "wide", "B764": "wide", "B772": "wide", "B77W": "wide", "B77L": "wide", "B788": "wide", "B789": "wide", "B78X": "wide",
       "A332": "wide", "A333": "wide", "A338": "wide", "A339": "wide", "A359": "wide", "A35K": "wide", "A343": "quad", "A345": "quad", "A346": "quad", "A388": "quad",
       "B744": "jumbo", "B748": "jumbo", "AT76": "prop", "AT75": "prop", "DH8D": "prop", "BE36": "prop", "GLF6": "biz", "GLF5": "biz", "C56X": "biz", "LJ45": "biz", "E55P": "biz"}

AIRLINES = {}
try:
    for _l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "airlines_iata.csv")): _k, _v = _l.strip().split(","); AIRLINES[_k] = _v
except OSError: pass

def flight_no(cs):                       # AFR98E -> AF98E (anything that is not 3 letters + digit stays as is)
    return AIRLINES[cs[:3]] + cs[3:] if len(cs) >= 4 and cs[:3].isalpha() and cs[3].isdigit() and cs[:3] in AIRLINES else cs

def planes(lat0, lon0, h0, head, fov, nm=45):
    out = []
    for o in json.loads(get(f"https://opendata.adsb.fi/api/v3/lat/{lat0:.4f}/lon/{lon0:.4f}/dist/{nm}")).get("ac", []):
        if o.get("lat") is None or o.get("alt_baro") is None or isinstance(o["alt_baro"], str): continue
        dy = (o["lat"] - lat0) * 111320.0; dx = (o["lon"] - lon0) * 111320.0 * math.cos(math.radians(lat0)); dist = math.hypot(dx, dy)
        if dist < 500: continue
        brg = math.degrees(math.atan2(dx, dy)) % 360; rel = (brg - head + 180) % 360 - 180
        if abs(rel) > fov / 2 + 4: continue
        el = math.degrees(math.atan2(o["alt_baro"] * 0.3048 - h0 - dist ** 2 / (2 * R_EARTH) * 0.87, dist))
        trk = o.get("track"); yaw = ((trk - brg + 180) % 360 - 180) if trk is not None else 90
        out.append(dict(cs=flight_no((o.get("flight") or o.get("hex", "")).strip()), type=o.get("t", ""), ft=int(o["alt_baro"]), dist=dist, rel=rel, el=el, yaw=yaw))
    return out

def export_pack(a, bg, h0):
    """terrain.bin for the frame: header, peak labels, then the 800x480 background as 1 bit per pixel (1 = ink), rows MSB first.
    Header: 'FFWT', version u8, fov*10 u16, heading u16, eye height above sea (m) i16, npeaks u8; per peak: x i16, y i16, height m u16, len u8, name."""
    import struct
    os.makedirs(a.export, exist_ok=True)
    pk = []
    for p in a.peak:
        nm, la, lo, hm = p.split(","); la, lo, hm = float(la), float(lo), float(hm)
        dy = (la - a.lat) * 111320.0; dx = (lo - a.lon) * 111320.0 * math.cos(math.radians(a.lat)); dist = math.hypot(dx, dy)
        rel = (math.degrees(math.atan2(dx, dy)) - a.head + 180) % 360 - 180
        if abs(rel) > a.fov / 2: continue
        el = math.degrees(math.atan2(hm - h0 - dist ** 2 / (2 * R_EARTH) * 0.87, dist))
        pk.append((int(W / 2 + rel / a.fov * (W - 1)), int(vy(el)), int(hm), nm.encode()[:24]))
    pk = pk[:6]                                   # the frame reads at most 6 peak labels
    out = b"FFWT" + struct.pack("<BHHhB", 1, int(a.fov * 10), int(a.head) % 360, int(round(h0)), len(pk))
    for x, y, hm, nm in pk: out += struct.pack("<hhHB", x, y, hm, len(nm)) + nm
    arr = (np.asarray(bg.convert("L")) < 128)
    out += np.packbits(arr, axis=1).tobytes()
    open(os.path.join(a.export, "terrain.bin"), "wb").write(out); bg.save(os.path.join(a.export, "terrain_preview.png"))
    print(f"[export] {a.export}/terrain.bin  {len(out)} bytes, {len(pk)} peaks, eye {h0:.0f} m")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lat", type=float, required=True); ap.add_argument("--lon", type=float, required=True)
    ap.add_argument("--head", type=float, default=255.0); ap.add_argument("--fov", type=float, default=110.0)
    ap.add_argument("--eye", type=float, default=6.0, help="eye height above the ground, m (a first-floor window is ~6)")
    ap.add_argument("--ground", type=float, default=None, help="override ground height above sea, m (default: from the terrain data)")
    ap.add_argument("--radius-nm", type=int, default=80, help="how far out to look for planes"); ap.add_argument("--loop", type=int, default=0, help="re-render every N seconds (terrain is only computed once)")
    ap.add_argument("--export", default=None, help="write the frame's window pack (terrain.bin) into this folder and exit")
    ap.add_argument("--font", required=True); ap.add_argument("--out", default="window.png"); ap.add_argument("--demo", action="store_true")
    ap.add_argument("--cache", default=os.path.expanduser("~/.cache/flightframe-dem")); ap.add_argument("--peak", action="append", default=[])
    a = ap.parse_args()
    f16, f14 = ImageFont.truetype(a.font, 16), ImageFont.truetype(a.font, 14)
    Hz, Dz, h0 = terrain(a.lat, a.lon, a.head, a.fov, a.eye, a.ground, a.cache)
    def frame():
        im = bg.copy(); d = ImageDraw.Draw(im)
        taken = [(9, 9, 300, 36), (W - 280, 9, W - 9, 36)]                           # overlays
        def free(b): return all(b[2] < t[0] or b[0] > t[2] or b[3] < t[1] or b[1] > t[3] for t in taken)
        # planes
        P = planes(a.lat, a.lon, h0, a.head, a.fov, a.radius_nm)
        if a.demo:
            P = [dict(cs="EZY209F", type="A320", ft=9200, dist=21000, rel=-38, el=3.0, yaw=70), dict(cs="BAW1432", type="A319", ft=12500, dist=17000, rel=30, el=7.6, yaw=-105),
                 dict(cs="UAL945", type="B77W", ft=34000, dist=45000, rel=-6, el=10.5, yaw=165), dict(cs="DLH3V", type="A343", ft=36000, dist=26000, rel=44, el=13.0, yaw=60),
                 dict(cs="EXS28A", type="B738", ft=2400, dist=7000, rel=12, el=-0.8, yaw=-40), dict(cs="N73SG", type="BE36", ft=1700, dist=4500, rel=-20, el=-1.4, yaw=25),
                 dict(cs="GSGTS", type="GLF6", ft=41000, dist=60000, rel=18, el=9.0, yaw=-125)]
        # pass 1: draw every plane (far first so near ones sit on top) and remember where they are
        for p in sorted(P, key=lambda p: -p["dist"]):
            x = W / 2 + p["rel"] / a.fov * (W - 1); y = vy(p["el"])
            yaw = round(abs(p["yaw"]) / 15.0) * 15 * (1 if p["yaw"] >= 0 else -1)                    # the frame only has 15-degree steps
            pit = min((0, 15, 30), key=lambda v: abs(v - max(p["el"], 0)))
            L = max(36, min(120, 900000.0 / p["dist"]))
            sp = plane3d.sprite(CLS.get(p["type"], "narrow"), yaw, pit, L)
            ink = sp.point(lambda v: 255 if v > 110 else 0).convert("1")
            halo = ink.convert("L").filter(ImageFilter.MaxFilter(7)).point(lambda v: 255 if v > 0 else 0).convert("1")
            px, py = int(x - sp.width / 2), int(y - sp.height / 2)
            hl = Image.new("L", im.size, 0); hl.paste(halo, (px, py)); im.paste(1, (0, 0), hl.convert("1"))
            il = Image.new("L", im.size, 0); il.paste(ink, (px, py)); im.paste(0, (0, 0), il.convert("1"))
            p.update(x=x, y=y, px=px, py=py, sw=sp.width, sh=sp.height)
            taken.append((px, py, px + sp.width, py + sp.height))
        d = ImageDraw.Draw(im)
        # peaks: label sits above the summit; slide it up if a plane is in the way
        for pk in a.peak:
            nm, la, lo, hm = pk.split(","); la, lo, hm = float(la), float(lo), float(hm)
            dy = (la - a.lat) * 111320.0; dx = (lo - a.lon) * 111320.0 * math.cos(math.radians(a.lat)); dist = math.hypot(dx, dy)
            rel = (math.degrees(math.atan2(dx, dy)) - a.head + 180) % 360 - 180
            if abs(rel) > a.fov / 2: continue
            el = math.degrees(math.atan2(hm - h0 - dist ** 2 / (2 * R_EARTH) * 0.87, dist)); x = W / 2 + rel / a.fov * (W - 1); y = vy(el)
            lab = f"{nm} {int(hm)} m"; tw = d.textlength(lab, font=f14)
            for up in (22, 44, 66, 88):
                box = (x - tw / 2 - 3, y - up - 18, x + tw / 2 + 3, y - up)
                if all(box[2] < t[0] or box[0] > t[2] or box[3] < t[1] or box[1] > t[3] for t in taken): break
            d.line([(x, y - 4), (x, box[3])], fill=0); d.rectangle(box, fill=1, outline=0); d.text((x - tw / 2, box[1] + 2), lab, font=f14, fill=0); taken.append(box)
        # pass 2: labels, nearest plane first so it gets the best spot; a label that finds no free spot is left out
        for p in sorted(P, key=lambda p: p["dist"]):
            x, y, px, py, sw, sh = p["x"], p["y"], p["px"], p["py"], p["sw"], p["sh"]
            t1 = ((p["cs"] or "?") + (" " + p["type"] if p["type"] else "")); t2 = f"{p['ft']:,} ft \u00b7 {p['dist'] / 1609.34:.0f} mi"; placed = None
            own = (px, py, px + sw, py + sh)
            for lines in (2, 1):
                tw = max(d.textlength(t1, font=f14), d.textlength(t2, font=f14)) if lines == 2 else d.textlength(t1, font=f14); hh = 29 if lines == 2 else 15
                spots = [(x - tw / 2, py + sh + 2), (x - tw / 2, py - hh - 3), (px + sw + 6, y - hh / 2), (px - tw - 6, y - hh / 2),
                         (x - tw / 2, py + sh + 2 + hh), (x - tw / 2, py - 2 * hh - 5), (px + sw + 6, y + 10), (px - tw - 6, y + 10), (px + sw + 6, y - hh - 8), (px - tw - 6, y - hh - 8)]
                for lx, ly in spots:
                    lx = min(max(lx, 4), W - tw - 4); box = (lx - 2, ly - 1, lx + tw + 2, ly + hh)
                    if 2 < box[1] and box[3] < YB and all(box[2] < t[0] or box[0] > t[2] or box[3] < t[1] or box[1] > t[3] for t in taken if t != own): placed = (lx, ly, box, lines); break
                if placed: break
            if not placed: continue
            lx, ly, box, lines = placed
            gap = max(box[0] - own[2], own[0] - box[2], box[1] - own[3], own[1] - box[3])
            if gap > 5:
                cx, cy = (box[0] + box[2]) / 2, (box[1] + box[3]) / 2; tx, ty = x, y
                for t in range(0, 100, 4):
                    u = t / 100.0; d.point((cx + (tx - cx) * u, cy + (ty - cy) * u), fill=0)
            d.rectangle(box, fill=1); d.text((lx, ly), t1, font=f14, fill=0)
            if lines == 2: d.text((lx, ly + 14), t2, font=f14, fill=0)
            taken.append(box)
        im.paste(bg.crop((0, YB, W, H)), (0, YB))                                    # the compass strip stays on top
        d.rectangle([9, 9, 299, 35], fill=1, outline=0, width=2); d.text((18, 12), f"WINDOW · facing {int(a.head)}° · {int(round(h0))} m up", font=f16, fill=0)
        ban = f"Updated {time.strftime('%H:%M')} · {len(P)} planes in view"; tw = d.textlength(ban, font=f14)
        d.rectangle([W - tw - 30, 9, W - 9, 35], fill=1, outline=0, width=2); d.text((W - tw - 21, 14), ban, font=f14, fill=0)
        im.save(a.out + ".tmp.png"); os.replace(a.out + ".tmp.png", a.out); print(f"[ok] {a.out} planes in view: {len(P)}", flush=True)
    base = shade(Hz, Dz)
    bg = Image.new("1", (W, H), 1); bg.paste(base, (0, 0)); d = ImageDraw.Draw(bg)
    # guides
    for deg, lab in ((30, "30°"), (20, "20°"), (10, "10°"), (5, "5°"), (0, "eye level"), (-5, "-5°")):
        y = vy(deg)
        for x in range(0, W, 6 if deg == 0 else 10): d.point((x, y), fill=0)
        d.text((8, y - 15), lab, font=f14, fill=0)
    # compass strip
    d.rectangle([0, YB, W, H], fill=1); d.line([(0, YB), (W, YB)], fill=0, width=2)
    names = {0: "N", 45: "NE", 90: "E", 135: "SE", 180: "S", 225: "SW", 270: "W", 315: "NW"}
    for deg in range(0, 360, 5):
        rel = (deg - a.head + 180) % 360 - 180
        if abs(rel) > a.fov / 2: continue
        x = W / 2 + rel / a.fov * (W - 1)
        d.line([(x, YB), (x, YB + (14 if deg % 45 == 0 else 8 if deg % 15 == 0 else 4))], fill=0, width=2 if deg % 45 == 0 else 1)
        if deg % 45 == 0: lab = names[deg]; d.text((x - d.textlength(lab, font=f16) / 2, YB + 17), lab, font=f16, fill=0)
        elif deg % 15 == 0: lab = str(deg); d.text((x - d.textlength(lab, font=f14) / 2, YB + 24), lab, font=f14, fill=0)
    if a.export:
        export_pack(a, bg, h0); return
    while True:
        try: frame()
        except Exception as e: print("[warn]", e, flush=True)
        if not a.loop: break
        time.sleep(a.loop)

if __name__ == "__main__":
    main()
