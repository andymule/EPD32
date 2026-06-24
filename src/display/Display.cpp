#include "display/Display.h"

#include <TimeLib.h>
#include <string.h>
#include <time.h>

#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
#include <SPI.h>
#include <driver/gpio.h>
#endif

#include "display/weather_layout.h"
#include "fonts/FreeSansOblique7pt7b.h"

void Display::begin() {
#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
  // Power the panel rail and bind SPI before init() (on the T5, main() owns
  // SPI.begin since its panel pins are the bus defaults). EPD_PWR (GPIO6) gates
  // the EPD3V3 rail via an AO3401 P-FET, so it is ACTIVE-LOW. Hold it LOW across
  // deep sleep so the panel stays powered + hibernating (image/RAM retained)
  // rather than floating its gate. (HARDWARE.md "Power rules" has the full story.)
  pinMode(pins::EPD_PWR, OUTPUT);
  digitalWrite(pins::EPD_PWR, LOW);
  gpio_hold_en(static_cast<gpio_num_t>(pins::EPD_PWR));

  // Park the FT6336 touch controller in reset for good: it's on the always-on
  // 3V3 rail, and the firmware never uses touch, so holding RST low (and
  // latching that level through deep sleep) keeps it from draining the battery.
  pinMode(pins::EPD_TP_RST, OUTPUT);
  digitalWrite(pins::EPD_TP_RST, LOW);
  gpio_hold_en(static_cast<gpio_num_t>(pins::EPD_TP_RST));

  // Keep the pin holds above (and the VBAT latch) active while in deep sleep.
  gpio_deep_sleep_hold_en();

  SPI.begin(pins::SPI_SCK, pins::SPI_MISO, pins::SPI_MOSI, pins::EPD_CS);
#endif
#ifdef DEBUG_BUILD
  gfx_.init(115200, false);
#else
  gfx_.init(0, false);
#endif
}

void Display::setRotation(uint8_t rotation) { gfx_.setRotation(rotation); }

void Display::setLayoutStyle(uint8_t style) { layoutStyle_ = style; }

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

  // Local date + time. Prefer the live RTC clock (kept running across deep sleep)
  // so hourly cached redraws show the real time; fall back to the observation
  // epoch only before the clock has ever been set. The date is "Wed Jun 10"
  // (day-of-week, month, day); the time is 12h with no am/pm ("9:53").
  constexpr time_t kClockValidAfter = 1579461559;  // 2020-01-20
  time_t sysNow = time(nullptr);
  time_t baseUtc =
      (sysNow > kClockValidAfter) ? sysNow : forecast.current.utcEpoch;
  time_t localNow = baseUtc + forecast.current.utcOffsetSeconds;

  // dayShortStr() and monthShortStr() (TimeLib) both return the same shared
  // static buffer, so copy the weekday out before formatting the month — passing
  // both to one snprintf would otherwise print the month twice ("Jun Jun 11").
  char dow[4];
  char mon[4];
  strncpy(dow, dayShortStr(weekday(localNow)), sizeof(dow) - 1);
  dow[sizeof(dow) - 1] = '\0';
  strncpy(mon, monthShortStr(month(localNow)), sizeof(mon) - 1);
  mon[sizeof(mon) - 1] = '\0';

  char dateBuf[16];
  char timeBuf[8];
  snprintf(dateBuf, sizeof(dateBuf), "%s %s %d", dow, mon, day(localNow));
  int h12 = hour(localNow) % 12;
  if (h12 == 0) h12 = 12;
  snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", h12, minute(localNow));

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
  view.time = timeBuf;
  view.condition = condBuf;
  view.temperature = forecast.current.temperature;
  view.code = forecast.current.code;
  view.days = days;
  view.dayCount = FORECAST_DAYS;
  // Roll the hourly window forward so its left edge is the current hour (the
  // hour shown by the clock), not the fetch hour: skip the hours that have
  // elapsed since the fetch, then draw up to HOURLY_WINDOW hours from there.
  int elapsed = 0;
  if (sysNow > kClockValidAfter && forecast.hourStartUtc > 0) {
    long diff = static_cast<long>(sysNow - forecast.hourStartUtc);
    if (diff > 0) elapsed = diff / 3600;
  }
  if (elapsed < 0) elapsed = 0;
  if (elapsed > forecast.hourlyCount - 2)
    elapsed = forecast.hourlyCount >= 2 ? forecast.hourlyCount - 2 : 0;
  int winCount = forecast.hourlyCount - elapsed;
  if (winCount > HOURLY_WINDOW) winCount = HOURLY_WINDOW;
  if (winCount < 0) winCount = 0;

  view.temp = winCount > 0 ? forecast.temp + elapsed : nullptr;
  view.precip = winCount > 0 ? forecast.precip + elapsed : nullptr;
  view.hourlyCount = winCount;
  view.hourStart = winCount > 0 ? (forecast.hourStart + elapsed) % 24 : -1;
  view.sunriseHour = forecast.sunriseHour;
  view.sunsetHour = forecast.sunsetHour;

  atmo::FontSet fonts{&FreeSans6pt7b,      &FreeSansBold6pt7b,  &FreeSans7pt7b,
                      &FreeSansBold7pt7b,  &FreeSans9pt7b,      &FreeSansBold9pt7b,
                      &FreeSans12pt7b,     &FreeSansBold12pt7b, &FreeSans18pt7b,
                      &FreeSansBold18pt7b, &FreeSans24pt7b,     &FreeSansBold24pt7b};

  // Full-window paged draw: a clean full refresh each day (no partial-update
  // ghosting buildup), and the layout redraws all content per page.
  atmo::LayoutStyle style = layoutStyle_ == 0 ? atmo::LayoutStyle::Dashboard
                                              : atmo::LayoutStyle::Horizon;
  gfx_.setFullWindow();
  gfx_.firstPage();
  do {
    atmo::renderWeather(gfx_, view, fonts, style);
  } while (gfx_.nextPage());
}

namespace {
// Draws a vertically- and horizontally-centered text block (optional title plus
// body lines) using a full-window refresh, so it shows reliably on any panel
// (partial refreshes don't always latch on the 1.54" SSD1681) and adapts to the
// panel's geometry (296x128 T5 vs 200x200 S3). The panel class differs per
// board, so gfx is a template parameter. An empty line acts as a blank spacer.
template <typename GFX>
void drawCenteredScreen(GFX& gfx, const char* title, const char* const* lines,
                        int count) {
  const bool narrow = gfx.width() < 240;
  const GFXfont* titleFont = narrow ? &FreeSansBold12pt7b : &FreeSansBold18pt7b;
  const GFXfont* bodyFont = narrow ? &FreeSans7pt7b : &FreeSans9pt7b;

  const int W = gfx.width();
  const int H = gfx.height();
  const int titleLH = title ? titleFont->yAdvance : 0;
  const int bodyLH = bodyFont->yAdvance;
  const int gap = title ? bodyLH / 2 : 0;  // breathing room under the title
  const int blockH = titleLH + gap + count * bodyLH;

  gfx.setFullWindow();
  gfx.firstPage();
  do {
    gfx.fillScreen(GxEPD_WHITE);
    gfx.setTextColor(GxEPD_BLACK);
    int16_t bx, by;
    uint16_t bw, bh;
    int y = (H - blockH) / 2;
    if (y < 0) y = 0;
    if (title) {
      gfx.setFont(titleFont);
      gfx.getTextBounds(title, 0, 0, &bx, &by, &bw, &bh);
      gfx.setCursor((W - static_cast<int>(bw)) / 2 - bx, y - by);
      gfx.print(title);
      y += titleLH + gap;
    }
    gfx.setFont(bodyFont);
    for (int i = 0; i < count; i++) {
      gfx.getTextBounds(lines[i], 0, 0, &bx, &by, &bw, &bh);
      gfx.setCursor((W - static_cast<int>(bw)) / 2 - bx, y - by);
      gfx.print(lines[i]);
      y += bodyLH;
    }
  } while (gfx.nextPage());
}
}  // namespace

void Display::drawConnectionInstructions() {
  const char* lines[] = {"Join the \"Atmo\"", "WiFi network,",
                         "then open at.mo", "to set up."};
  drawCenteredScreen(gfx_, "Atmo", lines, 4);
}

void Display::drawFailedToConnectToSite() {
  const char* lines[] = {"Couldn't reach the", "weather service.", "",
                         "Retrying soon..."};
  drawCenteredScreen(gfx_, "Offline", lines, 4);
}

void Display::drawFailedToConnectToWiFi(const String& ssid) {
  const String net = ssid.length() ? ssid : String("(none saved)");
#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
  // The S3 has no dedicated setup button; setup is a long-press of BOOT.
  const char* lines[] = {"Couldn't join:", net.c_str(), "", "Hold BOOT to",
                         "open setup."};
#else
  const char* lines[] = {"Couldn't join:", net.c_str(), "", "Hold setup to",
                         "reconfigure."};
#endif
  drawCenteredScreen(gfx_, "No WiFi", lines, 5);
}

void Display::showSettingsSaved() {
  // Quick white -> black -> white flash to acknowledge a saved form. Full-window
  // refreshes so the flash is visible on panels that ignore partial updates.
  const uint16_t sequence[] = {GxEPD_WHITE, GxEPD_BLACK, GxEPD_WHITE};
  gfx_.setFullWindow();
  for (uint16_t color : sequence) {
    gfx_.firstPage();
    do {
      gfx_.fillScreen(color);
    } while (gfx_.nextPage());
  }
}

namespace {
constexpr int kHeroSlideSteps = 14;  // frames for the icon to leave the screen
constexpr int kRainFrames = 26;      // frames for splash rain to reach bottom
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int mini(int a, int b) { return a < b ? a : b; }
inline int maxi(int a, int b) { return a > b ? a : b; }
}  // namespace

bool Display::animateHeroOutFrame(int frame, int weatherCode) {
  const int W = gfx_.width();
  const int Ht = gfx_.height();

  // Where the hero icon currently sits, matching the active layout so the slide
  // starts exactly on top of the on-screen icon (no jump).
  int cx, cy, box;
  if (layoutStyle_ != 0) {  // Horizon
    int topH = clampi(Ht * 8 / 100, 13, 34);
    int dayH = clampi(Ht * 24 / 100, 46, 150);
    int dayTop = Ht - dayH;
    int curveTop = maxi(topH + Ht * 6 / 100, dayTop - maxi(40, Ht * 22 / 100));
    int heroH = curveTop - topH;
    if (heroH >= W * 34 / 100) {  // stacked icon-over-temp
      box = mini(W * 62 / 100, heroH * 56 / 100);
      cx = W / 2;
      cy = topH + heroH * 34 / 100;
    } else {  // side-by-side
      box = mini(heroH * 84 / 100, W * 22 / 100);
      cx = W * 28 / 100;
      cy = topH + heroH / 2;
    }
  } else {  // Dashboard (approximate top-center hero)
    box = mini(W * 56 / 100, Ht * 30 / 100);
    cx = W / 2;
    cy = Ht * 22 / 100;
  }

  const int endCx = W + box;  // fully off the right edge
  const int top = clampi(cy - box / 2 - 2, 0, Ht - 1);
  const int bot = clampi(cy + box / 2 + 2, 0, Ht);
  int curCx = cx + (endCx - cx) * frame / kHeroSlideSteps;
  int prevCx = cx + (endCx - cx) * (frame - 1) / kHeroSlideSteps;

  // Refresh only the strip the icon swept (old + new footprint) so it glides
  // rather than flashing a big block; clears its trail to white as it leaves.
  int loCx = clampi(mini(prevCx, curCx) - box / 2 - 2, 0, W);
  int hiCx = clampi(maxi(prevCx, curCx) + box / 2 + 2, 0, W);
  if (hiCx <= loCx) hiCx = loCx + 1;
  gfx_.setPartialWindow(loCx, top, hiCx - loCx, bot - top);
  gfx_.firstPage();
  do {
    gfx_.fillScreen(GxEPD_WHITE);
    atmo::drawIcon(gfx_, curCx, cy, box, weatherCode, GxEPD_BLACK, GxEPD_WHITE);
  } while (gfx_.nextPage());
  return frame < kHeroSlideSteps;  // false once the icon is fully off-screen
}

bool Display::drawSplashFrame(int frame) {
  const int W = gfx_.width();
  const int Ht = gfx_.height();
  const int minDim = W < Ht ? W : Ht;

  // Scale down on short panels (e.g. a landscape orientation) so the title,
  // tagline and cloud stack without colliding.
  const bool tall = Ht >= 200;
  const GFXfont* titleFont = tall ? &FreeSansBold18pt7b : &FreeSansBold12pt7b;
  const int titleCy = Ht * 25 / 100;
  const int tagCy = Ht * 37 / 100;  // a touch more gap below the title
  const int cloudCy = Ht * 55 / 100;
  const int cloudS = minDim * (tall ? 46 : 38) / 100;
  const int cloudBottom = cloudCy + cloudS * 18 / 100 + 2;
  const int rainLeft = clampi(W / 2 - cloudS * 42 / 100, 0, W);
  const int rainRight = clampi(W / 2 + cloudS * 42 / 100, 0, W);
  const int rainTop = cloudBottom;
  const int rainBot = Ht;

  if (frame == 0) {  // "Atmo" + tagline + cloud (on the already-white screen)
    int top = clampi(titleCy - 30, 0, Ht);
    gfx_.setPartialWindow(0, top, W, clampi(cloudBottom + 2, 0, Ht) - top);
    gfx_.firstPage();
    do {
      gfx_.fillScreen(GxEPD_WHITE);
      gfx_.setTextColor(GxEPD_BLACK);
      int16_t bx, by;
      uint16_t bw, bh;
      gfx_.setFont(titleFont);  // title (smaller on short panels)
      gfx_.getTextBounds("Atmo", 0, 0, &bx, &by, &bw, &bh);
      gfx_.setCursor(W / 2 - bw / 2, titleCy + bh / 2);
      gfx_.print("Atmo");
      gfx_.setFont(&FreeSansOblique7pt7b);  // little italic tagline
      gfx_.getTextBounds("Weather or not", 0, 0, &bx, &by, &bw, &bh);
      gfx_.setCursor(W / 2 - bw / 2, tagCy + bh / 2);
      gfx_.print("Weather or not");
      atmo::iconCloud(gfx_, W / 2, cloudCy, cloudS, GxEPD_BLACK, GxEPD_WHITE);
    } while (gfx_.nextPage());
    return true;
  }

  // Rain fills downward, one strip per frame; earlier strips persist (we only
  // refresh the new strip), so the rain accumulates into falling streaks. Start
  // one column in so there's no streak hugging the left edge.
  int step = maxi(3, (rainBot - rainTop) / kRainFrames);
  int prevY = rainTop + (frame - 1) * step;
  int curY = rainTop + frame * step;
  if (prevY >= rainBot) return false;
  if (curY > rainBot) curY = rainBot;

  gfx_.setPartialWindow(rainLeft, prevY, rainRight - rainLeft, curY - prevY);
  gfx_.firstPage();
  do {
    gfx_.fillScreen(GxEPD_WHITE);
    for (int y = prevY; y < curY; y++)
      for (int x = rainLeft + 6; x < rainRight; x += 6)
        if (((y + x) & 3) < 2) gfx_.drawPixel(x, y, GxEPD_BLACK);  // diagonal dashes
  } while (gfx_.nextPage());
  return curY < rainBot;
}
