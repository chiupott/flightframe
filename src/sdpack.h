#pragma once
// First-boot setup of a blank SD card: downloads the public map and plane pictures from the "sdpack" GitHub release.
// Not included (never in the public repo): airline logos (trademarks) and the window-view skyline (it reveals where the frame is).
// Returns the number of files fetched this call (0 = nothing missing or nothing possible right now).
int sdpackEnsure();
