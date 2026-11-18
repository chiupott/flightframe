#!/usr/bin/env python3
"""Make a picture the frame can fetch by itself for a private message card.
Usage: mkpic.py <name> <picture.png|picture.jpg|picture.mg> [--upload]
Converts the picture to the frame's 4 greys (img2lg.py) if needed, scrambles it with the key built into the firmware, signs it with your private key
and writes ~/Documents/JoshProject/_msgpics/msgpic-<name>.bin. With --upload it is added to the public 'sdpack' release on GitHub (only scrambled bytes are visible there).
Then: sendmsg.py "text" --pic <name>. The frame downloads the picture the first time a message names it and keeps it on its SD card.
To change a picture later, make it under a new name."""
import sys, os, re, json, subprocess
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
K = os.path.expanduser("~/Documents/JoshProject/_keys"); OUT = os.path.expanduser("~/Documents/JoshProject/_msgpics")
def pack(name, mg):
    cfg = json.load(open(K + "/msg_config.json")); key = bytes.fromhex(cfg["aes"])
    w, h = mg[0] | (mg[1] << 8), mg[2] | (mg[3] << 8)
    if not (1 <= w <= 250 and 1 <= h <= 296) or len(mg) != 4 + h * ((w + 3) // 4): raise SystemExit("not a valid .mg picture (max 250x296)")
    nonce = os.urandom(12); ct_tag = AESGCM(key).encrypt(nonce, mg, name.encode()); ct, tag = ct_tag[:-16], ct_tag[-16:]
    raw = b"FFMX" + nonce + len(ct).to_bytes(2, "little") + ct + tag
    priv = serialization.load_pem_private_key(open(K + "/msg_signing_key.pem", "rb").read(), None)
    return raw + priv.sign(raw, ec.ECDSA(hashes.SHA256()))
if __name__ == "__main__":
    a = [x for x in sys.argv[1:] if x != "--upload"]; up = "--upload" in sys.argv
    if len(a) != 2 or not re.fullmatch(r"[A-Za-z0-9_-]{1,24}", a[0]): raise SystemExit(__doc__)
    name, src = a; os.makedirs(OUT, exist_ok=True)
    if not src.endswith(".mg"):
        mg_path = os.path.join(OUT, name + ".mg"); subprocess.check_call([sys.executable, os.path.join(os.path.dirname(__file__), "img2lg.py"), src, mg_path]); src = mg_path
    blob = pack(name, open(src, "rb").read()); dst = os.path.join(OUT, "msgpic-%s.bin" % name); open(dst, "wb").write(blob)
    print("wrote %s (%d bytes)" % (dst, len(blob)))
    if up:
        subprocess.check_call(["gh", "release", "upload", "sdpack", dst, "--repo", "chiupott/flightframe", "--clobber"]); print("uploaded")
