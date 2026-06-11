#include "display/Display.h"

#include <TimeLib.h>
#include <string.h>

#include "display/weather_layout.h"

void Display::begin() {
#ifdef DEBUG_BUILD
  gfx_.init(115200, false);
#else
  gfx_.init(0, false);
#endif
}

void Display::setRotation(uint8_t rotation) { gfx_.setRotation(rotation); }

void Display::hibernate() { gfx_.hibernate(); }

void Display::clearFullScreen() {
  gfx_.setFullWindow();
  gfx_.fillScreen(GxEPD_WHITE);
  gfx_.display(false);
}

namespace {
// Copies src into dst (capacity cap) and upper-cases the first letter, so the
// lowercase WMO text ("drizzle") shows as "Drizzle".
void titleCase(char* dst, size_t cap, const String& src) {
  size_t n = src.length() < cap - 1 ? src.length() : cap - 1;
  for (size_t i = 0; i < n; i++) dst[i] = src[i];
  dst[n] = '\0';
  if (dst[0] >= 'a' && dst[0] <= 'z') dst[0] -= 32;
}
}  // namespace

void Display::drawWeather(const Forecast& forecast, const String& city) {
  // Translate the model into the neutral view the shared layout consumes. The
  // backing buffers live for the whole function, which spans the paged draw.
  char condBuf[24];
  titleCase(condBuf, sizeof(condBuf), forecast.current.skyText);

  // Local date, e.g. "Wed Nov 3", derived from the observation epoch + offset.
  char dateBuf[16];
  time_t localNow = forecast.current.utcEpoch + forecast.current.utcOffsetSeconds;
  snprintf(dateBuf, sizeof(dateBuf), "%s %s %d", dayShortStr(weekday(localNow)),
           monthShortStr(month(localNow)), day(localNow));

  char dayLabels[FORECAST_DAYS][8];
  atmo::DayView days[FORECAST_DAYS];
  for (int i = 0; i < FORECAST_DAYS; i++) {
    strncpy(dayLabels[i], forecast.days[i].dayOfWeek.c_str(), sizeof(dayLabels[i]) - 1);
    dayLabels[i][sizeof(dayLabels[i]) - 1] = '\0';
    days[i].label = dayLabels[i];
    days[i].code = forecast.days[i].code;
    days[i].hi = forecast.days[i].high;
    days[i].lo = forecast.days[i].low;
    days[i].precip = forecast.days[i].precipChance;
  }

  atmo::WeatherView view;
  view.city = city.c_str();
  view.date = dateBuf;
  view.condition = condBuf;
  view.temperature = forecast.current.temperature;
  view.code = forecast.current.code;
  view.days = days;
  view.dayCount = FORECAST_DAYS;
  view.hourly = forecast.hourlyCount > 0 ? forecast.hourly : nullptr;
  view.hourlyCount = forecast.hourlyCount;

  atmo::FontSet fonts{&FreeSans9pt7b,      &FreeSansBold9pt7b,  &FreeSans12pt7b,
                      &FreeSansBold12pt7b, &FreeSans18pt7b,     &FreeSansBold18pt7b,
                      &FreeSans24pt7b,     &FreeSansBold24pt7b};

  // Full-window paged draw: a clean full refresh each day (no partial-update
  // ghosting buildup), and the layout redraws all content per page.
  gfx_.setFullWindow();
  gfx_.firstPage();
  do {
    atmo::renderWeather(gfx_, view, fonts);
  } while (gfx_.nextPage());
}

void Display::drawConnectionInstructions() {
  gfx_.setPartialWindow(0, 0, gfx_.width(), gfx_.height());
  gfx_.fillScreen(GxEPD_WHITE);
  gfx_.setFont(font9_);
  gfx_.setTextColor(GxEPD_BLACK);
  gfx_.setCursor(0, 19);
  gfx_.println("Connect to ATMO WiFi network.");
  gfx_.println("Then, browse to at.mo");
  gfx_.println();
  gfx_.println("Configure and enjoy!");
  gfx_.nextPage();
}

void Display::drawFailedToConnectToSite() {
  gfx_.setPartialWindow(0, 0, gfx_.width(), gfx_.height());
  gfx_.fillScreen(GxEPD_WHITE);
  gfx_.setFont(font9_);
  gfx_.setTextColor(GxEPD_BLACK);
  gfx_.setCursor(0, 40 + 9);
  gfx_.println("Couldn't reach the weather");
  gfx_.println("service. Check your internet.");
  gfx_.println();
  gfx_.println("Retrying soon...");
  gfx_.nextPage();
}

void Display::drawFailedToConnectToWiFi(const String& ssid) {
  gfx_.setPartialWindow(0, 0, gfx_.width(), gfx_.height());
  gfx_.fillScreen(GxEPD_WHITE);
  gfx_.setFont(font9_);
  gfx_.setTextColor(GxEPD_BLACK);
  gfx_.setCursor(0, 20 + 9);
  gfx_.println("Couldn't join WiFi network:");
  gfx_.println(ssid.length() ? ssid : String("(none saved)"));
  gfx_.println();
  gfx_.println("Hold the setup button to");
  gfx_.println("reconfigure. Retrying soon...");
  gfx_.nextPage();
}

void Display::showSettingsSaved() {
  gfx_.setPartialWindow(0, 0, gfx_.width(), gfx_.height());
  gfx_.fillScreen(GxEPD_WHITE);
  gfx_.nextPage();
  gfx_.fillScreen(GxEPD_BLACK);
  gfx_.nextPage();
  gfx_.fillScreen(GxEPD_WHITE);
  gfx_.nextPage();
}

void Display::drawUpdating() {
  gfx_.setFont(font9_);
  gfx_.setTextColor(GxEPD_BLACK);
  int16_t bx, by;
  uint16_t bw, bh;
  gfx_.getTextBounds("updating", 0, 0, &bx, &by, &bw, &bh);
  int cx = gfx_.width() / 2;
  int cy = 20;
  int pad = 4;
  int wx = cx - bw / 2 - pad;
  int wy = cy + by - pad;
  int ww = bw + pad * 2;
  int wh = bh + pad * 2;
  gfx_.setPartialWindow(wx, wy, ww, wh);
  gfx_.fillScreen(GxEPD_WHITE);
  gfx_.setCursor(cx - bw / 2, cy);
  gfx_.print("updating");
  gfx_.nextPage();
  gfx_.fillRect(wx, wy, ww, wh, GxEPD_WHITE);
}
