#pragma once
#include <Arduino.h>

class HttpClientHelper;

struct GeoResult {
  bool ok = false;
  float lat = 0.0f;
  float lon = 0.0f;
  String city;
  long utcOffsetSeconds = 0;
};

// Interface for IP-based geolocation lookups so the concrete service can be
// swapped without touching the orchestration code.
class GeoProvider {
 public:
  virtual ~GeoProvider() = default;
  virtual GeoResult fetch(HttpClientHelper& http) = 0;
};
