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
  url += "&hourly=temperature_2m";
  url +=
      "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
      "precipitation_probability_max";
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
  filter["daily"]["time"][0] = true;
  filter["daily"]["weather_code"][0] = true;
  filter["daily"]["temperature_2m_max"][0] = true;
  filter["daily"]["temperature_2m_min"][0] = true;
  filter["daily"]["precipitation_probability_max"][0] = true;

  // Read the whole body before parsing: Open-Meteo replies with chunked transfer
  // encoding (no Content-Length), which getString() de-chunks. Parsing the raw
  // stream instead would silently yield an empty document.
  String payload = http.body();
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

  out.current.utcEpoch = currentUtc;
  out.current.utcOffsetSeconds = offset;
  out.current.code = currentCode;
  out.current.temperature = lround(doc["current"]["temperature_2m"] | 0.0f);
  out.current.skyText = weather_codes::shortText(currentCode);

  JsonArray days = doc["daily"]["time"].as<JsonArray>();
  JsonArray codes = doc["daily"]["weather_code"].as<JsonArray>();
  JsonArray highs = doc["daily"]["temperature_2m_max"].as<JsonArray>();
  JsonArray lows = doc["daily"]["temperature_2m_min"].as<JsonArray>();
  JsonArray precip = doc["daily"]["precipitation_probability_max"].as<JsonArray>();

  for (int i = 0; i < FORECAST_DAYS; i++) {
    time_t dayLocal = static_cast<time_t>(days[i] | 0UL) + offset;
    out.days[i].dayOfWeek = String(dayShortStr(weekday(dayLocal)));
    out.days[i].code = codes[i] | 0;
    out.days[i].high = lround(highs[i] | 0.0f);
    out.days[i].low = lround(lows[i] | 0.0f);
    out.days[i].precipChance = precip[i] | 0;
  }

  // Hourly trend: copy HOURLY_POINTS samples starting at the current hour. The
  // hourly timestamps share the same basis as current.time, so the starting
  // index is the whole-hours elapsed since the first sample.
  JsonArray hourTimes = doc["hourly"]["time"].as<JsonArray>();
  JsonArray hourTemps = doc["hourly"]["temperature_2m"].as<JsonArray>();
  out.hourlyCount = 0;
  if (!hourTimes.isNull() && hourTimes.size() > 0) {
    const time_t firstHour = static_cast<time_t>(hourTimes[0] | 0UL);
    int start = static_cast<int>((currentUtc - firstHour) / 3600);
    if (start < 0) start = 0;
    for (int i = start; i < static_cast<int>(hourTemps.size()) &&
                        out.hourlyCount < HOURLY_POINTS;
         i++) {
      out.hourly[out.hourlyCount++] = lround(hourTemps[i] | 0.0f);
    }
  }
  return true;
}
