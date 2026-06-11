#pragma once
#include <Arduino.h>

// Helpers for interpreting Open-Meteo data into short, display-friendly text.
namespace weather_codes {

// Maps a WMO weather interpretation code (returned by Open-Meteo as
// `weather_code`) to a concise lowercase label that fits the small e-paper.
String shortText(int wmoCode);

}  // namespace weather_codes
