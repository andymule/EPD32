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
  // Determines why the device woke. Clears the panel on a cold/hard reset and
  // marks settings invalid when the dedicated setup pad was touched.
  WakeReason checkWakeReason(Display& display, Settings& settings);

  // Sets the system clock from a UTC epoch and records drift vs. the last sleep.
  void setClockAndCompensate(time_t utcEpoch);

  // Powers down peripherals and sleeps until the next refresh (timer + touch).
  // Never returns: the device resumes from setup() on the next wake.
  [[noreturn]] void deepSleep(Display& display, Settings& settings);

  // Like deepSleep but uses the shorter retry interval after a failed update.
  // Drift compensation is reset since we have no fresh server time.
  [[noreturn]] void deepSleepRetry(Display& display, Settings& settings);

  // Tracks whether the panel currently shows valid weather, so a later failure
  // can preserve it instead of replacing it with an error screen. Survives sleep.
  void setWeatherOnScreen(bool onScreen);
  bool weatherOnScreen();

 private:
  // Arms the deep-sleep wake sources (the physical wake button via ext1).
  void enableWakeSources();
  // True if the setup button is held (debounced).
  bool setupButtonHeld();
};
