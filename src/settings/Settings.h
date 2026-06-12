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

  // Weather layout style: 0 = Dashboard, 1 = Horizon.
  uint8_t layoutStyle();
  void setLayoutStyle(uint8_t style);

  // Panel rotation passed to GxEPD2 (0/2 = portrait, 1/3 = landscape).
  uint8_t rotation();
  void setRotation(uint8_t rotation);

  // Local hour (0-23) of the daily network fetch.
  uint8_t fetchHour();
  void setFetchHour(uint8_t hour);

  // Quiet window [start, end): hourly clock redraws pause (the daily fetch still
  // runs). start == end disables quiet hours. Hours are local, 0-23.
  uint8_t quietStart();
  void setQuietStart(uint8_t hour);
  uint8_t quietEnd();
  void setQuietEnd(uint8_t hour);

 private:
  Preferences prefs_;
};
