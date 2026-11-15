#pragma once
// Updates come from the public repo's latest GitHub Release (asset firmware.bin).
// Rollback is our own: after an update we boot the new slot "on probation"; if it fails to reach
// otaMarkGood() three boots in a row, we switch back to the previous slot and skip that version.
void otaBootCheck();      // call first thing in setup()
void otaMarkGood();       // call after the first successful fetch + draw
bool otaCheckAndUpdate(); // true = flashed (device reboots)
