#pragma once
#include "net/WeatherProvider.h"

// Weather via Open-Meteo (keyless, HTTPS). Uses unix timestamps so no ISO date
// parsing is needed, and WMO weather codes mapped to text by WeatherCodes.
class OpenMeteoProvider : public WeatherProvider {
 public:
  bool fetch(HttpClientHelper& http, float lat, float lon, bool metric,
             Forecast& out) override;

 private:
  String buildUrl(float lat, float lon, bool metric);
};
