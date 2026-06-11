// Desktop preview of the Atmo weather layout. Renders the shared layout at
// several panel resolutions to PGM files so the GUI can be designed visually.
#include <cstdio>
#include <string>

#include "host_gfx.h"
#include "../../src/display/weather_layout.h"

#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSans12pt7b.h"
#include "fonts/FreeSans18pt7b.h"
#include "fonts/FreeSans24pt7b.h"
#include "fonts/FreeSansBold9pt7b.h"
#include "fonts/FreeSansBold12pt7b.h"
#include "fonts/FreeSansBold18pt7b.h"
#include "fonts/FreeSansBold24pt7b.h"

static void writePGM(const std::string& path, const HostGFX& g) {
  FILE* f = fopen(path.c_str(), "wb");
  fprintf(f, "P5\n%d %d\n255\n", g.width(), g.height());
  fwrite(g.buffer().data(), 1, g.buffer().size(), f);
  fclose(f);
}

int main() {
  atmo::FontSet F{&FreeSans9pt7b,      &FreeSansBold9pt7b,  &FreeSans12pt7b,
                  &FreeSansBold12pt7b, &FreeSans18pt7b,     &FreeSansBold18pt7b,
                  &FreeSans24pt7b,     &FreeSansBold24pt7b};

  atmo::DayView days[6] = {{"Wed", 51, 92, 65, 68}, {"Thu", 95, 86, 65, 65},
                           {"Fri", 3, 80, 58, 6},   {"Sat", 53, 85, 66, 62},
                           {"Sun", 1, 76, 62, 12},  {"Mon", 71, 41, 30, 80}};
  // Sample next-24h temperatures (a gentle day curve).
  int hourly[24] = {72, 71, 70, 69, 68, 68, 69, 72, 76, 80, 84, 87,
                    90, 92, 92, 91, 89, 86, 82, 79, 77, 75, 74, 73};
  atmo::WeatherView v{"Evanston", "Wed Nov 3", "Drizzle", 77, 51,
                      days,       6,           hourly,    24};

  struct Screen { const char* name; int w; int h; };
  Screen screens[] = {{"t5_296x128", 296, 128},
                      {"ws_200x200", 200, 200},
                      {"big_800x480", 800, 480},
                      {"portrait_128x296", 128, 296}};

  for (const Screen& s : screens) {
    HostGFX g(s.w, s.h);
    atmo::renderWeather(g, v, F);
    std::string path = std::string("/tmp/atmo_") + s.name + ".pgm";
    writePGM(path, g);
    printf("wrote %s (%dx%d)\n", path.c_str(), s.w, s.h);
  }
  return 0;
}
