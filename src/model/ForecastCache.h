#pragma once
#include <Arduino.h>

#include "model/WeatherData.h"

// Persists the most recent forecast in RTC memory so the device can wake hourly
// and redraw (with a fresh clock) without hitting the network. The store is a
// flat POD in RTC slow memory; it survives deep sleep but not a power loss.
namespace cache {

// True when a previously stored forecast is present and intact.
bool valid();

// Copies a fetched forecast (and the city label) into RTC memory.
void store(const Forecast& forecast, const String& city);

// Reconstructs the cached forecast and city. Returns false if nothing valid is
// cached (forecast/city are then left untouched).
bool load(Forecast& forecast, String& city);

// Drops the cached forecast (forces a fetch on the next wake).
void invalidate();

}  // namespace cache
