#include "model/ForecastCache.h"

#include <esp_attr.h>
#include <string.h>

namespace {

// Fixed-size, String-free mirror of Forecast suitable for RTC memory (heap
// pointers such as String's buffer do not survive deep sleep).
struct CachedDay {
  int high;
  int low;
  int precipChance;
  int code;
  char dayOfWeek[8];
};

struct Blob {
  uint32_t magic;
  int curTemp;
  int curCode;
  char skyText[24];
  time_t utcEpoch;
  long utcOffsetSeconds;
  CachedDay days[FORECAST_DAYS];
  int temp[HOURLY_POINTS];
  int precip[HOURLY_POINTS];
  int hourlyCount;
  int hourStart;
  time_t hourStartUtc;
  int sunriseHour;
  int sunsetHour;
  char city[40];
};

// Bumped whenever Blob's layout changes, so a stale cache from an older build
// is rejected rather than misread.
constexpr uint32_t kMagic = 0xA7100004;

RTC_DATA_ATTR Blob g_blob;

void copyStr(char* dst, size_t cap, const char* src) {
  if (cap == 0) return;
  size_t i = 0;
  for (; src && src[i] && i + 1 < cap; i++) dst[i] = src[i];
  dst[i] = '\0';
}

}  // namespace

bool cache::valid() { return g_blob.magic == kMagic; }

void cache::invalidate() { g_blob.magic = 0; }

void cache::store(const Forecast& forecast, const String& city) {
  g_blob.curTemp = forecast.current.temperature;
  g_blob.curCode = forecast.current.code;
  copyStr(g_blob.skyText, sizeof(g_blob.skyText), forecast.current.skyText.c_str());
  g_blob.utcEpoch = forecast.current.utcEpoch;
  g_blob.utcOffsetSeconds = forecast.current.utcOffsetSeconds;

  for (int i = 0; i < FORECAST_DAYS; i++) {
    g_blob.days[i].high = forecast.days[i].high;
    g_blob.days[i].low = forecast.days[i].low;
    g_blob.days[i].precipChance = forecast.days[i].precipChance;
    g_blob.days[i].code = forecast.days[i].code;
    copyStr(g_blob.days[i].dayOfWeek, sizeof(g_blob.days[i].dayOfWeek),
            forecast.days[i].dayOfWeek.c_str());
  }

  int n = forecast.hourlyCount;
  if (n < 0) n = 0;
  if (n > HOURLY_POINTS) n = HOURLY_POINTS;
  for (int i = 0; i < n; i++) {
    g_blob.temp[i] = forecast.temp[i];
    g_blob.precip[i] = forecast.precip[i];
  }
  g_blob.hourlyCount = n;
  g_blob.hourStart = forecast.hourStart;
  g_blob.hourStartUtc = forecast.hourStartUtc;
  g_blob.sunriseHour = forecast.sunriseHour;
  g_blob.sunsetHour = forecast.sunsetHour;

  copyStr(g_blob.city, sizeof(g_blob.city), city.c_str());
  g_blob.magic = kMagic;
}

bool cache::load(Forecast& forecast, String& city) {
  if (!valid()) return false;

  forecast.current.temperature = g_blob.curTemp;
  forecast.current.code = g_blob.curCode;
  forecast.current.skyText = g_blob.skyText;
  forecast.current.utcEpoch = g_blob.utcEpoch;
  forecast.current.utcOffsetSeconds = g_blob.utcOffsetSeconds;

  for (int i = 0; i < FORECAST_DAYS; i++) {
    forecast.days[i].high = g_blob.days[i].high;
    forecast.days[i].low = g_blob.days[i].low;
    forecast.days[i].precipChance = g_blob.days[i].precipChance;
    forecast.days[i].code = g_blob.days[i].code;
    forecast.days[i].dayOfWeek = g_blob.days[i].dayOfWeek;
  }

  int n = g_blob.hourlyCount;
  if (n < 0) n = 0;
  if (n > HOURLY_POINTS) n = HOURLY_POINTS;
  for (int i = 0; i < n; i++) {
    forecast.temp[i] = g_blob.temp[i];
    forecast.precip[i] = g_blob.precip[i];
  }
  forecast.hourlyCount = n;
  forecast.hourStart = g_blob.hourStart;
  forecast.hourStartUtc = g_blob.hourStartUtc;
  forecast.sunriseHour = g_blob.sunriseHour;
  forecast.sunsetHour = g_blob.sunsetHour;

  city = g_blob.city;
  return true;
}
