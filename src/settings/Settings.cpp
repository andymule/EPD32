#include "settings/Settings.h"

#include "Config.h"

void Settings::begin() { prefs_.begin(prefs::NAMESPACE); }

void Settings::end() { prefs_.end(); }

bool Settings::isValid() { return prefs_.getBool(prefs::VALID, false); }

void Settings::setValid(bool valid) { prefs_.putBool(prefs::VALID, valid); }

bool Settings::useMetric() { return prefs_.getBool(prefs::METRIC, false); }

void Settings::setUseMetric(bool metric) { prefs_.putBool(prefs::METRIC, metric); }

float Settings::latitude() { return prefs_.getFloat(prefs::LAT, 0.0f); }

float Settings::longitude() { return prefs_.getFloat(prefs::LON, 0.0f); }

void Settings::setLocation(float lat, float lon) {
  prefs_.putFloat(prefs::LAT, lat);
  prefs_.putFloat(prefs::LON, lon);
}

String Settings::city() { return prefs_.getString(prefs::CITY, ""); }

void Settings::setCity(const String& city) { prefs_.putString(prefs::CITY, city); }

String Settings::ssid() { return prefs_.getString(prefs::SSID, ""); }

void Settings::setSsid(const String& ssid) { prefs_.putString(prefs::SSID, ssid); }

String Settings::password() { return prefs_.getString(prefs::PASSWORD, ""); }

void Settings::setPassword(const String& password) {
  prefs_.putString(prefs::PASSWORD, password);
}

uint8_t Settings::layoutStyle() {
  return prefs_.getUChar(prefs::STYLE, display_defaults::STYLE);
}

void Settings::setLayoutStyle(uint8_t style) {
  prefs_.putUChar(prefs::STYLE, style);
}

uint8_t Settings::rotation() {
  return prefs_.getUChar(prefs::ROTATION, display_defaults::ROTATION);
}

void Settings::setRotation(uint8_t rotation) {
  prefs_.putUChar(prefs::ROTATION, rotation);
}

uint8_t Settings::fetchHour() {
  return prefs_.getUChar(prefs::FETCH_HOUR, sched_defaults::FETCH_HOUR);
}

void Settings::setFetchHour(uint8_t hour) { prefs_.putUChar(prefs::FETCH_HOUR, hour); }

uint8_t Settings::quietStart() {
  return prefs_.getUChar(prefs::QUIET_START, sched_defaults::QUIET_START);
}

void Settings::setQuietStart(uint8_t hour) {
  prefs_.putUChar(prefs::QUIET_START, hour);
}

uint8_t Settings::quietEnd() {
  return prefs_.getUChar(prefs::QUIET_END, sched_defaults::QUIET_END);
}

void Settings::setQuietEnd(uint8_t hour) { prefs_.putUChar(prefs::QUIET_END, hour); }
