#pragma once
#define SITE_TIMEOUT_MS 5000
#define WIFI_DELAY_CHECK_TIME_MS 50
#define WIFI_TIMEOUT_MS 10000

TaskHandle_t WiFiTask;
SemaphoreHandle_t wifiConnectedSemaphore;
String cachedSSID;
String cachedPassword;

void StartWiFi(void *args) {
	pp(xPortGetCoreID());
	int connAttempts = 0;
	WiFi.mode(WIFI_STA);
	WiFi.begin(cachedSSID.c_str(), cachedPassword.c_str());
	while (WiFi.status() != WL_CONNECTED) {
		delay(WIFI_DELAY_CHECK_TIME_MS);
		if (connAttempts > WIFI_TIMEOUT_MS / WIFI_DELAY_CHECK_TIME_MS) {
			Prefs.putBool(PREF_VALID_BOOL, false);
			DrawFailedToConnectToWiFi();
			AtmoDeepSleep();
		}
		connAttempts++;
	}
	xSemaphoreGive(wifiConnectedSemaphore);
	vTaskDelete(NULL);
}

void StopWiFi() {
	WiFi.mode(WIFI_OFF);
}

void EnsureWiFiIsStarted()
{
	if (WiFi.isConnected()) return;
	if (xSemaphoreTake(wifiConnectedSemaphore, pdMS_TO_TICKS(WIFI_TIMEOUT_MS + 5000)) != pdTRUE)
	{
		DrawFailedToConnectToWiFi();
		AtmoDeepSleep();
	}
}
