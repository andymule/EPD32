#pragma once
#include <Arduino.h>

#include "model/WeatherData.h"

class HttpClientHelper;

// Interface for fetching the current conditions + multi-day forecast.
class WeatherProvider {
 public:
  virtual ~WeatherProvider() = default;

  // Fills `out` and returns true on success.
  virtual bool fetch(HttpClientHelper& http, float lat, float lon, bool metric,
                     Forecast& out) = 0;
};
