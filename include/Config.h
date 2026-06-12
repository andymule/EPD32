#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Hardware pins (Lilygo T5 v2.2: ESP32 + 2.9" b/w e-paper, integrated wiring)
// ---------------------------------------------------------------------------
namespace pins {
constexpr int EPD_CS = 5;
constexpr int EPD_DC = 19;
constexpr int EPD_RST = 12;
constexpr int EPD_BUSY = 4;

constexpr int SPI_SCK = 18;
constexpr int SPI_MISO = 2;
constexpr int SPI_MOSI = 23;

// Physical side buttons (LilyGo T5: input-only RTC GPIOs, active-low, held high
// by the board's external pull-ups until pressed).
//   WAKE_BUTTON  - wakes the device from deep sleep for an immediate refresh.
//   SETUP_BUTTON - forces the configuration portal when held during boot/wake.
constexpr int WAKE_BUTTON = 37;
constexpr int SETUP_BUTTON = 39;
}  // namespace pins

// ---------------------------------------------------------------------------
// Timing constants
// ---------------------------------------------------------------------------
namespace timing {
constexpr uint64_t ONE_SECOND_US = 1000000ULL;
constexpr uint64_t ONE_MINUTE_US = ONE_SECOND_US * 60;
constexpr uint64_t ONE_HOUR_US = ONE_MINUTE_US * 60;
constexpr uint64_t ONE_DAY_US = ONE_HOUR_US * 24;

// Normal operation wakes at the top of every hour to redraw the clock from
// cached data (see SleepManager::deepSleep); a network fetch happens only once a
// day at FETCH_HOUR, on a button press, or on a hard reset.
//
// Shorter sleep used to retry soon after a failed update (network hiccup etc.)
// instead of going dark for a full day. On repeated failures the retry interval
// backs off exponentially (1h, 2h, 4h, ...) so a prolonged outage (router down,
// travel, dead ISP) doesn't flatten the battery with hourly radio wake-ups.
constexpr uint64_t RETRY_INTERVAL_US = ONE_HOUR_US;

// Ceiling for the exponential retry backoff: never sleep longer than the normal
// daily refresh, so the display still attempts to recover at least once a day.
constexpr uint64_t MAX_RETRY_INTERVAL_US = ONE_DAY_US;

// Generous enough to cover a TLS handshake plus the response on a slow link
// (a tight 5s window intermittently tripped HTTPC_ERROR_READ_TIMEOUT / -11).
constexpr uint32_t SITE_TIMEOUT_MS = 8000;
constexpr uint32_t WIFI_POLL_INTERVAL_MS = 50;
constexpr uint32_t WIFI_TIMEOUT_MS = 10000;

constexpr int HTTP_MAX_RETRIES = 3;
constexpr uint32_t HTTP_RETRY_DELAY_MS = 1000;
}  // namespace timing

// ---------------------------------------------------------------------------
// Persisted-settings keys (NVS namespace + entries)
// ---------------------------------------------------------------------------
namespace prefs {
constexpr const char* NAMESPACE = "settings";
constexpr const char* VALID = "valid";
constexpr const char* METRIC = "useMetric";
constexpr const char* LAT = "lat";
constexpr const char* LON = "lon";
constexpr const char* CITY = "city";
constexpr const char* PASSWORD = "wifi_password";
constexpr const char* SSID = "wifi_ssid";
constexpr const char* STYLE = "layoutStyle";  // 0=Dashboard, 1=Horizon
constexpr const char* ROTATION = "rotation";  // 0/2=portrait, 1/3=landscape
constexpr const char* FETCH_HOUR = "fetchHour";    // local hour of daily fetch
constexpr const char* QUIET_START = "quietStart";  // hourly redraws pause from..
constexpr const char* QUIET_END = "quietEnd";      // ..until this local hour
}  // namespace prefs

// Schedule defaults (local hours, 0-23) used when nothing is stored yet.
namespace sched_defaults {
constexpr uint8_t FETCH_HOUR = 5;    // daily network fetch at 5am
constexpr uint8_t QUIET_START = 22;  // no hourly redraws 10pm..
constexpr uint8_t QUIET_END = 5;     // ..through 5am (the fetch still happens)
}  // namespace sched_defaults

// Display layout/orientation defaults (used when nothing is stored yet).
namespace display_defaults {
constexpr uint8_t STYLE = 1;     // Horizon
constexpr uint8_t ROTATION = 0;  // portrait (128x296)
}  // namespace display_defaults

// ---------------------------------------------------------------------------
// External services (both keyless / no signup required)
// ---------------------------------------------------------------------------
namespace api {
// ip-api.com: keyless IP geolocation (non-commercial, HTTP only, 45 req/min).
constexpr const char* GEO_URL =
    "http://ip-api.com/json/?fields=status,city,lat,lon,offset,timezone";

// Open-Meteo: keyless weather forecast (HTTPS). Query params appended at runtime.
constexpr const char* WEATHER_BASE_URL = "https://api.open-meteo.com/v1/forecast";
}  // namespace api

// ---------------------------------------------------------------------------
// Captive setup portal
// ---------------------------------------------------------------------------
namespace setup_portal {
constexpr const char* AP_NAME = "Atmo";
constexpr const char* LOCAL_HOSTNAME = "at.mo";
constexpr byte DNS_PORT = 53;
}  // namespace setup_portal

// Number of forecast days rendered (today + following days).
constexpr int FORECAST_DAYS = 6;
