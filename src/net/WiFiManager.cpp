#include "net/WiFiManager.h"

#include <WiFi.h>

#include "Config.h"
#include "Log.h"

void WiFiManager::beginAsync(const String& ssid, const String& password) {
  ssid_ = ssid;
  password_ = password;
  connected_ = false;
  if (resolvedSem_ == nullptr) resolvedSem_ = xSemaphoreCreateBinary();

  // Pin to core 0 so association runs alongside the display work on core 1.
  xTaskCreatePinnedToCore(connectTask, "wifi_connect", 4096, this, 1, &task_, 0);
}

void WiFiManager::connectTask(void* arg) {
  WiFiManager* self = static_cast<WiFiManager*>(arg);
  LOGLN(xPortGetCoreID());

  WiFi.mode(WIFI_STA);
  WiFi.begin(self->ssid_.c_str(), self->password_.c_str());

  const uint32_t maxAttempts =
      timing::WIFI_TIMEOUT_MS / timing::WIFI_POLL_INTERVAL_MS;
  uint32_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(timing::WIFI_POLL_INTERVAL_MS);
    if (attempts++ > maxAttempts) {
      self->connected_ = false;
      self->task_ = nullptr;
      if (self->resolvedSem_) xSemaphoreGive(self->resolvedSem_);
      vTaskDelete(NULL);
      return;
    }
  }

  self->connected_ = true;
  self->task_ = nullptr;
  if (self->resolvedSem_) xSemaphoreGive(self->resolvedSem_);
  vTaskDelete(NULL);
}

bool WiFiManager::ensureConnected() {
  if (WiFi.isConnected()) return true;
  if (resolvedSem_ == nullptr) return false;

  // Wait slightly longer than the task's own timeout so we never race it.
  TickType_t wait = pdMS_TO_TICKS(timing::WIFI_TIMEOUT_MS + 5000);
  if (xSemaphoreTake(resolvedSem_, wait) != pdTRUE) return false;
  return connected_;
}

void WiFiManager::stop() { WiFi.mode(WIFI_OFF); }
