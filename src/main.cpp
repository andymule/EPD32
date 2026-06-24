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

#if defined(BAREBONES_EPD_TEST)
// Minimal, app-independent panel bring-up test. Bypasses Settings/WiFi/layouts
// and draws an unmistakable pattern straight to the panel so we can confirm
// pins + driver class + SPI in isolation. Enable via the s3_test env.
#include <Fonts/FreeSansBold18pt7b.h>

#include <GxEPD2_BW.h>
namespace {
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> testEpd{
    GxEPD2_154_D67(pins::EPD_CS, pins::EPD_DC, pins::EPD_RST, pins::EPD_BUSY)};

void runBarebonesEpdTest() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== BAREBONES EPD TEST ===");
  Serial.printf("pins CS=%d DC=%d RST=%d BUSY=%d SCK=%d MOSI=%d PWR=%d\n",
                pins::EPD_CS, pins::EPD_DC, pins::EPD_RST, pins::EPD_BUSY,
                pins::SPI_SCK, pins::SPI_MOSI, pins::EPD_PWR);

  // P-channel high-side switch (AO3401) on GPIO6 -> drive LOW to power the panel.
  pinMode(pins::EPD_PWR, OUTPUT);
  digitalWrite(pins::EPD_PWR, LOW);   // enable panel rail (active-LOW)
  pinMode(pins::EPD_TP_RST, OUTPUT);
  digitalWrite(pins::EPD_TP_RST, LOW);  // park touch in reset
  delay(100);
  pinMode(pins::EPD_BUSY, INPUT);
  Serial.printf("BUSY level after power-on (LOW): %d\n", digitalRead(pins::EPD_BUSY));
  Serial.println("power rail on, binding SPI...");

  SPI.begin(pins::SPI_SCK, pins::SPI_MISO, pins::SPI_MOSI, pins::EPD_CS);
  Serial.println("SPI ok, init panel...");
  testEpd.init(115200, true, 10, false);
  Serial.println("init done, drawing...");

  testEpd.setRotation(0);
  testEpd.setFullWindow();
  testEpd.firstPage();
  do {
    testEpd.fillScreen(GxEPD_WHITE);
    testEpd.drawRect(0, 0, 200, 200, GxEPD_BLACK);      // full border
    testEpd.fillRect(20, 20, 160, 70, GxEPD_BLACK);     // big solid block
    testEpd.fillRect(0, 0, 30, 30, GxEPD_BLACK);        // top-left corner mark
    testEpd.fillRect(170, 170, 30, 30, GxEPD_BLACK);    // bottom-right mark
    testEpd.setTextColor(GxEPD_BLACK);
    testEpd.setFont(&FreeSansBold18pt7b);
    testEpd.setCursor(30, 140);
    testEpd.print("HELLO");
  } while (testEpd.nextPage());

  testEpd.hibernate();
  Serial.println("=== TEST DRAW COMPLETE ===");
}
}  // namespace
#endif  // BAREBONES_EPD_TEST

void setup() {
#if defined(BAREBONES_EPD_TEST)
  runBarebonesEpdTest();
  return;
#endif
  [[maybe_unused]] uint32_t startMs = millis();
  LOG_BEGIN(115200);

  // Latch the battery rail on before any long work so the board stays powered
  // after the PWR button is released when running from battery (no-op on USB).
  sleepManager.initBoardPower();

#if !defined(BOARD_WAVESHARE_S3_154_TOUCH)
  // T5: the panel pins are the SPI bus defaults, so bind the bus here. The S3
  // board binds SPI inside Display::begin() (custom pins + panel power rail).
  SPI.begin(pins::SPI_SCK, pins::SPI_MISO, pins::SPI_MOSI, SS);
#endif
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
#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
    pinMode(pins::WAKE_BUTTON, INPUT_PULLUP);  // BOOT tap-to-exit in loop()
    pinMode(pins::PWR_BUTTON, INPUT_PULLUP);   // PWR power-off in loop()
#endif
    inSetupMode = true;
    return;
  }

  // Load any cached forecast (survives deep sleep) so an hourly wake can redraw
  // the live clock over yesterday's data without touching the network.
  Forecast forecast;
  String city = settings.city();
  bool haveCache = cache::load(forecast, city);

  // Network fetch only when: the user presses the button, after a hard reset,
  // once a day at FETCH_HOUR, whenever we have no usable cached data/clock, or
  // when a prior fetch failed and a retry is still owed (so a failed daily fetch
  // recovers on the backoff schedule instead of waiting a whole day).
  bool doFetch = reason == WakeReason::RefreshNow ||
                 reason == WakeReason::HardReset || !haveCache ||
                 sleepManager.fetchRetryPending();
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

  bool fetchFailed = false;  // attempted a fetch this wake but it didn't land
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
    } else {
      // Fetch failed but cached data exists: redraw the cache (below) with the
      // live clock, then sleep on the *retry* backoff so we re-attempt soon
      // instead of waiting for the next daily fetch hour.
      fetchFailed = true;
    }
  }

  [[maybe_unused]] uint32_t drawStart = millis();
  display.drawWeather(forecast, city);
  sleepManager.setWeatherOnScreen(true);
  [[maybe_unused]] uint32_t drawMs = millis() - drawStart;

  LOGF("Draw %lums, total %lums\n", (unsigned long)drawMs,
       (unsigned long)(millis() - startMs));
  if (fetchFailed)
    sleepManager.deepSleepRetry(display, settings);  // recover on backoff
  else
    sleepManager.deepSleep(display, settings, forecast.current.utcOffsetSeconds);
}

void loop() {
#if defined(BAREBONES_EPD_TEST)
  delay(1000);
  return;
#endif
  // Only the setup portal needs the main loop; normal runs deep-sleep in setup().
  if (!inSetupMode) return;
  setupPortal.handle();

#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
  // Tap the top (BOOT) button to leave setup and resume normal operation -- but
  // only once WiFi is already configured. Require a full release -> press ->
  // release cycle: the long-press that opened setup may still be held on entry,
  // and the button must be released (GPIO0 HIGH) when we reboot, or it would
  // strap the chip into ROM download mode.
  static bool sawRelease = false;  // button let go after entering setup
  static bool sawPress = false;    // a fresh press seen after that release
  const bool pressed = digitalRead(pins::WAKE_BUTTON) == LOW;
  if (!sawRelease) {
    sawRelease = !pressed;
  } else if (!sawPress) {
    sawPress = pressed;
  } else if (!pressed) {  // released after a fresh tap
    if (settings.ssid().length() > 0) {
      LOGLN("BOOT tap in setup -> resume normal mode");
      ESP.restart();
    }
    sawPress = false;  // no WiFi saved yet: stay in setup, wait for another tap
  }

  // PWR button powers the board off from setup, so it can't sit hosting the AP
  // and draining the battery if left unattended.
  if (digitalRead(pins::PWR_BUTTON) == LOW) {
    delay(20);  // debounce
    if (digitalRead(pins::PWR_BUTTON) == LOW) {
      LOGLN("PWR in setup -> power off");
      sleepManager.powerOff();  // does not return
    }
  }
#endif
}
