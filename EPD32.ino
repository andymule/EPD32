/*
    Pins 1 and 3 are REPL UART TX and RX respectively
    Pins 6, 7, 8, 11, 16, and 17 are used for connecting the embedded flash, and are not recommended for other uses
    Pins 34-39 are input only, and also do not have internal pull-up resistors
    The pull value of some pins can be set to Pin.PULL_HOLD to reduce power consumption during deepsleep.
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <gfxfont.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#define ESP32
#include <GxEPD2_BW.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiAP.h>

#include <FS.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <HTTPClient.h>

#include <ArduinoJson.h>

#include <Preferences.h>
#include "esp32/ulp.h"
#include <time.h>
#include <sys/time.h>
#include <TimeLib.h>
#include <rom/rtc.h>

#define DEBUG_BUILD
#ifdef DEBUG_BUILD
	#define pp(...) Serial.println(__VA_ARGS__)
#else
	#define pp(...) ((void)0)
#endif

#include "atmo_types.h"
#include "atmo_spline.h"
#include "atmo_gfx.h"
#include "atmo_sleep.h"
#include "atmo_wifi.h"
#include "atmo_webserver.h"
#include "atmo_webrequests.h"

#define MAIN_CORE 1
#define BACKGROUND_CORE 0
#include "Icon2.h"

int wifisection, displaysection;

void setup() {
	wifisection = millis();
#ifdef DEBUG_BUILD
	Serial.begin(115200);
#endif

	SPI.begin(18, 2, 23, SS);
#ifdef DEBUG_BUILD
	gfx.init(115200, false);
#else
	gfx.init(0, false);
#endif
	gfx.setRotation(3);
	Prefs.begin("settings");

	WakeReason reason = CheckResetReasonAndClearScreenIfNeeded();
	cachedSSID = Prefs.getString(PREF_SSID_STRING);
	cachedPassword = Prefs.getString(PREF_PASSWORD_STRING);
	if (reason == WakeReason::EnterSettings || cachedSSID.length() == 0)
	{
		pp("Entering setup, reason: " + String(reason));
		pp("Saved ssid: " + cachedSSID);
		DrawConnectionInstructions();
		HostWebsiteForInit();
		return;
	}

	wifiConnectedSemaphore = xSemaphoreCreateBinary();
	xTaskCreatePinnedToCore(StartWiFi, "StartWiFi", 2048, 0, 1, &WiFiTask, BACKGROUND_CORE);
	DrawUpdating();

	if (!Prefs.getBool(PREF_VALID_BOOL))
	{
		GetGeolocationFromNet();
	}

	int weatherHttpCode = RequestWeatherData();
	StopWiFi();

	if (weatherHttpCode == 200)
	{
		ParseWeatherAndTime();
	}
	else
	{
		DrawFailedToConnectToSite();
		AtmoDeepSleep();
	}

	wifisection = millis() - wifisection;
	displaysection = millis();
	DrawWeather();
	displaysection = millis() - displaysection;

	pp("Wifi took:    " + String(wifisection / 1000.0) + " secs");
	pp("Display took: " + String(displaysection / 1000.0) + " secs");
	AtmoDeepSleep();
}

void loop() {
	dnsServer.processNextRequest();
	server.handleClient();
}
