#pragma once
#include <Arduino.h>
// A private note that pops up on the frame as a full-screen card (until BOOT is pressed). Messages are scrambled with a built-in key and
// signed by the maker; a frame shows one only if the signature checks out (see msg_keys.h). An optional picture is /msg/<name>.lg on the SD card.
struct Msg { String text; String pic; uint32_t sent = 0; };
bool msgPoll(Msg& m);              // checks the mailbox; true if a new, valid message arrived
void msgDraw(const Msg& m);        // the card, on whichever driver the current build uses
