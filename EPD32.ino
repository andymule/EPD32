
// TODO consider clock reset from pin, make sure timer makes sense (doesn't currently)
// TODO remove FREERTOS thing and try swapping cores, does it save time?
// TODO detect wrong password? explicitly 
// TODO have way to update firmware?
// TODO verify webpage input e.g. empty wifi name
// TODO ULP bitbang drive SPI to update screen in ULP???
// TODO display current detected location in webserver, allow custom location setting
// TODO test on Open WiFi... test on very secure wifi?
// TODO i wish GXEPD didn't clear buffer to black, can i swap?
// TODO sometimes i compile to 441kB, sometimes i compile to 950kB???
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <gfxfont.h>
/* include any other fonts you want to use https://github.com/adafruit/Adafruit-GFX-Library */
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
//#include <Fonts/FreeMonoBold12pt7b.h>
// TODO make your own fonts here: http://oleddisplay.squix.ch/
//#include "Icon2.h"
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

#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h>

#include <Preferences.h>
#include "esp32/ulp.h"
#include <time.h>
#include <sys/time.h>
#include <TimeLib.h>
#include <rom/rtc.h>

#define pp Serial.println

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
unsigned long lastConnectionTime = 0;          // Last time you connected to the server, in milliseconds


// TODO andymule FOR RELEASE BUILD Disabling all logging and holding the UART disable pin high only increases boot time by around 20 ms?
void setup() {
	wifisection = millis();
	

	//struct timeval wakeTime;
	//gettimeofday(&wakeTime, NULL);
	//int sleep_time_ms = (wakeTime.tv_sec - sleep_enter_time.tv_sec) * 1000 + (wakeTime.tv_usec - sleep_enter_time.tv_usec) / 1000;
	Serial.begin(115200);
	//verbose_print_reset_reason(rtc_get_reset_reason(1));

	//Serial.print("Time spent in deep sleep : ");
	//Serial.print(sleep_time_ms);
	//Serial.println(" ms");

	gfx.init();
	gfx.setRotation(3);
	gfx.firstPage();
	MakePatterns();	// fill our gfx patterns into RAM

	while (true) // andy GFX demo
	{
		gfx.setRotation(3);
		//DrawSpecksUnderSpline();
		gfx.fillScreen(GxEPD_WHITE);
		DrawPattern(PatternSparseDots, gfx.height()/4*3);
		delay(2000);
		DrawPatternUnderSpline(PatternZigZagH);
		AtmoDeepSleep();
		delay(2000);
		//DrawSplines();
		//delay(2000);
		//DrawSpecks();
		//delay(2000);
		//DrawSpecksUnderSpline();
		//delay(2000);
		//DrawFont(font12);
		//delay(2000);
		//DrawFont(font9);
		//delay(2000);
	}
	//AtmoDeepSleep();
	//DrawLines();
	//gfx.drawPicture(Icon1, sizeof(Icon1));
	//gfx.drawBitmap(gImage_Icon2, sizeof(gImage_Icon2), gfx.bm_default);
	
	prefs.begin("settings");

	WakeReason reason = CheckResetReasonAndClearScreenIfNeeded();
	if (reason==WakeReason::EnterSettings || prefs.getString(PREF_SSID_STRING) == "")
	{
		DrawConnectionInstructions();
		HostWebsiteForInit();
		return;	// go straight to webserver setup loop, skip rest of init
	}
	
	xTaskCreatePinnedToCore(StartWiFi, "StartWiFi", 2048, 0, 1, WiFiTask, BACKGROUND_CORE); // start wifi on other core // TODO Move earlier but dont start if i host the server

	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	DrawUpdating();
	
	lastConnectionTime = millis();
	if (!prefs.getBool(PREF_VALID_BOOL))	// if cache is invalidated, lets get our location again
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
		AtmoDeepSleep();
	}
	wifisection = millis() - wifisection;
	displaysection = millis();
	if (weatherHttpCode == 200) {
		DrawWeather();
	}
	displaysection = millis() - displaysection;
	Serial.println("Wifi took:	 " + String(wifisection / 1000.0) + " secs");
	Serial.println("Display took:" + String(displaysection / 1000.0) + " secs");
	AtmoDeepSleep();
}
