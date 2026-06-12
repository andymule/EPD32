#include "net/OpenMeteoProvider.h"

#include <ArduinoJson.h>
#include <TimeLib.h>
#include <math.h>

#include "Config.h"
#include "Log.h"
#include "net/HttpClientHelper.h"
#include "weather/WeatherCodes.h"

String OpenMeteoProvider::buildUrl(float lat, float lon, bool metric) {
  String url = api::WEATHER_BASE_URL;
  url += "?latitude=" + String(lat, 4);
  url += "&longitude=" + String(lon, 4);
  url += "&current=temperature_2m,weather_code";
  url += "&hourly=temperature_2m,precipitation_probability,weather_code";
  url +=
      "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
      "precipitation_probability_max,sunrise,sunset";
  url += "&timezone=auto&timeformat=unixtime";
  url += "&forecast_days=" + String(FORECAST_DAYS);
  url += metric ? "&temperature_unit=celsius" : "&temperature_unit=fahrenheit";
  return url;
}

bool OpenMeteoProvider::fetch(HttpClientHelper& http, float lat, float lon,
                              bool metric, Forecast& out) {
  String url = buildUrl(lat, lon, metric);
  LOGLN(url);

  int code = http.get(url);
  if (code != 200) {
    LOGLN("Weather request failed: " + String(code));
    http.end();
    return false;
  }

  // Filter to just the rendered fields to bound heap use during the parse.
  JsonDocument filter;
  filter["utc_offset_seconds"] = true;
  filter["current"]["time"] = true;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["weather_code"] = true;
  filter["hourly"]["time"][0] = true;
  filter["hourly"]["temperature_2m"][0] = true;
  filter["hourly"]["precipitation_probability"][0] = true;
  filter["hourly"]["weather_code"][0] = true;
  filter["daily"]["time"][0] = true;
  filter["daily"]["weather_code"][0] = true;
  filter["daily"]["temperature_2m_max"][0] = true;
  filter["daily"]["temperature_2m_min"][0] = true;
  filter["daily"]["precipitation_probability_max"][0] = true;
  filter["daily"]["sunrise"][0] = true;
  filter["daily"]["sunset"][0] = true;

  // Read the whole body before parsing: Open-Meteo replies with chunked transfer
  // encoding (no Content-Length), which getString() de-chunks. Parsing the raw
  // stream instead would silently yield an empty document.
  String payload = http.body();
  const time_t respEpoch = http.serverEpoch();  // exact wall time (to the second)
  http.end();

  JsonDocument doc;
  DeserializationError error =
      deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error) {
    LOGLN("Weather JSON parse failed: " + String(error.c_str()));
    return false;
  }

  const long offset = doc["utc_offset_seconds"] | 0L;
  const time_t currentUtc = static_cast<time_t>(doc["current"]["time"] | 0UL);
  const int currentCode = doc["current"]["weather_code"] | 0;

  // Clock comes from the response Date header (to the second); current.time is
  // only 15-min coarse. Fall back to current.time if the header was missing.
  out.current.utcEpoch = respEpoch > 1579461559 ? respEpoch : currentUtc;
  out.current.utcOffsetSeconds = offset;
  out.current.code = currentCode;
  out.current.temperature = lround(doc["current"]["temperature_2m"] | 0.0f);
  out.current.skyText = weather_codes::shortText(currentCode);

  JsonArray days = doc["daily"]["time"].as<JsonArray>();
  JsonArray codes = doc["daily"]["weather_code"].as<JsonArray>();
  JsonArray highs = doc["daily"]["temperature_2m_max"].as<JsonArray>();
  JsonArray lows = doc["daily"]["temperature_2m_min"].as<JsonArray>();
  JsonArray precip = doc["daily"]["precipitation_probability_max"].as<JsonArray>();

  JsonArray hourTimes = doc["hourly"]["time"].as<JsonArray>();
  JsonArray hourTemps = doc["hourly"]["temperature_2m"].as<JsonArray>();
  JsonArray hourPrecip = doc["hourly"]["precipitation_probability"].as<JsonArray>();
  JsonArray hourCodes = doc["hourly"]["weather_code"].as<JsonArray>();
  const bool haveHourly = !hourTimes.isNull() && hourTimes.size() > 0;
  const time_t firstHour = haveHourly ? static_cast<time_t>(hourTimes[0] | 0UL) : 0;
  const int hourCount = haveHourly ? static_cast<int>(hourTimes.size()) : 0;

  for (int i = 0; i < FORECAST_DAYS; i++) {
    const time_t dayStartUtc = static_cast<time_t>(days[i] | 0UL);
    out.days[i].dayOfWeek = String(dayShortStr(weekday(dayStartUtc + offset)));
    // Icon from the 1pm-local condition (a representative daytime sky) rather
    // than Open-Meteo's daily code, which is the day's *most-significant*
    // weather and badly over-reports clouds/rain. Fall back to the daily code.
    int dayCode = codes[i] | 0;
    if (haveHourly) {
      int idx = static_cast<int>((dayStartUtc + 13 * 3600 - firstHour) / 3600);
      if (idx >= 0 && idx < hourCount) dayCode = hourCodes[idx] | 0;
    }
    out.days[i].code = dayCode;
    out.days[i].high = lround(highs[i] | 0.0f);
    out.days[i].low = lround(lows[i] | 0.0f);
    out.days[i].precipChance = precip[i] | 0;
  }

  // Today's sunrise/sunset as local clock hours (rounded to the nearest hour),
  // used to place the sun glyphs on the temperature curve. They vary by ~a
  // minute/day, so today's values mark the right hour anywhere in the window.
  const time_t srUtc = static_cast<time_t>(doc["daily"]["sunrise"][0] | 0UL);
  const time_t ssUtc = static_cast<time_t>(doc["daily"]["sunset"][0] | 0UL);
  out.sunriseHour =
      srUtc > 0 ? static_cast<int>(((srUtc + offset + 1800) / 3600) % 24) : -1;
  out.sunsetHour =
      ssUtc > 0 ? static_cast<int>(((ssUtc + offset + 1800) / 3600) % 24) : -1;

  // Hourly temp + precip: copy HOURLY_POINTS samples starting at the current
  // hour. The hourly timestamps share the same basis as current.time, so the
  // starting index is the whole-hours elapsed since the first sample.
  out.hourlyCount = 0;
  if (haveHourly) {
    int start = static_cast<int>((currentUtc - firstHour) / 3600);
    if (start < 0) start = 0;
    const time_t startUtc = static_cast<time_t>(hourTimes[start] | 0UL);
    out.hourStartUtc = startUtc;
    out.hourStart = hour(startUtc + offset);
    for (int i = start; i < hourCount && out.hourlyCount < HOURLY_POINTS; i++) {
      int p = hourPrecip[i] | 0;
      out.temp[out.hourlyCount] = lround(hourTemps[i] | 0.0f);
      out.precip[out.hourlyCount] = p < 0 ? 0 : (p > 100 ? 100 : p);
      out.hourlyCount++;
    }
  }
  return true;
}
