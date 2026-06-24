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
#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
#include <driver/gpio.h>  // gpio_hold_en / gpio_deep_sleep_hold_en
#else
#include "esp32/ulp.h"  // classic-ESP32-only header path
#endif
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

void SleepManager::initBoardPower() {
#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
  // Latch the battery rail on and hold that level through deep sleep, so the
  // board stays powered once the PWR button is released (on battery) and across
  // every wake. On USB this is harmless (the board is fed from VSYS regardless).
  // The held value (HIGH) equals the desired value, so on a deep-sleep wake the
  // rail is already latched and this simply re-affirms it.
  pinMode(pins::VBAT_PWR, OUTPUT);
  digitalWrite(pins::VBAT_PWR, HIGH);
  gpio_hold_en(static_cast<gpio_num_t>(pins::VBAT_PWR));
  gpio_deep_sleep_hold_en();
#endif
}

void SleepManager::enableWakeSources() {
  // Wake button (active-low) via ext1. ext1 is chosen over ext0/touch because it
  // lets the RTC peripheral power domain switch off during deep sleep (lowest
  // sleep current); the board's external pull-up holds the line high until the
  // button is pressed and pulls it low.
#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
  // BOOT (refresh) or PWR (power off) can wake; ANY_LOW since either may fire.
  const uint64_t mask =
      (1ULL << pins::WAKE_BUTTON) | (1ULL << pins::PWR_BUTTON);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
#else
  esp_sleep_enable_ext1_wakeup(1ULL << pins::WAKE_BUTTON, ESP_EXT1_WAKEUP_ALL_LOW);
#endif
}

#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
[[noreturn]] void SleepManager::powerOff() {
  // PWR button: unlatch the battery rail. On battery the board powers off the
  // instant the latch opens; on USB (still fed from VSYS) we fall through to a
  // deep sleep that only a button press can end, which is the closest to "off".
  gpio_hold_dis(static_cast<gpio_num_t>(pins::VBAT_PWR));
  pinMode(pins::VBAT_PWR, OUTPUT);
  digitalWrite(pins::VBAT_PWR, LOW);
  esp_sleep_enable_ext1_wakeup(
      (1ULL << pins::WAKE_BUTTON) | (1ULL << pins::PWR_BUTTON),
      ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}
#endif

bool SleepManager::setupButtonHeld() {
#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
  // No dedicated setup button on this board: a *long press* of the top/refresh
  // button (BOOT/GPIO0) opens the setup portal. A quick tap wakes for an
  // immediate refresh; holding it for kSetupHoldMs opens the at.mo AP to change
  // settings -- and since setup mode stays awake, that's also the moment to
  // reflash over USB without entering ROM download mode. Returns as soon as the
  // button is released (fast path for a normal refresh tap).
  constexpr uint32_t kSetupHoldMs = 3000;
  pinMode(pins::WAKE_BUTTON, INPUT_PULLUP);  // BOOT also has its own pull-up
  if (digitalRead(pins::WAKE_BUTTON) != LOW) return false;  // not held -> refresh
  const uint32_t start = millis();
  while (digitalRead(pins::WAKE_BUTTON) == LOW) {
    if (millis() - start >= kSetupHoldMs) {
      LOGLN("Top button long-press -> setup portal.");
      return true;
    }
    delay(10);
  }
  return false;  // released before the hold threshold -> normal refresh
#else
  // LilyGo T5: dedicated setup button, active-low, debounced so a transient
  // can't drop the device into the portal.
  pinMode(pins::SETUP_BUTTON, INPUT);
  if (digitalRead(pins::SETUP_BUTTON) != LOW) return false;
  delay(20);
  return digitalRead(pins::SETUP_BUTTON) == LOW;
#endif
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

bool SleepManager::fetchRetryPending() { return consecutiveFailures > 0; }

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
#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
      if (esp_sleep_get_ext1_wakeup_status() & (1ULL << pins::PWR_BUTTON)) {
        LOGLN("PWR button -> power off.");
        powerOff();  // unlatches battery rail; does not return
      }
#endif
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
#if defined(CONFIG_ULP_COPROC_RESERVE_MEM) && CONFIG_ULP_COPROC_RESERVE_MEM > 0
      memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);
#endif
      return WakeReason::HardReset;
  }
}
