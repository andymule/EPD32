#pragma once
#include <Arduino.h>
#include <time.h>

#include "Config.h"

// A single day in the multi-day forecast strip.
struct WeatherDay {
  int high = 0;
  int low = 0;
  int precipChance = 0;
  int code = 0;      // WMO weather code (drives the day's icon)
  String dayOfWeek;  // 3-letter label, e.g. "Mon"
};

// "Right now" conditions shown at the top of the display.
struct CurrentConditions {
  int temperature = 0;
  int code = 0;          // WMO weather code (drives the hero icon)
  String skyText;        // human label mapped from the WMO weather code
  time_t utcEpoch = 0;   // UTC epoch of the observation (for drift compensation)
  long utcOffsetSeconds = 0;
};

// Hourly samples fetched and cached, starting at the fetch hour. We keep ~48h
// so the rendered 24h window can roll forward (left edge = "now") all day from a
// single daily fetch without running out of future data.
constexpr int HOURLY_POINTS = 48;

// Width of the rolling window actually drawn, in hours.
constexpr int HOURLY_WINDOW = 24;

// Full payload produced by a WeatherProvider and consumed by the Display.
struct Forecast {
  CurrentConditions current;
  WeatherDay days[FORECAST_DAYS];
  int temp[HOURLY_POINTS] = {0};    // hourly temperature from the fetch hour
  int precip[HOURLY_POINTS] = {0};  // hourly precip probability (%) from then
  int hourlyCount = 0;              // shared length of temp[]/precip[]
  int hourStart = 0;                // local clock hour (0-23) of sample[0]
  time_t hourStartUtc = 0;          // UTC epoch of sample[0] (for rolling window)
  int sunriseHour = -1;             // local clock hour of sunrise (0-23)
  int sunsetHour = -1;              // local clock hour of sunset (0-23)
};
