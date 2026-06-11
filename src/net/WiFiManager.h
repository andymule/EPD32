#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Connects to WiFi on a background-core task so the display can render an
// "updating" frame in parallel. Unlike the original, the task only signals the
// outcome; the caller decides how to handle a failure.
class WiFiManager {
 public:
  void beginAsync(const String& ssid, const String& password);

  // Blocks until the connection attempt resolves; true if associated.
  bool ensureConnected();

  void stop();

 private:
  static void connectTask(void* arg);

  TaskHandle_t task_ = nullptr;
  SemaphoreHandle_t resolvedSem_ = nullptr;
  String ssid_;
  String password_;
  volatile bool connected_ = false;
};
