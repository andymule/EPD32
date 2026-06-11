#include "weather/WeatherCodes.h"

namespace weather_codes {

String shortText(int wmoCode) {
  switch (wmoCode) {
    case 0:   // Clear sky
    case 1:   // Mainly clear
      return "clear";
    case 2:  // Partly cloudy
      return "cloudy";
    case 3:  // Overcast
      return "overcast";
    case 45:  // Fog
    case 48:  // Depositing rime fog
      return "fog";
    case 51:  // Drizzle: light
    case 53:  // Drizzle: moderate
    case 55:  // Drizzle: dense
      return "drizzle";
    case 56:  // Freezing drizzle: light
    case 57:  // Freezing drizzle: dense
      return "icy";
    case 61:  // Rain: slight
    case 63:  // Rain: moderate
      return "rain";
    case 65:  // Rain: heavy
      return "heavy rain";
    case 66:  // Freezing rain: light
    case 67:  // Freezing rain: heavy
      return "icy rain";
    case 71:  // Snow fall: slight
    case 73:  // Snow fall: moderate
      return "snow";
    case 75:  // Snow fall: heavy
      return "heavy snow";
    case 77:  // Snow grains
      return "snow";
    case 80:  // Rain showers: slight
    case 81:  // Rain showers: moderate
    case 82:  // Rain showers: violent
      return "showers";
    case 85:  // Snow showers: slight
    case 86:  // Snow showers: heavy
      return "snow";
    case 95:  // Thunderstorm
    case 96:  // Thunderstorm with slight hail
    case 99:  // Thunderstorm with heavy hail
      return "storms";
    default:
      return "";
  }
}

}  // namespace weather_codes
