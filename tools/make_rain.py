#!/usr/bin/env python3
"""Builds rain.bin: a 1-bit rain / no-rain mask of the UK and Ireland from the newest RainViewer radar frame.
Format "FFR2": tx0, ty0 (z7 tile origin, bytes), w, h (le16, half-resolution pixels), unix time of the radar frame (le32),
then h rows of (w+7)//8 bytes, MSB first. The frame downloads this file and reads only the part around its own location,
so the file contains no personal data. Runs in GitHub Actions every 10 minutes. Radar data: RainViewer (https://www.rainviewer.com)."""
import io, json, math, struct, sys, urllib.request
from PIL import Image
Z = 7
LAT = (49.0, 61.0); LON = (-11.5, 8.5)         # UK + Ireland + Shetland + Netherlands, Belgium, north-west France
UA = {"User-Agent": "FlightFrame-rain/1.0 (+https://github.com/chiupott/flightframe)"}
def get(url):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=30) as r: return r.read()
    except urllib.error.HTTPError as e:
        if e.code == 404: return None
        raise
def tile(lat, lon):
    n = 2 ** Z; x = (lon + 180) / 360 * n; s = math.sin(math.radians(lat))
    return int(x), int((0.5 - math.log((1 + s) / (1 - s)) / (4 * math.pi)) * n)
def main(out):
    meta = json.loads(get("https://api.rainviewer.com/public/weather-maps.json"))
    f = meta["radar"]["past"][-1]; host = meta["host"]
    x0, y1 = tile(LAT[0], LON[0]); x1, y0 = tile(LAT[1], LON[1])      # y grows southwards
    tw, th = x1 - x0 + 1, y1 - y0 + 1; W, H = tw * 128, th * 128
    mask = bytearray((W + 7) // 8 * H); stride = (W + 7) // 8; wet = 0
    for ty in range(y0, y1 + 1):
        for tx in range(x0, x1 + 1):
            b = get(f"{host}{f['path']}/256/{Z}/{tx}/{ty}/0/0_0.png")
            if not b: continue
            a = Image.open(io.BytesIO(b)).convert("RGBA").getchannel("A").point(lambda v: 1 if v > 0 else 0)
            px = a.load()
            for j in range(128):
                for i in range(128):
                    if px[2*i, 2*j] + px[2*i+1, 2*j] + px[2*i, 2*j+1] + px[2*i+1, 2*j+1] >= 2:      # 2x2 block mostly wet
                        u = (tx - x0) * 128 + i; v = (ty - y0) * 128 + j
                        mask[v * stride + (u >> 3)] |= 0x80 >> (u & 7); wet += 1
    hdr = b"FFR2" + bytes([x0, y0]) + struct.pack("<HHI", W, H, f["time"])
    open(out, "wb").write(hdr + bytes(mask))
    print(f"rain.bin: tiles {tw}x{th} from ({x0},{y0}), {W}x{H} px, wet {wet}, frame time {f['time']}, {14 + len(mask)} bytes")
if __name__ == "__main__": main(sys.argv[1] if len(sys.argv) > 1 else "rain.bin")
