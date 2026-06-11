#include "power/SleepManager.h"

#include <SPI.h>
#include <TimeLib.h>
#include <esp_sleep.h>
#include <string.h>

#include "Config.h"
#include "Log.h"
#include "display/Display.h"
#include "esp32/ulp.h"
#include "settings/Settings.h"

// Drift-compensation state, preserved across deep sleep in RTC memory.
RTC_DATA_ATTR static bool sleepDriftWasTooFast = false;
RTC_DATA_ATTR static uint64_t sleepDriftCompensation = 0;
RTC_DATA_ATTR static time_t lastServerTime = 0;
RTC_DATA_ATTR static uint64_t lastSleepDuration = 0;

// Number of consecutive failed updates, used to back the retry interval off
// exponentially. Survives deep sleep; reset to zero after any successful update.
RTC_DATA_ATTR static uint32_t consecutiveFailures = 0;

// True while the e-paper is showing a good weather frame (survives deep sleep).
RTC_DATA_ATTR static bool screenShowsWeather = false;

void SleepManager::setClockAndCompensate(time_t utcEpoch) {
  setTime(utcEpoch);

  // Drift = actual elapsed time vs. the duration we intended to sleep for.
  const time_t JAN_20_2020 = 1579461559;
  if (lastServerTime > JAN_20_2020 && lastSleepDuration > 0) {
    int64_t actualElapsedSec = static_cast<int64_t>(utcEpoch - lastServerTime);
    int64_t intendedElapsedSec =
        static_cast<int64_t>(lastSleepDuration / timing::ONE_SECOND_US);
    int64_t driftSeconds = actualElapsedSec - intendedElapsedSec;
    sleepDriftWasTooFast = (driftSeconds < 0);
    if (sleepDriftWasTooFast) driftSeconds *= -1;
    sleepDriftCompensation = static_cast<uint64_t>(driftSeconds) * timing::ONE_SECOND_US;
  } else {
    // No usable baseline (first run, or after a retry reset). Clear any prior
    // compensation so the next full sleep isn't skewed by a stale value.
    sleepDriftWasTooFast = false;
    sleepDriftCompensation = 0;
  }
  lastServerTime = utcEpoch;
}

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

[[noreturn]] void SleepManager::deepSleep(Display& display, Settings& settings) {
  settings.end();
  display.hibernate();
  SPI.end();

  // A full refresh reached this point, so the device is healthy again: clear the
  // failure streak that drives the retry backoff.
  consecutiveFailures = 0;

  enableWakeSources();

  uint64_t sleepTime;
  if (sleepDriftWasTooFast)
    sleepTime = timing::REFRESH_INTERVAL_US + sleepDriftCompensation;
  else
    sleepTime = timing::REFRESH_INTERVAL_US - sleepDriftCompensation;

  lastSleepDuration = sleepTime;
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

  // No fresh server time on a failure, so reset the drift baseline rather than
  // computing bogus drift across the (short) retry sleep on the next success.
  lastServerTime = 0;
  lastSleepDuration = retryTime;
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
