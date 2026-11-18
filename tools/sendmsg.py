#!/usr/bin/env python3
"""Send a private note to the frame (it appears as a full-screen card until BOOT is pressed).
Usage: sendmsg.py "text" [--pic name] [--dry]      --pic = a picture on the frame's SD card at /msg/<name>.mg (make it with img2lg.py)
The text is scrambled with the key built into the firmware and signed with your private key (~/Documents/JoshProject/_keys/msg_signing_key.pem).
The frame shows only messages signed with that key. Posts to the ntfy.sh mailbox named in msg_keys.h; the frame checks it about once a minute."""
import sys, os, json, time, base64, urllib.request
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
K = os.path.expanduser("~/Documents/JoshProject/_keys")
def build(text, pic=""):
    cfg = json.load(open(K + "/msg_config.json")); key = bytes.fromhex(cfg["aes"])
    pic = pic.encode(); t = text.encode("utf-8")
    pt = int(time.time()).to_bytes(4, "big") + bytes([len(pic)]) + pic + t
    if len(pt) > 250: raise SystemExit("message too long (%d bytes, max about 240)" % len(pt))
    nonce = os.urandom(12); out = AESGCM(key).encrypt(nonce, pt, None); ct, tag = out[:-16], out[-16:]
    raw = b"\x01" + nonce + bytes([len(ct)]) + ct + tag
    priv = serialization.load_pem_private_key(open(K + "/msg_signing_key.pem", "rb").read(), None)
    sig = priv.sign(raw, ec.ECDSA(hashes.SHA256()))
    return cfg["topic"], base64.b64encode(raw + sig).decode()
if __name__ == "__main__":
    a = sys.argv[1:]; dry = "--dry" in a; pic = ""
    if "--pic" in a: i = a.index("--pic"); pic = a[i + 1]; del a[i:i + 2]
    a = [x for x in a if x != "--dry"]
    if not a: raise SystemExit(__doc__)
    topic, body = build(" ".join(a), pic)
    if dry: print("dry run: payload %d chars" % len(body)); sys.exit()
    r = urllib.request.urlopen(urllib.request.Request("https://ntfy.sh/" + topic, data=body.encode(), method="POST"), timeout=20)
    print("sent" if r.status == 200 else "failed %d" % r.status)
