#pragma once
#define SITE_TIMEOUT_MS 5000 // timeout for site request
#define WIFI_DELAY_CHECK_TIME_MS 50
#define WIFI_TIMEOUT_MS 10000 // 10 sec to connect to wifi?

TaskHandle_t* WiFiTask;

void StartWiFi(void *args) {
	int connAttempts = 0;
	WiFi.mode(WIFI_STA);
	WiFi.begin(savedSettings.wifi_ssid.c_str(), savedSettings.wifi_password.c_str());
	while (WiFi.status() != WL_CONNECTED) {
		delay(WIFI_DELAY_CHECK_TIME_MS); //Serial.print(F("."));
		if (connAttempts > WIFI_TIMEOUT_MS / WIFI_DELAY_CHECK_TIME_MS) {
			prefs.putBool("valid", false); //invalidate location data // TODO indicate this on display
			DrawFailedToConnectToWiFi();
			DeepSleep();
		}
		connAttempts++;
	}
	vTaskDelete(NULL); // deletes self
}

void StopWiFi() {
	WiFi.mode(WIFI_OFF);
}

void EnsureWiFiIsStarted()
{
	int counter = 0;
	while (!WiFi.isConnected())
	{
		if (counter % 10 == 0)
		{
			counter = 0;
			Serial.println(".");
		}
		counter++;
		delay(10);
	}
}