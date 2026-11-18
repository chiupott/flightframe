#!/bin/bash
# Publish a firmware update that every frame picks up by itself (checks GitHub every 6 h).
# Usage: tools/release.sh 0.2.1 "what changed"      (version must be higher than the one running on the frames)
set -e
V="$1"; NOTES="${2:-Update}"; [ -z "$V" ] && { echo "usage: $0 <version like 0.2.1> [notes]"; exit 1; }
cd "$(dirname "$0")/.."
python3 - "$V" <<'PY'
import re, sys
s = open("platformio.ini").read(); s = re.sub(r'FW_VERSION=\\"[0-9.]+\\"', lambda m: 'FW_VERSION=\\"%s\\"' % sys.argv[1], s); open("platformio.ini", "w").write(s)
PY
~/pio312/bin/pio run -e esp32c6grey
cp .pio/build/esp32c6grey/firmware.bin /tmp/firmware.bin
KEY=~/Documents/JoshProject/_keys/ota_signing_key.pem
[ -f "$KEY" ] || { echo "signing key missing: $KEY"; exit 1; }
openssl dgst -sha256 -sign "$KEY" -out /tmp/firmware.sig.der /tmp/firmware.bin && openssl base64 -A -in /tmp/firmware.sig.der > /tmp/firmware.sig
gh release create "v$V" /tmp/firmware.bin /tmp/firmware.sig --repo chiupott/flightframe --title "Firmware $V" --notes "$NOTES" --latest
echo "Published v$V. Frames update within 6 hours (or on the next restart)."
