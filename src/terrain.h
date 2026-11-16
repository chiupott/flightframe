#pragma once
// Window view skyline, made on the frame itself: downloads public elevation tiles for the frame location, ray-marches the
// view and writes /window/terrain.bin (same format tools/window_view.py --export makes). Nothing leaves the device but tile requests.
bool terrainReady();       // a terrain.bin exists that matches the current location + facing (or was made by hand)
bool terrainGenerate();    // blocks for a few minutes, shows progress on screen; false if it could not finish
