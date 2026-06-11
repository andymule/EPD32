# EPD32 / Atmo

A low-power ESP32 e-paper weather display. It wakes once a day (or on a touch
tap), fetches the local forecast over WiFi, renders it to a 2.9" black/white
e-paper panel, and goes back into deep sleep.

## Building (PlatformIO)

This project uses [PlatformIO](https://platformio.org/).

```bash
# Install PlatformIO Core (once)
pip install platformio

# Build (debug = serial logging on)
pio run

# Build the silent, smaller production image
pio run -e release

# Flash + open the serial monitor
pio run --target upload
pio device monitor
```

Two build profiles are defined in `platformio.ini`: `debug` (default, serial
logging via `-DDEBUG_BUILD`) and `release` (no logging, smaller/lower power).

Dependencies are declared in `platformio.ini` and fetched automatically:

- [GxEPD2](https://github.com/ZinggJM/GxEPD2) - e-paper driver
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) (v7)
- [Time](https://github.com/PaulStoffregen/Time)

## First-time setup

On first boot (or after touching the setup pad) the device hosts a WiFi access
point named **Atmo**. Connect to it and browse to **at.mo** to enter your WiFi
credentials and choose units. Credentials are submitted over a POST form (never
in the URL) and stored in NVS.

## Refresh behavior

The display wakes once a day, fetches the forecast, renders it, and deep-sleeps.
If an update fails (WiFi or network hiccup) it keeps the last good frame on
screen and retries within an hour instead of going dark for a full day. Repeated
failures back the retry off exponentially (1h, 2h, 4h, ... capped at a day) so a
prolonged outage doesn't drain the battery with hourly radio wake-ups. Press the
wake button for an immediate refresh, or hold the setup button during boot/wake
to reconfigure.

## External services

Both services are keyless (no signup or API key required):

- Weather: [Open-Meteo](https://open-meteo.com/) (HTTPS)
- IP geolocation: [ip-api.com](https://ip-api.com/) (HTTP, non-commercial)

## Project layout

```
platformio.ini        Build configuration
include/Config.h       Pins, timeouts, NVS keys, API endpoints
include/Log.h          Serial logging macros (compiled out without DEBUG_BUILD)
include/Secrets.h.example  Optional dev-only WiFi creds template
src/main.cpp           Orchestration (setup/loop)
src/model/             Plain weather data structs
src/settings/          Preferences (NVS) wrapper
src/net/               WiFi, HTTP helper, geolocation + weather providers
src/weather/           WMO weather-code -> text mapping
src/display/           GxEPD2 wrapper + all rendering
src/power/             Deep sleep, wake reason, clock-drift compensation
src/setup/             Captive configuration portal
lib/tk_spline/         Vendored cubic spline library (currently unused)
assets/                Unused bitmap headers retained from earlier experiments
```

## Hardware

Lilygo T5 v2.2 (ESP32 + integrated 2.9" b/w e-paper). No external wiring needed.

Pin assignments live in `include/Config.h` (the `pins` namespace) and default to
the T5 v2.2:

| Signal | GPIO |       | Signal | GPIO |
| ------ | ---- | ----- | ------ | ---- |
| EPD CS | 5    |       | SCK    | 18   |
| EPD DC | 19   |       | MOSI   | 23   |
| EPD RST| 12   |       | MISO   | 2    |
| EPD BUSY | 4  |       |        |      |

Physical buttons (input-only RTC GPIOs, active-low): `GPIO 37` wakes the device
from deep sleep for an immediate refresh; `GPIO 39`, held during boot or wake,
forces the setup portal. `GPIO 38` is unused and free for future use.

For a different board or a bare ESP32 + e-paper panel, adjust these values in
`include/Config.h` to match your wiring.
