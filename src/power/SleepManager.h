#pragma once
#include <Arduino.h>
#include <time.h>

class Display;
class Settings;

enum class WakeReason { HardReset, TimedRefresh, EnterSettings, RefreshNow };

// Owns deep-sleep, wake-reason detection, and the clock-drift compensation that
// keeps the once-a-day refresh aligned to real time. Drift state lives in RTC
// memory so it survives deep sleep.
class SleepManager {
 public:
  // Latches the battery power rail on (and keeps it latched through deep sleep)
  // for boards with a soft power switch. Must be called early in setup() so the
  // board stays powered after the PWR button is released when on battery. No-op
  // on boards without a latch (e.g. the T5).
  void initBoardPower();

  // Determines why the device woke. Clears the panel on a cold/hard reset and
  // marks settings invalid when the dedicated setup pad was touched. May not
  // return if the PWR button requested a power-off.
  WakeReason checkWakeReason(Display& display, Settings& settings);

  // Sets the system (RTC) clock from a UTC epoch. The ESP32 RTC keeps running
  // through deep sleep, so the time stays correct across hourly cached redraws.
  void setClock(time_t utcEpoch);

  // True once the clock has been set to a real time (survives deep sleep).
  bool clockValid();

  // Current UTC epoch from the RTC (0/garbage until setClock has run).
  time_t currentUtc();

  // Powers down peripherals and sleeps until the top of the next hour (so the
  // clock on screen is redrawn each hour). utcOffsetSeconds aligns the wake to
  // local time. Never returns: the device resumes from setup() on the next wake.
  [[noreturn]] void deepSleep(Display& display, Settings& settings,
                              long utcOffsetSeconds);

  // Like deepSleep but uses the shorter retry interval after a failed update.
  // Drift compensation is reset since we have no fresh server time.
  [[noreturn]] void deepSleepRetry(Display& display, Settings& settings);

  // Tracks whether the panel currently shows valid weather, so a later failure
  // can preserve it instead of replacing it with an error screen. Survives sleep.
  void setWeatherOnScreen(bool onScreen);
  bool weatherOnScreen();

  // True while a previous update failed and a retry is still owed (the failure
  // streak that drives the backoff is non-zero). Lets the next wake force a fetch
  // even if it isn't the scheduled daily hour, so a failed daily fetch recovers
  // on the backoff schedule instead of waiting a full day. Survives deep sleep.
  bool fetchRetryPending();

#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
  // Unlatches the battery rail and sleeps (PWR-button power-off). On battery the
  // board powers off; on USB it deep-sleeps until a button press. Never returns.
  [[noreturn]] void powerOff();
#endif

 private:
  // Arms the deep-sleep wake sources (the physical wake button via ext1).
  void enableWakeSources();
  // True if the setup button is held (debounced).
  bool setupButtonHeld();
};
