#include <Arduino.h>
#include <SPI.h>

#include "Config.h"
#include "Log.h"
#include "display/Display.h"
#include "model/WeatherData.h"
#include "net/HttpClientHelper.h"
#include "net/IpApiGeoProvider.h"
#include "net/OpenMeteoProvider.h"
#include "net/WiFiManager.h"
#include "power/SleepManager.h"
#include "settings/Settings.h"
#include "setup/SetupPortal.h"

// Optional, gitignored, copied from include/Secrets.h.example. When present it
// provides DEV_WIFI_SSID / DEV_WIFI_PASSWORD to skip the captive portal during
// local development.
#if __has_include("Secrets.h")
#include "Secrets.h"
#endif

namespace {
Settings settings;
Display display;
SleepManager sleepManager;
WiFiManager wifiManager;
SetupPortal setupPortal;
bool inSetupMode = false;

// Failure handlers: keep the last good weather frame on screen if there is one,
// otherwise show an error, then sleep for the short retry interval (not a full
// day) so a transient outage recovers quickly.
[[noreturn]] void failWiFi(const String& ssid) {
  if (!sleepManager.weatherOnScreen()) display.drawFailedToConnectToWiFi(ssid);
  sleepManager.deepSleepRetry(display, settings);
}

[[noreturn]] void failSite() {
  if (!sleepManager.weatherOnScreen()) display.drawFailedToConnectToSite();
  sleepManager.deepSleepRetry(display, settings);
}
}  // namespace

void setup() {
  [[maybe_unused]] uint32_t startMs = millis();
  LOG_BEGIN(115200);

  SPI.begin(pins::SPI_SCK, pins::SPI_MISO, pins::SPI_MOSI, SS);
  display.begin();
  display.setRotation(3);
  settings.begin();

  WakeReason reason = sleepManager.checkWakeReason(display, settings);
  String ssid = settings.ssid();
  String password = settings.password();

  // Fall back to compile-time development credentials if none are stored.
#if defined(DEV_WIFI_SSID)
  if (ssid.length() == 0 && DEV_WIFI_SSID[0] != '\0') {
    ssid = DEV_WIFI_SSID;
    password = DEV_WIFI_PASSWORD;
    LOGLN("Using development WiFi credentials from Secrets.h");
  }
#endif

  // First-run or explicit setup request: host the captive configuration portal.
  if (reason == WakeReason::EnterSettings || ssid.length() == 0) {
    LOGLN("Entering setup");
    sleepManager.setWeatherOnScreen(false);
    display.drawConnectionInstructions();
    setupPortal.begin(settings, display);
    inSetupMode = true;
    return;
  }

  // Connect on a background task while we draw the "updating" frame.
  wifiManager.beginAsync(ssid, password);
  display.drawUpdating();

  HttpClientHelper http;

  // Resolve location once via IP geolocation, then cache it in settings.
  if (!settings.isValid()) {
    if (!wifiManager.ensureConnected()) failWiFi(ssid);
    IpApiGeoProvider geo;
    GeoResult location = geo.fetch(http);
    if (!location.ok) failSite();
    settings.setLocation(location.lat, location.lon);
    settings.setCity(location.city);
    settings.setValid(true);
  }

  if (!wifiManager.ensureConnected()) failWiFi(ssid);

  Forecast forecast;
  OpenMeteoProvider weather;
  [[maybe_unused]] uint32_t fetchStart = millis();
  bool ok = weather.fetch(http, settings.latitude(), settings.longitude(),
                          settings.useMetric(), forecast);
  wifiManager.stop();
  if (!ok) failSite();
  [[maybe_unused]] uint32_t fetchMs = millis() - fetchStart;

  sleepManager.setClockAndCompensate(forecast.current.utcEpoch);

  [[maybe_unused]] uint32_t drawStart = millis();
  display.drawWeather(forecast, settings.city());
  sleepManager.setWeatherOnScreen(true);
  [[maybe_unused]] uint32_t drawMs = millis() - drawStart;

  LOGF("Fetch %lums, draw %lums, total %lums\n", (unsigned long)fetchMs,
       (unsigned long)drawMs, (unsigned long)(millis() - startMs));
  sleepManager.deepSleep(display, settings);
}

void loop() {
  // Only the setup portal needs the main loop; normal runs deep-sleep in setup().
  if (inSetupMode) setupPortal.handle();
}
