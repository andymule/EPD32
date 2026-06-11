#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Typed wrapper around the ESP32 Preferences (NVS) store. Replaces the raw
// global `Prefs` object and the scattered PREF_* string macros.
class Settings {
 public:
  void begin();
  void end();

  // Whether a usable location is stored (drives the geolocation step).
  bool isValid();
  void setValid(bool valid);

  bool useMetric();
  void setUseMetric(bool metric);

  float latitude();
  float longitude();
  void setLocation(float lat, float lon);

  String city();
  void setCity(const String& city);

  String ssid();
  void setSsid(const String& ssid);

  String password();
  void setPassword(const String& password);

 private:
  Preferences prefs_;
};
