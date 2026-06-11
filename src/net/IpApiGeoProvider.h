#pragma once
#include "net/GeoProvider.h"

// Geolocation via ip-api.com (keyless, non-commercial, HTTP).
class IpApiGeoProvider : public GeoProvider {
 public:
  GeoResult fetch(HttpClientHelper& http) override;
};
