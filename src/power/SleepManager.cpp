#include "power/SleepManager.h"

#include <SPI.h>
#include <TimeLib.h>
#include <esp_sleep.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "Config.h"
#include "Log.h"
#include "display/Display.h"
#include "esp32/ulp.h"
#include "settings/Settings.h"

namespace {
// Any epoch beyond this means the RTC has been set to a real time (2020-01-20).
constexpr time_t kClockValidAfter = 1579461559;

// Quiet window [qs, qe): hourly redraws pause. qs == qe disables it. Wraps
// midnight when qs > qe.
bool isQuietHour(int h, int qs, int qe) {
  if (qs == qe) return false;
  return qs < qe ? (h >= qs && h < qe) : (h >= qs || h < qe);
}

// A wake at local hour h is allowed if it's outside quiet hours, or it's the
// daily fetch hour (which must always fire, even inside the quiet window).
bool wakeAllowed(int h, int qs, int qe, int fetchHour) {
  return !isQuietHour(h, qs, qe) || h == fetchHour;
}
}  // namespace

// Number of consecutive failed updates, used to back the retry interval off
// exponentially. Survives deep sleep; reset to zero after any successful update.
RTC_DATA_ATTR static uint32_t consecutiveFailures = 0;

// True while the e-paper is showing a good weather frame (survives deep sleep).
RTC_DATA_ATTR static bool screenShowsWeather = false;

void SleepManager::setClock(time_t utcEpoch) {
  // settimeofday drives the ESP32 RTC, which keeps ticking through deep sleep,
  // so time(nullptr) stays correct on later (network-free) hourly wakes.
  struct timeval tv;
  tv.tv_sec = utcEpoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  setTime(utcEpoch);  // keep TimeLib in sync for any of its consumers
}

bool SleepManager::clockValid() { return time(nullptr) > kClockValidAfter; }

time_t SleepManager::currentUtc() { return time(nullptr); }

void SleepManager::enableWakeSources() {
  // Wake button (active-low) via ext1. ext1 is chosen over ext0/touch because it
  // lets the RTC peripheral power domain switch off during deep sleep (lowest
  // sleep current); the board's external pull-up holds the line high until the
  // button is pressed and pulls it low.
  esp_sleep_enable_ext1_wakeup(1ULL << pins::WAKE_BUTTON, ESP_EXT1_WAKEUP_ALL_LOW);
}

bool SleepManager::setupButtonHeld() {
  // Active-low, debounced so a transient can't drop the device into the portal.
  pinMode(pins::SETUP_BUTTON, INPUT);
  if (digitalRead(pins::SETUP_BUTTON) != LOW) return false;
  delay(20);
  return digitalRead(pins::SETUP_BUTTON) == LOW;
}

[[noreturn]] void SleepManager::deepSleep(Display& display, Settings& settings,
                                          long utcOffsetSeconds) {
  // Read schedule settings before closing the NVS handle.
  const int fetchHour = settings.fetchHour();
  const int quietStart = settings.quietStart();
  const int quietEnd = settings.quietEnd();
  settings.end();
  display.hibernate();
  SPI.end();

  // A full refresh reached this point, so the device is healthy again: clear the
  // failure streak that drives the retry backoff.
  consecutiveFailures = 0;

  enableWakeSources();

  // Wake at the top of the next *allowed* local hour so the on-screen clock is
  // redrawn hourly from cached data — but skip the quiet window (e.g. 10pm-5am),
  // except for the daily fetch hour which always fires. Fall back to a flat hour
  // if the clock isn't set yet.
  uint64_t sleepTime = timing::ONE_HOUR_US;
  time_t nowUtc = time(nullptr);
  if (nowUtc > kClockValidAfter) {
    long localNow = static_cast<long>(nowUtc) + utcOffsetSeconds;
    long sod = ((localNow % 86400) + 86400) % 86400;  // seconds into local day
    int curHour = static_cast<int>(sod / 3600);
    long intoHour = sod % 3600;
    // Walk forward to the next allowed hour and wake at its :00. (No "skip the
    // almost-here boundary" shortcut: it could skip the fetch hour if we woke a
    // few seconds early, and wakes already target :00 so it rarely applied.)
    int hoursAhead = 1;
    for (int k = 1; k <= 24; k++) {
      if (wakeAllowed((curHour + k) % 24, quietStart, quietEnd, fetchHour)) {
        hoursAhead = k;
        break;
      }
    }
    long secsToTarget = static_cast<long>(hoursAhead) * 3600 - intoHour;
    if (secsToTarget < 1) secsToTarget = 1;
    sleepTime = static_cast<uint64_t>(secsToTarget) * timing::ONE_SECOND_US;
  }

  esp_sleep_enable_timer_wakeup(sleepTime);
  esp_deep_sleep_start();
}

[[noreturn]] void SleepManager::deepSleepRetry(Display& display, Settings& settings) {
  settings.end();
  display.hibernate();
  SPI.end();
  enableWakeSources();

  // Exponential backoff: 1h, 2h, 4h, ... capped at the daily refresh interval.
  // Shifting by the (pre-increment) failure count keeps a brief outage snappy
  // while a long one stops hammering the WiFi radio every hour. Cap the shift to
  // avoid undefined behaviour on a very large counter.
  uint32_t shift = consecutiveFailures < 24 ? consecutiveFailures : 24;
  uint64_t retryTime = timing::RETRY_INTERVAL_US << shift;
  if (retryTime > timing::MAX_RETRY_INTERVAL_US || retryTime < timing::RETRY_INTERVAL_US)
    retryTime = timing::MAX_RETRY_INTERVAL_US;
  if (consecutiveFailures < UINT32_MAX) consecutiveFailures++;

  esp_sleep_enable_timer_wakeup(retryTime);
  esp_deep_sleep_start();
}

void SleepManager::setWeatherOnScreen(bool onScreen) { screenShowsWeather = onScreen; }

bool SleepManager::weatherOnScreen() { return screenShowsWeather; }

WakeReason SleepManager::checkWakeReason(Display& display, Settings& settings) {
  // A held setup button takes priority over the wake cause, so the user can
  // force the configuration portal from any boot or wake (hold it during a
  // reset, or while pressing the wake button).
  if (setupButtonHeld()) {
    LOGLN("Setup button held -> entering setup");
    settings.setValid(false);
    return WakeReason::EnterSettings;
  }

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
      LOGLN("Wake up from timer.");
      return WakeReason::TimedRefresh;

    case ESP_SLEEP_WAKEUP_EXT1:
      LOGLN("Wake from button (ext1).");
      return WakeReason::RefreshNow;

    case ESP_SLEEP_WAKEUP_GPIO:
    case ESP_SLEEP_WAKEUP_ULP:
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
    case ESP_SLEEP_WAKEUP_UART:
      LOGLN("Wakeup caused by peripheral");
      return WakeReason::RefreshNow;

    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      display.clearFullScreen();
      screenShowsWeather = false;  // panel was just wiped white
      LOGLN("Wake from RESET or other");
      memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);
      return WakeReason::HardReset;
  }
}
