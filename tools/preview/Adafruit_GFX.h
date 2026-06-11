// Minimal stub so the real Adafruit font headers (which #include <Adafruit_GFX.h>)
// compile in the host preview without the full library. Provides PROGMEM and the
// GFXfont/GFXglyph structs they reference.
#pragma once
#ifndef PROGMEM
#define PROGMEM
#endif
#include <stdint.h>
#include "gfxfont.h"
