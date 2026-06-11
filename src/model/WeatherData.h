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

// Number of hourly temperature samples kept for the trend curve.
constexpr int HOURLY_POINTS = 24;

// Full payload produced by a WeatherProvider and consumed by the Display.
struct Forecast {
  CurrentConditions current;
  WeatherDay days[FORECAST_DAYS];
  int hourly[HOURLY_POINTS] = {0};  // upcoming temperatures from the current hour
  int hourlyCount = 0;
};
