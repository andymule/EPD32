#pragma once
#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <GxEPD2_BW.h>

#include "fonts/FreeSans6pt7b.h"
#include "fonts/FreeSansBold6pt7b.h"
#include "fonts/FreeSans7pt7b.h"
#include "fonts/FreeSansBold7pt7b.h"
#include "Config.h"
#include "model/WeatherData.h"

// Owns the e-paper panel and all rendering. Wraps GxEPD2 so the rest of the
// codebase never touches the display object or global drawing state directly.
// The weather frame is drawn by the resolution-independent layout in
// weather_layout.h, so the same code adapts to other panel sizes.
class Display {
 public:
  void begin();
  void setRotation(uint8_t rotation);
  // Selects the weather layout: 0 = Dashboard, 1 = Horizon.
  void setLayoutStyle(uint8_t style);
  void hibernate();

  // Full white refresh (used on a hard reset to clear ghosting).
  void clearFullScreen();

  void drawConnectionInstructions();
  // One frame of the "slide the current hero icon off the right" cue. `frame`
  // is 1-based; `weatherCode` is the icon currently on screen (from the cached
  // forecast). Returns true while more frames remain (icon not yet fully gone),
  // so callers can loop without hard-coding the step count.
  bool animateHeroOutFrame(int frame, int weatherCode);
  // One frame of the first-boot splash: "Atmo" + a cloud, then rain filling
  // downward (old pixels persist). frame 0 draws the title+cloud; later frames
  // add rain. Returns false once the rain has reached the bottom.
  bool drawSplashFrame(int frame);
  void drawFailedToConnectToSite();
  void drawFailedToConnectToWiFi(const String& ssid);
  void drawWeather(const Forecast& forecast, const String& city);

  // Brief white/black/white flash used to acknowledge saved setup settings.
  void showSettingsSaved();

 private:
  GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> gfx_{
      GxEPD2_290(pins::EPD_CS, pins::EPD_DC, pins::EPD_RST, pins::EPD_BUSY)};
  const GFXfont* font9_ = &FreeSans9pt7b;
  const GFXfont* font12_ = &FreeSans12pt7b;
  uint8_t layoutStyle_ = display_defaults::STYLE;
};
