#pragma once
// Radar view: 480x480 square at the left, centre (240,240), outer ring r=235 = cfg.radiusMi miles.
// cfg.heading = compass direction pointing up; cfg.flip mirrors east/west.
static const int VCX = 240, VCY = 240, VR = 235, VW = 480;
void viewProj(float lat, float lon, float& x, float& y);
void viewUnproj(float x, float y, float& lat, float& lon);
float viewAngle(float compassDeg);        // compass bearing -> screen rotation (degrees clockwise from up)
