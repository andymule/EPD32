
#define NO_METRIC 1	// toggle use of metric here for now TODO make this web option
// TODO consider clock reset from pin, make sure timer makes sense (doesn't currently)
// TODO remove FREERTOS thing and try swapping cores, does it save time?
// TODO detect wrong password? explicitly 
// TODO have way to update firmware?
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <gfxfont.h>
/* include any other fonts you want to use https://github.com/adafruit/Adafruit-GFX-Library */
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/FreeMonoBold12pt7b.h>
// TODO make your own fonts here: http://oleddisplay.squix.ch/
//#include "Icon2.h"
#include <GxEPD.h>
#include <GxGDEH029A1/GxGDEH029A1.h>
#include <GxIO/GxIO.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiAP.h>
//#include <WiFiServer.h>
//#include <ESP8266WebServer.h>
#include <FS.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
//#include <ESPAsyncWebServer.h>
//#include <AsyncTCP.h>
//#include <HTTP_Method.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "esp32/ulp.h"
#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h>

#include <time.h>
#include <sys/time.h>
#include <TimeLib.h>
#include <rom/rtc.h>

#include "atmo_types.h"
#include "atmo_gfx.h"
#include "atmo_sleep.h"
#include "atmo_wifi.h"
#include "atmo_webserver.h"
#include "atmo_webrequests.h"


//#if CONFIG_FREERTOS_UNICORE
//#define ARDUINO_RUNNING_CORE 0
//#define BACKGROUND_CORE 0
//#else
//#define ARDUINO_RUNNING_CORE 1
//#define BACKGROUND_CORE 0
//#endif

#define MAIN_CORE 1
#define BACKGROUND_CORE 0

int wifisection, displaysection;
unsigned long lastConnectionTime = 0;          // Last time you connected to the server, in milliseconds

#define pp Serial.println

// TODO andymule FOR RELEASE BUILD Disabling all logging and holding the UART disable pin high only increases boot time by around 20 ms?
void setup() {
	wifisection = millis();

	//struct timeval wakeTime;
	//gettimeofday(&wakeTime, NULL);
	//int sleep_time_ms = (wakeTime.tv_sec - sleep_enter_time.tv_sec) * 1000 + (wakeTime.tv_usec - sleep_enter_time.tv_usec) / 1000;
	Serial.begin(115200);
	verbose_print_reset_reason(rtc_get_reset_reason(1));

	//Serial.print("Time spent in deep sleep : ");
	//Serial.print(sleep_time_ms);
	//Serial.println(" ms");

	gfx.init();
	gfx.setRotation(3);
	prefs.begin("settings");

	WakeReason reason = CheckResetReason();

	savedSettings.valid = prefs.getBool("valid");
	if (savedSettings.valid)
	{
		Serial.println("loaded saved lat and lon");
		ReloadSavedSettings();
	}

	if (reason==WakeReason::EnterSettings || savedSettings.wifi_ssid == "")
	{
		DrawConnectionInstructions();
		HostWebsiteForInit();
		return;
	}

	xTaskCreatePinnedToCore(StartWiFi, "StartWiFi", 2048, 0, 1, WiFiTask, BACKGROUND_CORE); // start wifi on other core // TODO Move earlier but dont start if i host the server

	DrawUpdating();

	lastConnectionTime = millis();
	if (!savedSettings.valid)
	{
		GetGeolocationFromNet();
	}
	int weatherHttpCode = RequestWeatherData();
	StopWiFi(); // stop wifi and reduces power consumption
	if (weatherHttpCode == 200)
	{
		ParseWeatherAndTime(); // Remove this passed variable?
	}
	else if (weatherHttpCode != 200) {
		DrawFailedToConnectToSite();
		DeepSleep();
	}
	wifisection = millis() - wifisection;
	displaysection = millis();
	if (weatherHttpCode == 200) {
		DrawDisplay();
	}
	displaysection = millis() - displaysection;
	Serial.println("Wifi took:	 " + String(wifisection / 1000.0) + " secs");
	Serial.println("Display took:" + String(displaysection / 1000.0) + " secs");
	DeepSleep();
}
