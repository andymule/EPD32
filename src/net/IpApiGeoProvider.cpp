#include "net/IpApiGeoProvider.h"

#include <ArduinoJson.h>

#include "Config.h"
#include "Log.h"
#include "net/HttpClientHelper.h"

GeoResult IpApiGeoProvider::fetch(HttpClientHelper& http) {
  GeoResult result;

  int code = http.get(api::GEO_URL);
  if (code != 200) {
    LOGLN("Geolocation request failed: " + String(code));
    http.end();
    return result;
  }

  // Read the full body before parsing (see HttpClientHelper::body): robust to
  // chunked responses where a direct stream parse can come up empty.
  String payload = http.body();
  http.end();
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    LOGLN("Geolocation JSON parse failed: " + String(error.c_str()));
    return result;
  }
  if (String(doc["status"] | "") != "success") {
    LOGLN("Geolocation status not 'success'");
    return result;
  }

  result.city = String(doc["city"] | "");
  result.lat = doc["lat"] | 0.0f;
  result.lon = doc["lon"] | 0.0f;
  result.utcOffsetSeconds = doc["offset"] | 0L;
  result.ok = true;
  LOGLN("Geolocation: " + result.city);
  return result;
}
