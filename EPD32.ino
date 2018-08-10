#include <gfxfont.h>
#include <Adafruit_SPITFT_Macros.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_GFX.h>
#include <GxEPD.h>
#include <GxGDEH029A1/GxGDEH029A1.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
/* include any other fonts you want to use https://github.com/adafruit/Adafruit-GFX-Library */
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/FreeMonoBold12pt7b.h>
// make your own fonts here: http://oleddisplay.squix.ch/
#include <ArduinoJson.h>
#include <SPI.h>

// this code is for ESP32, but should work just as well on ESP8266
#if defined(ESP8266)
#include <ESP8266WiFi.h>          
#else
#include <WiFi.h>          
#endif

#include <DNSServer.h>
#if defined(ESP8266)
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#else
#include <WebServer.h>
#include <ESP32httpUpdate.h>
#endif

#include <HTTPClient.h>
#include <WiFiManager.h> 
#include <WiFiClient.h>

#include "time.h"
#include <rom/rtc.h>

#include <sys/time.h>
#include "esp32/ulp.h"
//#include "esp_deep_sleep.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <sstream>
#include <string>

const char* ssid = "ohm";
const char* password = "shesthebest";

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16);	// arbitrary selection of 17, 16
GxEPD_Class gfx(io, /*RST=*/ 16, /*BUSY=*/ 4);				 // arbitrary selection of (16), 4

unsigned long lastConnectionTime = 0;          // Last time you connected to the server, in milliseconds
// 1000000LL is one second

const uint64_t OneMinute = 60000000LL;	/*6000000 uS = 1 minute*/
const uint64_t MinutesInAnHour = 60LL;		/*60 min = 1 hour*/
// TODO andymule this number weirdly lets me wake up every 6 hours or so ? This hasn't proven true and needs more work
const uint64_t SleepTimeMicroseconds = OneMinute * MinutesInAnHour * 2LL;

/*
NOTE:
======
Only RTC IO can be used as a source for external wake
source. They are pins: 0,2,4,12-15,25-27,32-39.
*/
// TODO wake oncaptouch, possible gestures?
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/DeepSleep/ExternalWakeUp/ExternalWakeUp.ino
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/DeepSleep/TouchWakeUp/TouchWakeUp.ino

/*
//https://github.com/Serpent999/ESP32_Touch_LED/blob/master/Touch_LED/Touch_LED.ino //WEIRD example of touch? might be useful
Touch Sensor Pin Layout
T0 = GPIO4
T1 = GPIO0
T2 = GPIO2
T3 = GPIO15
T4 = GPIO13
T5 = GPIO12
T6 = GPIO14
T7 = GPIO27
T8 = GPIO33
T9 = GPIO32 */

// RTC_DATA_ATTR marks variables to be saved across sleep
static RTC_DATA_ATTR struct timeval sleep_enter_time;

// here is original string
// https://query.yahooapis.com/v1/public/yql?q=select%20*%20from%20weather.forecast%20where%20woeid%20in%20(select%20woeid%20from%20geo.places(1)%20where%20text%3D%22seattle%2C%20wa%22)&format=json&env=store%3A%2F%2Fdatatables.org%2Falltableswithkeys

const String yahoostring1 = "https://query.yahooapis.com/v1/public/yql?q=select%20*%20from%20weather.forecast%20where%20woeid%20in%20(select%20woeid%20from%20geo.places(1)%20where%20text%3D%22";
//String yahoostringCity;
const String yahoostring2 = "%2C%20";
//String yahoostringState;
const String yahoostring3 = "%22)&format=json&env=store%3A%2F%2Fdatatables.org%2Falltableswithkeys";

const String geolocatestring = "http://api.ipstack.com/check?access_key=d0dfe9b52fa3f5bb2a5ff47ce435c7d8&format=1";

HTTPClient yahoohttp;
HTTPClient geolocatehttp;
WiFiClient client; // wifi client object

class Geolocate
{
public:
	String ip;
	String city;
	String region_code;
	float lat, lon;
	int geoname_id;	//www.geonames.org/5809844  TODO use GEOID for stuff?? Make own GeoID lookup service? jeez
};
Geolocate geolocate;

const char* city;
const char* weathertime;
int    wifisection, displaysection;
int Sunrise, Sunset;

class WeatherDay
{
	//float    Temperature;
	//String   Icon;
public:
	int High;
	int Low;
	const char* Text;
	const char* Date;
	const char* DayOfWeek;
};

WeatherDay WeatherDays[10] = {};

//const int width = 296;
//const int height = 128;

// TODO andymule ON RELEASE Disabling all logging but keeping the UART disable pin high only increased boot time by around 20 ms.
void setup() {
	Serial.begin(128000);

	struct timeval now;
	gettimeofday(&now, NULL);
	int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;

	uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();
	if (wakeupBit & GPIO_SEL_33) {
		// GPIO 33 woke up
	}
	else if (wakeupBit & GPIO_SEL_34) {
		// GPIO 34
	}

	// Определение, по какому событию проснулись
	switch (esp_sleep_get_wakeup_cause()) {
	case ESP_SLEEP_WAKEUP_TIMER: {
		//Serial.println();
		Serial.print("Wake up from timer. Time spent in deep sleep: ");
		Serial.print(sleep_time_ms);
		Serial.println(" ms");
		WiFi.disconnect();
		//WiFi.mode(WIFI_OFF); 
		//esp_wifi_start();	// need to manually power up?? IDK BUG on 2 hour deep sleep
		//esp_wifi_stop();
		//Serial.println();
		break;
	}
	case ESP_SLEEP_WAKEUP_UNDEFINED:
	default: {
		//Serial.println();
		Serial.println("Not a deep sleep reset");
		//Serial.println();
		memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);
	}
	}

	wifisection = millis();
	StartWiFi();
	lastConnectionTime = millis();

	geolocatehttp.begin(geolocatestring); //Specify the URL
	int geoHttpCode = geolocatehttp.GET();

	if (geoHttpCode == 200)
	{//StaticJsonBuffer<6000> jsonBuffer;	// we're told to use this, but it doesn't parse so we're using the deprecated dynamicjsonbuffer
		DynamicJsonBuffer  jsonBuffer2(1200);
		JsonObject& root2 = jsonBuffer2.parseObject(geolocatehttp.getString());
		geolocate.ip			= root2["ip"].asString();
		geolocate.city			= root2["city"].asString();
		geolocate.geoname_id	= root2["geoname_id"];
		geolocate.lat			= root2["latitude"];
		geolocate.lon			= root2["longitude"];
		geolocate.region_code	= root2["region_code"].asString();	//state in USA
		geolocatehttp.end();
	}
	else {
		Serial.println("Error on Geolocate request");
		//weathertime = "HTTP ERROR:" + yahooHttpCode;
		Sleep();
	}
	
	geolocate.city.replace(" ", "%20");
	geolocate.region_code.replace(" ", "%20");

	String yahooReal = yahoostring1 + geolocate.city + yahoostring2 + geolocate.region_code + yahoostring3;
	yahoohttp.begin(yahooReal); //Specify the URL
	int yahooHttpCode = yahoohttp.GET();

	StopWiFi(); // stop wifi and reduces power consumption

	if (yahooHttpCode == 200)
	{//StaticJsonBuffer<6000> jsonBuffer;	// we're told to use this, but it doesn't parse so we're using the deprecated dynamicjsonbuffer
		DynamicJsonBuffer  jsonBuffer(6000);
		JsonObject& root = jsonBuffer.parseObject(yahoohttp.getString());
		city = root["query"]["results"]["channel"]["location"]["city"];
		weathertime = root["query"]["results"]["channel"]["lastBuildDate"];
		//Serial.println(weathertime);
		ParseIntoWeatherObjects(root);
		yahoohttp.end();
	}
	else {
		Serial.println("Error on Weather HTTP request");
		//weathertime = "HTTP ERROR:" + yahooHttpCode;
		Sleep();
	}

	wifisection = millis() - wifisection;
	displaysection = millis();
	
	// TODO andymule add other output when error 
	if (yahooHttpCode == 200) {
		//Received data OK at this point so turn off the WiFi to save power
		//displaysection = millis();
		gfx.init();
		gfx.setRotation(3);
		gfx.setTextColor(GxEPD_BLACK);

		gfx.setCursor(0,0);
		gfx.println(geolocate.city);

		gfx.setCursor(gfx.width() / 4, 2);
		gfx.println(weathertime);

		const GFXfont* font9 = &FreeSans9pt7b;		// TODO andymule use ishac fonts
		const GFXfont* font12 = &FreeSans12pt7b;
		gfx.setFont(font12);
		gfx.setCursor(gfx.width() / 2 - 16, 30);
		//gfx.println(WeatherDays[0].DayOfWeek);

		gfx.setFont(font12);
		gfx.setCursor(8, 59);
		gfx.print(" Low:");
		gfx.print(WeatherDays[0].Low);
		//gfx.println(String("°"));	// TODO andymule draw degrees
		gfx.setCursor(gfx.width() - 89, 59);
		gfx.print("High:");
		gfx.print(WeatherDays[0].High);
		//gfx.println(String("°")); // TODO andymule draw degrees

		gfx.setFont(font9);
		gfx.setCursor(gfx.width() / 2 - 26, 30);
		gfx.println(WeatherDays[0].Text);

		addsun(gfx.width() / 2, 62, 11);	// TODO andymule use bitmap prolly

		DrawDaysAhead(6);

		gfx.update();
	}
	displaysection = millis() - displaysection;
	Serial.println("Wifi took:	 " + String(wifisection		/ 1000.0) + " secs");
	Serial.println("Display took:" + String(displaysection	/ 1000.0) + " secs");
	Sleep();
}

void Sleep()
{
	gfx.powerDown();	// saves power probably?? lololol
	int result = esp_sleep_enable_timer_wakeup(SleepTimeMicroseconds);
	std::ostringstream ss;
	ss << SleepTimeMicroseconds;
	if (result == ESP_ERR_INVALID_ARG)
	{
		Serial.print("FAILED to sleep:");
		Serial.println(ss.str().c_str());
	}
	else
	{
		Serial.print("SUCCESS SLEEPING:");
		Serial.println(ss.str().c_str());
	}
	gettimeofday(&sleep_enter_time, NULL);
	EnableWakeOnTilt();	// here so we always have more current orientation when setting our interupts 
	esp_deep_sleep_start(); // REALLY DEEP sleep and save power
}

void EnableWakeOnTilt()
{
	// TODO andymule detect current orientation and set interupts accordingly
	esp_sleep_enable_ext1_wakeup(GPIO_NUM_33, ESP_EXT1_WAKEUP_ANY_HIGH);
	esp_sleep_enable_ext0_wakeup(GPIO_NUM_34, ESP_EXT1_WAKEUP_ANY_HIGH);
}

void DrawDaysAhead(int daysAhead)
{
	for (int i = 0; i <= daysAhead; i++)
	{
		DrawDayOfWeek(i, (gfx.width() / daysAhead), 90, 9);
	}
}

void DrawDayOfWeek(int daysAfterToday, int width, int heightStart, int fontHeight)
{
	gfx.setFont();	// resets to default little font guy
	gfx.setCursor(width*(daysAfterToday - 1), heightStart);
	gfx.println(WeatherDays[daysAfterToday].DayOfWeek);
	gfx.setCursor(width*(daysAfterToday - 1), heightStart + fontHeight);

	String CheckText = String(WeatherDays[daysAfterToday].Text);
	String PrintText = String(CheckText);
	if (CheckText.indexOf(" ") > 0)
	{
		PrintText = CheckText.substring(CheckText.lastIndexOf(" ") + 1, CheckText.length());
	}
	gfx.println(PrintText);

	gfx.setCursor(width*(daysAfterToday - 1), heightStart + fontHeight * 2);
	gfx.println(WeatherDays[daysAfterToday].High);
	gfx.setCursor(width*(daysAfterToday - 1), heightStart + fontHeight * 3);
	gfx.println(WeatherDays[daysAfterToday].Low);
}

void ParseIntoWeatherObjects(JsonObject& root)
{
	for (int i = 0; i <= 9; i++)
	{
		WeatherDays[i].Date = root["query"]["results"]["channel"]["item"]["forecast"][i]["date"];
		WeatherDays[i].DayOfWeek = root["query"]["results"]["channel"]["item"]["forecast"][i]["day"];
		WeatherDays[i].High = root["query"]["results"]["channel"]["item"]["forecast"][i]["high"];
		WeatherDays[i].Low = root["query"]["results"]["channel"]["item"]["forecast"][i]["low"];
		WeatherDays[i].Text = root["query"]["results"]["channel"]["item"]["forecast"][i]["text"];
		//Serial.println(String(i) + ":" +
		//	String(WeatherDays[i].Date) + " " +
		//	String(WeatherDays[i].DayOfWeek) + " " +
		//	String(WeatherDays[i].High) + " " +
		//	String(WeatherDays[i].Low) + " " +
		//	String(WeatherDays[i].Text));
	}
}

void loop() { // this will never run!
	Serial.println("UH OH IN LOOP!");
	delay(3000);
	yield;
	Sleep();
}

////#########################################################################################
//void DrawPressureTrend(int x, int y, float pressure, String slope) {
//	gfx.drawString(90, 47, String(pressure, 1) + (Units == "M" ? "mb" : "in"));
//	x = x + 8;
//	if (slope == "+") {
//		gfx.drawLine(x, y, x + 4, y - 4);
//		gfx.drawLine(x + 4, y - 4, x + 8, y);
//	}
//	else if (slope == "0") {
//		gfx.drawLine(x + 3, y - 4, x + 8, y);
//		gfx.drawLine(x + 3, y + 4, x + 8, y);
//	}
//	else if (slope == "-") {
//		gfx.drawLine(x, y, x + 4, y + 4);
//		gfx.drawLine(x + 4, y + 4, x + 8, y);
//	}
//}
////#########################################################################################

int StartWiFi() {
	int connAttempts = 0;
	Serial.print(F("\r\nConnecting to: "));
	Serial.println(String(ssid));
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	//WiFi.waitForConnectResult();
	//WiFi.setAutoReconnect(true);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500); Serial.print(F("."));
		if (connAttempts > 20) {	// give it 10 seconds
			Serial.println("WiFi down? Failed to connect. RESTARTING DEVICE?");
			// wifi prolly down, try again later
			ESP.restart();	// TODO is this best?
			Sleep();
		}
		connAttempts++;
	}

	Serial.println("WiFi connected at: " + String(WiFi.localIP()));
	return 1;
}
//#########################################################################################
void StopWiFi() {
	WiFi.disconnect();
	WiFi.mode(WIFI_OFF);
	esp_wifi_stop();
	Serial.println("Wifi Off");
}


void addcloud(int x, int y, int scale, int linesize) {
	//Draw cloud outer
	gfx.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                           // Left most circle
	gfx.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                           // Right most circle
	gfx.fillCircle(x - scale, y - scale, scale*1.4, GxEPD_BLACK);                   // left middle upper circle
	gfx.fillCircle(x + scale * 1.5, y - scale * 1.3, scale*1.75, GxEPD_BLACK);          // Right middle upper circle
	gfx.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK);         // Upper and lower lines
	//Clear cloud inner
	gfx.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);                  // Clear left most circle
	gfx.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);                  // Clear right most circle
	gfx.fillCircle(x - scale, y - scale, scale*1.4 - linesize, GxEPD_WHITE);          // left middle upper circle
	gfx.fillCircle(x + scale * 1.5, y - scale * 1.3, scale*1.75 - linesize, GxEPD_WHITE); // Right middle upper circle
	gfx.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale*5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE); // Upper and lower lines
}
void addrain(int x, int y, int scale) {
	y = y + scale / 2;
	for (int i = 0; i < 6; i++) {
		gfx.drawLine(x - scale * 4 + scale * i*1.3 + 0, y + scale * 1.9, x - scale * 3.5 + scale * i*1.3 + 0, y + scale, GxEPD_BLACK);
		if (scale > 30) {
			gfx.drawLine(x - scale * 4 + scale * i*1.3 + 1, y + scale * 1.9, x - scale * 3.5 + scale * i*1.3 + 1, y + scale, GxEPD_BLACK);
			gfx.drawLine(x - scale * 4 + scale * i*1.3 + 2, y + scale * 1.9, x - scale * 3.5 + scale * i*1.3 + 2, y + scale, GxEPD_BLACK);
		}
	}
}
void addsnow(int x, int y, int scale) {
	int dxo, dyo, dxi, dyi;
	for (int flakes = 0; flakes < 5; flakes++) {
		for (int i = 0; i < 360; i = i + 45) {
			dxo = 0.5*scale * cos((i - 90)*3.14 / 180); dxi = dxo * 0.1;
			dyo = 0.5*scale * sin((i - 90)*3.14 / 180); dyi = dyo * 0.1;
			gfx.drawLine(dxo + x + 0 + flakes * 1.5*scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5*scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
		}
	}
}
void addtstorm(int x, int y, int scale) {
	y = y + scale / 2;
	for (int i = 0; i < 5; i++) {
		gfx.drawLine(x - scale * 4 + scale * i*1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i*1.5 + 0, y + scale, GxEPD_BLACK);
		if (scale > 30) {
			gfx.drawLine(x - scale * 4 + scale * i*1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i*1.5 + 1, y + scale, GxEPD_BLACK);
			gfx.drawLine(x - scale * 4 + scale * i*1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i*1.5 + 2, y + scale, GxEPD_BLACK);
		}
		gfx.drawLine(x - scale * 4 + scale * i*1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i*1.5 + 0, y + scale * 1.5 + 0, GxEPD_BLACK);
		if (scale > 30) {
			gfx.drawLine(x - scale * 4 + scale * i*1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i*1.5 + 0, y + scale * 1.5 + 1, GxEPD_BLACK);
			gfx.drawLine(x - scale * 4 + scale * i*1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i*1.5 + 0, y + scale * 1.5 + 2, GxEPD_BLACK);
		}
		gfx.drawLine(x - scale * 3.5 + scale * i*1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i*1.5 + 0, y + scale * 1.5, GxEPD_BLACK);
		if (scale > 30) {
			gfx.drawLine(x - scale * 3.5 + scale * i*1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i*1.5 + 1, y + scale * 1.5, GxEPD_BLACK);
			gfx.drawLine(x - scale * 3.5 + scale * i*1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i*1.5 + 2, y + scale * 1.5, GxEPD_BLACK);
		}
	}
}
void addsun(int x, int y, int scale) {
	int linesize = 3;
	if (scale > 30) linesize = 1;
	int dxo, dyo, dxi, dyi;
	gfx.fillCircle(x, y, scale, GxEPD_BLACK);
	gfx.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
	for (float i = 0; i < 360; i = i + 45) {
		dxo = 2.2*scale * cos((i - 90)*3.14 / 180); dxi = dxo * 0.6;
		dyo = 2.2*scale * sin((i - 90)*3.14 / 180); dyi = dyo * 0.6;
		if (i == 0 || i == 180) {
			gfx.drawLine(dxo + x - 1, dyo + y, dxi + x - 1, dyi + y, GxEPD_BLACK);
			if (scale > 30) {
				gfx.drawLine(dxo + x + 0, dyo + y, dxi + x + 0, dyi + y, GxEPD_BLACK);
				gfx.drawLine(dxo + x + 1, dyo + y, dxi + x + 1, dyi + y, GxEPD_BLACK);
			}
		}
		if (i == 90 || i == 270) {
			gfx.drawLine(dxo + x, dyo + y - 1, dxi + x, dyi + y - 1, GxEPD_BLACK);
			if (scale > 30) {
				gfx.drawLine(dxo + x, dyo + y + 0, dxi + x, dyi + y + 0, GxEPD_BLACK);
				gfx.drawLine(dxo + x, dyo + y + 1, dxi + x, dyi + y + 1, GxEPD_BLACK);
			}
		}
		if (i == 45 || i == 135 || i == 225 || i == 315) {
			gfx.drawLine(dxo + x - 1, dyo + y, dxi + x - 1, dyi + y, GxEPD_BLACK);
			if (scale > 30) {
				gfx.drawLine(dxo + x + 0, dyo + y, dxi + x + 0, dyi + y, GxEPD_BLACK);
				gfx.drawLine(dxo + x + 1, dyo + y, dxi + x + 1, dyi + y, GxEPD_BLACK);
			}
		}
	}
}