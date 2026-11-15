#!/bin/bash
# Host simulation of the real firmware sources. Usage: ./run.sh <flights.json> [out.png]
set -e
H=/home/claude; FW=$H/flightframe-fw; W=$FW/sim/_work; mkdir -p $W
python3 $FW/sim/gen_snaps.py "$1" $W
G=$H/adafruit/adafruit-gfx-library/src; A=$H/arduino/ArduinoCore-API/api
g++ -std=c++17 -O1 -w -I$FW/sim -I$FW/include -I$FW/src -I$H/arduino/ArduinoCore-API -I$H/bblanchon/arduinojson/src -I$G \
  $FW/sim/sim.cpp $FW/src/{store,geo,adsb,draw,main,icons,view,map}.cpp $G/Adafruit_GFX.cpp $A/String.cpp $A/Print.cpp $A/Common.cpp -DFW_VERSION=\"sim\" -DARDUINO=10819 -o $W/sim
(cd $W && ./sim . 12 out.pbm)
python3 -c "from PIL import Image; Image.open('$W/out.pbm').save('${2:-$W/out.png}')"
