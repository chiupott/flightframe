# Flight Frame firmware (skeleton v0.1.0)

ESP32 + 7.5" 800x480 e-paper. Shows aircraft near a location on a radar plus a list of the nearest flights.

## Status
- Compiles and links with PlatformIO (ESP32, about 1.27 MB, 64% of an update slot). **Not yet run on hardware.**
- The renderer is the real design: NicoClean fonts, top-down aircraft sprites (rotated at draw time), coastline, rain dots with a dotted edge, trails, labels that avoid each other, overlays, status icons, list rows with airline logos. It runs in the host simulator and matches the design boards.
- Rain: a GitHub Action (`.github/workflows/rain.yml`, every 10 min) turns the newest RainViewer radar frame into a 1-bit UK mask (`tools/make_rain.py`) and uploads it to a rolling pre-release called `rain`. The frame downloads it to `/map/rain.bin` and reads only the part around its own location. Scheduled Actions pause after 60 days without a commit.

## Flow
1. Boot. 3 power-cycles in a row (each shorter than 5 s) wipes Wi-Fi and location and opens setup.
2. No Wi-Fi saved: open network `FlightFrame-Setup`, captive page, saves SSID, password, postcode, options.
3. Joins Wi-Fi, geocodes the postcode once (postcodes.io), keeps lat/lon in flash only. Location is never in this repo.
4. Every 30 s: adsb.fi v3 (planes) -> adsbdb routes for the rows shown (cached) -> draw. Trails come from the last 10 positions per aircraft, kept in RAM.
5. Every 6 h: latest GitHub Release `firmware.bin`; if newer, flash it. New image runs on probation; 3 boots without a good fetch + draw switch back to the old slot and skip that version.

## Before first flash (check on the kit)
- `config.h`: `GH_OWNER`, display pins, SD chip-select. Pins are the Waveshare ESP32 driver board defaults, unverified.
- Panel driver `GxEPD2_750_T7` (UC8179, GDEW075T7) is a guess. Confirm the exact controller of the Xicoolee kit.
- adsb.fi v3 path untested. The setup page (`portal_page.html`) scans Wi-Fi via `/api/scan` and saves via `/api/save`. The UK time zone is hardcoded in `clock.cpp`.

## Not in this repo
- Airline logos (`/logos/<ICAO>.bmp`, on the SD card): scraped, trademarked.

## Credits
Plane data: adsb.fi. Routes: adsbdb. Postcodes: postcodes.io. Rain: RainViewer. Icons: ADS-B Radar (https://adsb-radar.com).

## Host simulator (no hardware needed)
`sim/run.sh flights.json out.png` compiles the real sources against stand-ins for the ESP32 parts (fake clock, scripted network, in-memory flash, 800x480 framebuffer) and renders the screen after 12 simulated polls. Needs the ArduinoJson, Adafruit GFX and ArduinoCore-API sources (paths at the top of `run.sh`). `SIM_HEAD=90` and `SIM_FLIP=1` test rotation and mirroring.

## SD card layout (not in the repo)
- `/logos/<ICAO>.lg`: airline logos, 44x44 1-bit (`tools/bake_logos.py`). Missing logo: the frame draws a code badge.
- `/map/land.bin`: UK + Ireland coastline (`tools/bake_land.py`, Natural Earth, public domain).
- `/map/rain.bin`: written by the frame.

## Tools (run on a dev machine)
`gfxfont.py` (pixel font to GFX header), `bake_icons.py` (icon pack to sprites), `bake_status.py`, `bake_land.py`, `bake_logos.py`.
