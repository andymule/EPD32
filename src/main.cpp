#include <Arduino.h>
#include <SPI.h>
#include <TimeLib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "Config.h"
#include "Log.h"
#include "display/Display.h"
#include "model/ForecastCache.h"
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

// The full network update (connect-wait + one-time geolocation + weather fetch),
// run on the main core. Factored out so it can run inline (silent timer wakes)
// or alongside the intro animation (which runs on the *other* core).
struct FetchOutcome {
  bool fetched = false;
  bool wifiFailed = false;
};

FetchOutcome performFetch(Forecast& out) {
  FetchOutcome r;
  HttpClientHelper http;
  do {
    if (!settings.isValid()) {
      if (!wifiManager.ensureConnected()) { r.wifiFailed = true; break; }
      IpApiGeoProvider geo;
      GeoResult location = geo.fetch(http);
      if (!location.ok) break;
      settings.setLocation(location.lat, location.lon);
      settings.setCity(location.city);
      settings.setValid(true);
    }
    if (!wifiManager.ensureConnected()) { r.wifiFailed = true; break; }
    OpenMeteoProvider weather;
    if (!weather.fetch(http, settings.latitude(), settings.longitude(),
                       settings.useMetric(), out))
      break;
    r.fetched = true;
  } while (false);
  return r;
}

// Intro animation runs on a dedicated task (the ESP32's second core), so it can
// keep drawing throughout the *entire* connect+fetch and stop the instant the
// data lands — the heavy network work stays on the main core, untouched.
volatile bool g_stopAnim = false;
SemaphoreHandle_t g_animDone = nullptr;
struct IntroCtx {
  WakeReason reason;
  int weatherCode;  // snapshot of the on-screen icon (no race with the fetch)
  bool haveCache;
};
IntroCtx g_introCtx;

void introTaskFn(void*) {
  if (g_introCtx.reason == WakeReason::HardReset) {
    // First-boot splash: loop until interrupted (data arrived) or rain bottoms.
    int f = 0;
    bool more = true;
    while (!g_stopAnim) {
      if (more) more = display.drawSplashFrame(f++);
      else vTaskDelay(pdMS_TO_TICKS(50));
    }
  } else if (g_introCtx.haveCache) {
    // Button press: play the full hero-icon slide to completion (it looks better
    // finished than cut off), regardless of when the data lands. The frame call
    // reports when the icon is fully gone, so there's no step count to keep in
    // sync here.
    for (int f = 1; display.animateHeroOutFrame(f, g_introCtx.weatherCode); f++) {
    }
  }
  xSemaphoreGive(g_animDone);
  vTaskDelete(nullptr);
}

// Starts the intro animation task. Returns true only if the task was actually
// created — the caller must NOT join (stopIntro) otherwise, or it would wait
// forever on a semaphore that never gets given.
bool startIntro(WakeReason reason, int weatherCode, bool haveCache) {
  g_introCtx = {reason, weatherCode, haveCache};
  g_stopAnim = false;
  if (g_animDone == nullptr) g_animDone = xSemaphoreCreateBinary();
  if (g_animDone == nullptr) return false;
  // Pin to the app core (1), the same core the main loop runs on: while the main
  // core blocks on network I/O, the scheduler hands this task the CPU. The panel
  // (SPI) and the radio don't share a bus, so they progress independently.
  BaseType_t ok = xTaskCreatePinnedToCore(introTaskFn, "intro", 8192, nullptr, 1,
                                          nullptr, 1);
  return ok == pdPASS;
}

void stopIntro() {
  g_stopAnim = true;                                  // ask the splash loop to end
  xSemaphoreTake(g_animDone, portMAX_DELAY);          // join (+memory barrier)
}
}  // namespace

void setup() {
  [[maybe_unused]] uint32_t startMs = millis();
  LOG_BEGIN(115200);

  SPI.begin(pins::SPI_SCK, pins::SPI_MISO, pins::SPI_MOSI, SS);
  display.begin();
  settings.begin();
  // Apply the user-chosen orientation and layout before anything is drawn.
  display.setRotation(settings.rotation());
  display.setLayoutStyle(settings.layoutStyle());

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

  // Load any cached forecast (survives deep sleep) so an hourly wake can redraw
  // the live clock over yesterday's data without touching the network.
  Forecast forecast;
  String city = settings.city();
  bool haveCache = cache::load(forecast, city);

  // Network fetch only when: the user presses the button, after a hard reset,
  // once a day at FETCH_HOUR, or whenever we have no usable cached data/clock.
  bool doFetch = reason == WakeReason::RefreshNow ||
                 reason == WakeReason::HardReset || !haveCache;
  if (!doFetch && reason == WakeReason::TimedRefresh) {
    if (sleepManager.clockValid()) {
      time_t localNow =
          sleepManager.currentUtc() + forecast.current.utcOffsetSeconds;
      if (hour(localNow) == settings.fetchHour()) doFetch = true;
    } else {
      doFetch = true;  // no real clock yet -> must fetch to set it
    }
  }
  LOGF("Wake reason %d, cache %d, fetch %d\n", (int)reason, (int)haveCache,
       (int)doFetch);

  if (doFetch) {
    // Kick off WiFi association, then run an intro animation on the second core
    // for the whole connect+fetch — the main core does the network work and the
    // animation stops the instant data lands, so it never delays the new frame.
    //   - First boot/hard reset: the "Atmo" + cloud + rain splash.
    //   - Physical button press: slide the current hero icon off to the right.
    //   - Hourly timer wakes: no animation, just update.
    wifiManager.beginAsync(ssid, password);
    bool animated = reason == WakeReason::HardReset ||
                    (reason == WakeReason::RefreshNow && haveCache);
    bool animating = animated && startIntro(reason, forecast.current.code, haveCache);

    [[maybe_unused]] uint32_t fetchStart = millis();
    FetchOutcome r = performFetch(forecast);
    wifiManager.stop();

    if (animating) stopIntro();  // halt the animation and reclaim the panel
    LOGF("Fetch %lums, ok %d\n", (unsigned long)(millis() - fetchStart),
         (int)r.fetched);

    if (r.fetched) {
      sleepManager.setClock(forecast.current.utcEpoch);
      city = settings.city();
      cache::store(forecast, city);
    } else if (!haveCache) {
      // No fresh data and nothing cached to show: keep the existing frame (if
      // any) and back off with the retry interval.
      if (r.wifiFailed) failWiFi(ssid);
      failSite();
    }
    // Otherwise: fetch failed but cached data exists -> fall through and redraw
    // it (with the live clock), then resume the normal hourly schedule.
  }

  [[maybe_unused]] uint32_t drawStart = millis();
  display.drawWeather(forecast, city);
  sleepManager.setWeatherOnScreen(true);
  [[maybe_unused]] uint32_t drawMs = millis() - drawStart;

  LOGF("Draw %lums, total %lums\n", (unsigned long)drawMs,
       (unsigned long)(millis() - startMs));
  sleepManager.deepSleep(display, settings, forecast.current.utcOffsetSeconds);
}

void loop() {
  // Only the setup portal needs the main loop; normal runs deep-sleep in setup().
  if (inSetupMode) setupPortal.handle();
}
