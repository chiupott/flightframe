#pragma once
#include <stdint.h>
enum class Feed : uint8_t { Live, Stale, None };
void drawInit();
void drawMessage(const char* l1, const char* l2 = "");   // setup / error screens
void drawScreen(Feed feed, bool wifiOk);                  // radar + list from planes[] and trails[]
