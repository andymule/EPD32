// Desktop preview of the Atmo weather layout. Renders the shared layout at
// several panel resolutions to PGM files so the GUI can be designed visually.
#include <cstdio>
#include <string>

#include "host_gfx.h"
#include "../../src/display/weather_layout.h"

#include "fonts/FreeSans6pt7b.h"
#include "fonts/FreeSans7pt7b.h"
#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSans12pt7b.h"
#include "fonts/FreeSans18pt7b.h"
#include "fonts/FreeSans24pt7b.h"
#include "fonts/FreeSansBold6pt7b.h"
#include "fonts/FreeSansBold7pt7b.h"
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
  atmo::FontSet F{&FreeSans6pt7b,      &FreeSansBold6pt7b,  &FreeSans7pt7b,
                  &FreeSansBold7pt7b,  &FreeSans9pt7b,      &FreeSansBold9pt7b,
                  &FreeSans12pt7b,     &FreeSansBold12pt7b, &FreeSans18pt7b,
                  &FreeSansBold18pt7b, &FreeSans24pt7b,     &FreeSansBold24pt7b};

  // days[0] is today; the forecast strip renders the days ahead (from days[1]).
  atmo::DayView days[6] = {{"Wed", 51, 21, 17, 68}, {"Thu", 0, 21, 14, 65},
                           {"Fri", 3, 23, 15, 6},   {"Sat", 63, 24, 16, 62},
                           {"Sun", 1, 19, 12, 12},  {"Mon", 1, 20, 13, 80}};
  // Sample next-24h temperature and precip probability (%) starting at 9:00 — a
  // warm dry morning, a cooler wet afternoon/evening. Index 15 is local midnight.
  int temp[24] = {21, 22, 23, 24, 24, 23, 22, 21, 20, 19, 19, 18,
                  18, 17, 17, 16, 16, 17, 18, 19, 20, 21, 22, 22};
  int precip[24] = {0,  0,  5,  10, 20, 35, 55, 75, 80, 65, 45, 30,
                    20, 15, 10, 5,  10, 20, 35, 25, 15, 10, 5,  0};
  atmo::WeatherView v{"Evanston", "Wed Jun 10", "9:53", "Clear", 21, 0,
                      days,       6,            temp,   precip,  24, 9,
                      6,          20,           64};  // sunrise 6am, sunset 8pm, 64% RH

  struct Screen { const char* name; int w; int h; };
  Screen screens[] = {{"t5_296x128", 296, 128},
                      {"ws_200x200", 200, 200},
                      {"portrait_128x296", 128, 296}};
  struct Style { const char* name; atmo::LayoutStyle style; };
  Style styles[] = {{"dashboard", atmo::LayoutStyle::Dashboard},
                    {"horizon", atmo::LayoutStyle::Horizon}};

  for (const Style& st : styles) {
    for (const Screen& s : screens) {
      HostGFX g(s.w, s.h);
      atmo::renderWeather(g, v, F, st.style);
      std::string path =
          std::string("/tmp/atmo_") + st.name + "_" + s.name + ".pgm";
      writePGM(path, g);
      printf("wrote %s (%dx%d)\n", path.c_str(), s.w, s.h);
    }
  }
  return 0;
}
