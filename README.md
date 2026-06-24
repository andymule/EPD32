# EPD32 / Atmo

A low-power ESP32 e-paper weather display. It fetches the local forecast over
WiFi once a day, caches it, then wakes briefly each hour to redraw the clock from
the cached data and deep-sleeps in between. A button press triggers an immediate
refresh. It renders to a black/white e-paper panel in one of two layouts.

Two boards are supported, selected by PlatformIO environment:

- **Lilygo T5 v2.2** — ESP32 + 2.9" 296×128 panel (`debug`/`release` envs).
- **Waveshare ESP32-S3-Touch-ePaper-1.54** — ESP32-S3 + 1.54" 200×200 panel,
  onboard battery charging + touch (`s3_touch` env). See `HARDWARE.md`.

## Documentation

- `HARDWARE.md` — board reference (pinout, power rules, peripherals, flashing) for
  building any firmware on the Waveshare S3 board.
- `BOARD_MIGRATION.md` — how Atmo was ported from the T5 to the S3.
- `ULP_REFRESH.md` — deferred ULP-driven-refresh plan and honest tradeoffs.

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

Build environments in `platformio.ini`: `debug` (default, T5 with serial logging
via `-DDEBUG_BUILD`), `release` (T5, no logging, smaller/lower power), and
`s3_touch` (Waveshare ESP32-S3 build — flashing notes in `HARDWARE.md`).

Dependencies are declared in `platformio.ini` and fetched automatically:

- [GxEPD2](https://github.com/ZinggJM/GxEPD2) - e-paper driver
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) (v7)
- [Time](https://github.com/PaulStoffregen/Time)

## Flashing to hardware

`pio run --target upload` (above) flashes the T5 v2.2. In practice:

```bash
# 1. Find the serial port. The board MUST be on a *data* USB cable, not a
#    charge-only one. A short on the Grove header can also drop the CP2104 USB
#    bridge so nothing enumerates -- if no port shows up it's almost always the
#    cable or a wiring short, not software.
pio device list                 # e.g. /dev/cu.usbserial-01B9721E

# 2. Build + upload the default (debug) env to that port.
pio run -e debug -t upload --upload-port /dev/cu.usbserial-01B9721E

# 3. (optional) watch it boot.
pio device monitor
```

Notes:

- The default upload speed is reliable; forcing a faster baud (e.g. 1.5 Mbaud)
  builds quicker but intermittently fails with "chip stopped responding" — just
  retry at the default if that happens.
- Uploading or opening the monitor toggles DTR/RTS, which resets the board, so
  it cold-boots on connect.
- **Settings survive a reflash.** WiFi credentials and preferences live in NVS, a
  separate flash partition that a normal app upload does *not* erase — so a
  previously-configured board boots straight back to the weather screen rather
  than the setup portal. To force first-run setup again, hold the setup button
  during boot/wake, or wipe NVS with `pio run -t erase` and re-upload.

## First-time setup

On first boot (or while holding the setup button during boot/wake) the device
hosts a WiFi access point named **Atmo**. Connect to it and browse to **at.mo**
to configure:

- WiFi network + password
- Units (°F / °C)
- Layout (Horizon / Dashboard)
- Orientation (portrait / landscape, normal or flipped)
- Daily update time (the local hour of the once-a-day network fetch)
- Quiet hours (a window when the hourly clock redraws pause)

Settings are submitted over a POST form (never in the URL) and stored in NVS.

## Refresh behavior

The device fetches the forecast over WiFi once a day (default 5am, configurable),
caches it in RTC memory, and renders it. In between it wakes at the top of each
hour to redraw the clock and the rolling 24-hour curve from the cached data (no
radio), then deep-sleeps. During the configurable **quiet hours** (default
10pm–5am) the hourly redraws pause and the device sleeps straight through — the
daily fetch still runs even if it falls inside that window.

Press the **wake button** for an immediate fetch + refresh, or hold the **setup
button** during boot/wake to reconfigure. If an update fails and there is no
cached frame to show, the device displays a brief error and retries with an
exponential backoff (1h, 2h, 4h, … capped at a day); if a cached frame already
exists it keeps showing it and tries again at the next daily fetch.

## Layouts & animations

Two resolution-independent layouts (selectable in setup):

- **Horizon** — a big hero icon + current temp with today's hi/lo, a smooth 24h
  temperature curve with dotted precipitation and sun/moon/sunrise/sunset marks,
  and a high-only day strip.
- **Dashboard** — header + hero block + precipitation trend + daily strip.

A first-boot splash ("Atmo" + a raining cloud) and an update cue (the current
hero icon sliding away on a button refresh) animate on a second core while the
network work runs on the main core, so they never delay the new frame.

## External services

Both services are keyless (no signup or API key required):

- Weather: [Open-Meteo](https://open-meteo.com/) (HTTPS)
- IP geolocation: [ip-api.com](https://ip-api.com/) (HTTP, non-commercial)

## Project layout

```
platformio.ini             Build configuration
include/Config.h           Pins, timeouts, NVS keys, API endpoints, schedule defaults
include/Log.h              Serial logging macros (compiled out without DEBUG_BUILD)
include/fonts/             Generated GFX fonts (FreeSans 6/7pt + oblique tagline)
include/Secrets.h.example  Optional dev-only WiFi creds template
src/main.cpp               Orchestration (setup/loop), update + animation flow
src/model/                 Weather data structs + RTC-memory forecast cache
src/settings/              Preferences (NVS) wrapper
src/net/                   WiFi, HTTP helper, geolocation + weather providers
src/weather/               WMO weather-code -> text mapping
src/display/               GxEPD2 wrapper, the two layouts, and animations
src/power/                 Deep sleep, wake reason, hourly + quiet-hours scheduling
src/setup/                 Captive configuration portal
tools/preview/             Desktop harness to render the layouts to PNG (see its README)
```

## Hardware

Two boards are supported (pin maps live in `include/Config.h`, the `pins`
namespace, selected by the `BOARD_WAVESHARE_S3_154_TOUCH` build flag). For the
Waveshare ESP32-S3 board's full pinout, power rules, and peripherals, see
`HARDWARE.md`; the section below documents the default Lilygo T5 v2.2.

Lilygo T5 v2.2 (ESP32 + integrated 2.9" b/w e-paper). No external wiring needed.
Pin assignments default to the T5 v2.2:

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
