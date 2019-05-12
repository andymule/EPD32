// TODO investigate multicore usage
// TODO remove all String usages? use char* or char[]
// TODO store lat lon after recieved once?
// TODO profile WIFI vs data parse times
#include <SPI.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "esp32/ulp.h"
#define ARDUINOJSON_DECODE_UNICODE 1
#include <ArduinoJson.h>
#include <gfxfont.h>
#include <Adafruit_GFX.h>
#include <GxEPD.h>
#include <GxGDEH029A1/GxGDEH029A1.h>
#include <GxIO/GxIO.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
/* include any other fonts you want to use https://github.com/adafruit/Adafruit-GFX-Library */
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/FreeMonoBold12pt7b.h>
// make your own fonts here: http://oleddisplay.squix.ch/
#include <string>

#include "Icon2.h"

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

const char* ssid = "slow";
const char* password = "bingbangbong";

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16);	// arbitrary selection of 17, 16
GxEPD_Class gfx(io, /*RST=*/ 16, /*BUSY=*/ 4);				 // arbitrary selection of (16), 4

int wifisection, displaysection;
unsigned long lastConnectionTime = 0;          // Last time you connected to the server, in milliseconds
// 1000000LL is one second

const uint64_t OneSecond = 1000000LL;
const uint64_t Time60 = 60LL;		/*60 min = 1 hour*/
const uint64_t OneMinute = OneSecond * Time60;	/*6000000 uS = 1 minute*/
const uint64_t OneHour   = OneMinute * Time60;	/*6000000 uS = 1 minute*/
// TODO andymule this number weirdly lets me wake up every 12 hours or so ? This hasn't proven true and needs more work

const uint64_t SleepTimeMicroseconds = OneHour*24LL;

/*
NOTE:
======
Only RTC IO can be used as a source for external wake
source. They are pins: 0,2,4,12-15,25-27,32-39.
*/
// TODO wake oncaptouch, possible gestures?
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/DeepSleep/ExternalWakeUp/ExternalWakeUp.ino
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/DeepSleep/TouchWakeUp/TouchWakeUp.ino

#define Threshold 100 /* Greater the value, more the sensitivity */
RTC_DATA_ATTR int bootCount = 0;
touch_pad_t touchPin;

/*
//https://github.com/Serpent999/ESP32_Touch_LED/blob/master/Touch_LED/Touch_LED.ino //WEIRD example of touch? might be useful
Touch Sensor Pin Layout
T0 = GPIO4, T1 = GPIO0, T2 = GPIO2, T3 = GPIO15, T4 = GPIO13, T5 = GPIO12, T6 = GPIO14, T7 = GPIO27, T8 = GPIO33, T9 = GPIO32 */

// RTC_DATA_ATTR marks variables to be saved across sleep
static RTC_DATA_ATTR struct timeval sleep_enter_time;	// TODO exploit this for saving preferences??

// original strings:
// http://api.openweathermap.org/data/2.5/weather?&appid=ba42e4a918f7e742d3143c5e8fff9210&lat=59.3307&lon=18.0718&units=metric
//https://weather.api.here.com/weather/1.0/report.json?app_id=JoN1SBsEzJ5pWD5OkXwN&app_code=J9XdgHlHuUKzV2j5GqxlVg&product=forecast_7days_simple&latitude=59.3307&longitude=18.0718

//const String weatherCurrent = "http://api.openweathermap.org/data/2.5/weather?&appid=ba42e4a918f7e742d3143c5e8fff9210&lat=";
const String weatherCurrent = "https://weather.api.here.com/weather/1.0/report.json?app_id=JoN1SBsEzJ5pWD5OkXwN&app_code=J9XdgHlHuUKzV2j5GqxlVg&product=forecast_7days_simple&product=observation&oneobservation=true&latitude=";
const String weatherCurren = "https://weather.api.here.com/weather/1.0/report.json?app_id=JoN1SBsEzJ5pWD5OkXwN&app_code=J9XdgHlHuUKzV2j5GqxlVg&product=forecast_7days_simple&product=observation&oneobservation=true&latitude=59.3307&longitude=18.0718";
const String weatherAndLon = "&longitude=";
const String weatherNoMetric = "&metric=false";	// todo is this autoresolved by here api?
const String weatherLanguage = "&language=";	// https://developer.here.com/documentation/weather/topics/supported-languages.html

const String geolocatestring = "http://api.ipstack.com/check?access_key=d0dfe9b52fa3f5bb2a5ff47ce435c7d8"; //key=ab925796fd105310f825bbdceece059e

HTTPClient weathercurrenthttp, geolocatehttp;
//WiFiClient client; 

class SavedSettings
{
public:
	bool valid;
	//String ip;	// monitor and check for change? 
	String city;
	//String region_code;
	float lat;
	float lon;
	//int geoname_id;	//www.geonames.org/5809844  TODO use GEOID for stuff?? Make own GeoID lookup service? jeez
};
SavedSettings savedSettings;

//String city;
//String CurrentDateTimeString;
//String CurrentDayOfWeek;
//int CurrentTimeInMinutesIfMidnightWereZeroMinutes;
//int Sunrise, Sunset;

String CurrentTime; 
int CurrentTemp;

class WeatherDay
{
public:
	int High;
	int Low;
	const char* SkyText;
	const char* PrecipText;
	const char* UTCTime;
	const char* DayOfWeek;
};

WeatherDay WeatherDays[10] = {};

// useful tools, like bitmap converter, fonts, and font converters
// https://github.com/cesanta/arduino-drivers/tree/master/Adafruit-GFX-Library
#include GxEPD_BitmapExamples
// 

//const int width = 296;
//const int height = 128;
Preferences prefs;


// TODO andymule FOR RELEASE BUILD Disabling all logging and holding the UART disable pin high only increases boot time by around 20 ms?
void setup() {
	struct timeval now;
	gettimeofday(&now, NULL);
	int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;

	Serial.begin(128000);
	prefs.begin("settings");
	gfx.init();

	uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();
	if (wakeupBit & GPIO_SEL_33) {
		// GPIO 33 woke up
	}
	else if (wakeupBit & GPIO_SEL_34) {
		// GPIO 34
	}

	esp_sleep_wakeup_cause_t wakeup_reason;
	wakeup_reason = esp_sleep_get_wakeup_cause();
	switch (wakeup_reason) {
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_TIMER: {
			Serial.print("Wake up from timer. Time spent in deep sleep: ");
			Serial.print(sleep_time_ms);
			Serial.println(" ms");
			break;
		}
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_GPIO:
		{
			Serial.println("Wakeup caused by GPIO"); break;
		}
		case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_UART:
		{
			Serial.println("Wakeup caused by UART??"); break;
		}
		case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_TOUCHPAD:
		{
			prefs.putBool("valid", false); //invalidate location data // TODO indicate this on display
			Serial.println("Wakeup caused by touchpad"); break;
		}
		case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
		case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by EXT0"); break;
		case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by EXT1"); break;
		case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_UNDEFINED:
		default: {
			gfx.eraseDisplay(true);
			gfx.eraseDisplay();
			Serial.println("Wake from RESET or other");
			prefs.putBool("valid", false); //invalidate location data // TODO indicate this on display
			memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);
		}
	}
	//gfx.drawBitmap(BitmapExample1, sizeof(BitmapExample1), gfx.bm_default);
	//gfx.drawBitmap(gImage_Icon2, sizeof(gImage_Icon2), gfx.bm_default);
	//gfx.drawPicture(Icon1, sizeof(Icon1));
	//gfx.powerDown();
	//gfx.powerUp();
	gfx.setRotation(3);
	gfx.setTextColor(GxEPD_BLACK);
	const GFXfont* font9 = &FreeSans9pt7b;		// TODO andymule use ishac fonts
	gfx.setFont(font9);
	gfx.setCursor(3, 30);
	gfx.println("UPDATING");
	gfx.updateWindow(0, 18, 120, 17, true);
	gfx.fillRect(0, 15, 120, 17, GxEPD_WHITE);	// cover it up though
	gfx.setFont();
	Serial.println(":3");

	gfx.setCursor(0, 0);
	//gfx.fillRect(box_x, box_y, box_w, box_h, GxEPD_WHITE);	// cover it up though
	//xTaskCreatePinnedToCore(StartWiFi, "StartWiFi", 4096, NULL, 1, NULL, ARDUINO_RUNNING_CORE); // multicore?
	//vTaskDelete(StartWiFi);

	wifisection = millis();
	StartWiFi();
	lastConnectionTime = millis();
	
	savedSettings.valid = prefs.getBool("valid");
	if (savedSettings.valid)
	{
		Serial.println("loaded saved lat and lon");
		savedSettings.lat = prefs.getFloat("lat");
		savedSettings.lon = prefs.getFloat("lon");
		savedSettings.city = prefs.getString("city");
	}
	else
	{
		Serial.println("getting geolocation from internet");
		geolocatehttp.begin(geolocatestring); //Specify the URL
		DynamicJsonDocument geoDoc(900);
		int geoHttpCode = geolocatehttp.GET();

		//box_x = 40;
		//gfx.fillRect(box_x, 0, 5, gfx.height(), GxEPD_BLACK);
		//gfx.updateWindow(box_x, 0, 5, gfx.height(), true);
		//gfx.fillRect(box_x, 0, 5, gfx.height(), GxEPD_WHITE);
		//Serial.println(geolocatestring);
		if (geoHttpCode == 200)
		{
			// Enough space for:
			// + 1 object with 3 members
			// + 2 objects with 1 member
			//const int capacity = JSON_OBJECT_SIZE(3) + 2 * JSON_OBJECT_SIZE(1);
			//StaticJsonDocument<capacity> doc;
			// TODO look into this for making exactly sized JSON docs
			//"Of course, if the JsonDocument were bigger, it would make sense to move it the heap" (<- from PDF)

			DeserializationError error = deserializeJson(geoDoc, geolocatehttp.getString());  //optimize doc size
			if (error) {
				Serial.print(F("deserializeJson() failed 1 : "));
				Serial.println(error.c_str());
				Sleep();
			}

			String city = geoDoc["city"].as<String>();
			city.replace(" ", "%20");
			savedSettings.lat = geoDoc["latitude"];
			savedSettings.lon = geoDoc["longitude"];
			savedSettings.city = const_cast<char*>(city.c_str());

			prefs.putBool("valid", true);
			prefs.putFloat("lat", savedSettings.lat);
			prefs.putFloat("lon", savedSettings.lon);
			prefs.putString("city", city);

			geolocatehttp.end();
		}
		else {
			Serial.println("Error on Geolocate request:" + geoHttpCode);
			Sleep();
		}
	}

	String weatherCall = weatherCurrent + savedSettings.lat + weatherAndLon + savedSettings.lon;
	weathercurrenthttp.begin(weatherCall); //Specify the URL
	int weatherHttpCode = weathercurrenthttp.GET();
	Serial.println(weatherCall);
	//box_x = 60;
	//gfx.fillRect(box_x, 0, 5, gfx.height(), GxEPD_BLACK);
	//gfx.updateWindow(box_x, 0, 5, gfx.height(), true);
	//gfx.fillRect(box_x, 0, 5, gfx.height(), GxEPD_WHITE);

	StopWiFi(); // stop wifi and reduces power consumption

	DynamicJsonDocument weatherCurrentDoc(12000);
	if (weatherHttpCode == 200)
	{
		DeserializationError error = deserializeJson(weatherCurrentDoc, weathercurrenthttp.getString() ); //optimize doc size
		
		if (error) {
			Serial.print(F("deserializeJson() failed22: "));
			Serial.println(error.c_str());
			Sleep();
		}
		String ObservationTime = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["utcTime"].as<String>();
		int timezonestartI = ObservationTime.lastIndexOf('+');
		int timezoneendI = ObservationTime.lastIndexOf(':');
		int timezone = ObservationTime.substring(timezonestartI + 1, timezoneendI).toInt();
		CurrentTime = weatherCurrentDoc["feedCreation"].as<String>();
		int startI = CurrentTime.indexOf('T');
		int hourI = CurrentTime.indexOf(':');
		int minuteI = hourI+3;
		int hour = CurrentTime.substring(startI+1, hourI).toInt()+timezone; // TODO add timezone 4real -- use lib tho haha
		String minutes = CurrentTime.substring(hourI+1, minuteI);
		CurrentTime = String(hour) + ":" + minutes;
		if (CurrentTime.length() < 5)
			CurrentTime = " " + CurrentTime;	// prepend whitespace to keep time in the corner
		for (int i = 0; i <= 7; i++)
		{
			WeatherDays[i].UTCTime = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["utcTime"];
			WeatherDays[i].DayOfWeek = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["weekday"].as<char*>();
			WeatherDays[i].High = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["highTemperature"];
			WeatherDays[i].Low = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["lowTemperature"];
			WeatherDays[i].SkyText = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["skyDescription"];
			WeatherDays[i].PrecipText = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["precipitationDesc"];
		}

		//ParseIntoWeatherObjects(&weatherCurrentDoc);
		//CurrentTemp = weatherCurrentDoc["main"]["temp"];
		//long temp2 = weatherCurrentDoc["dt"].as<long>(); // time?
		//weathercurrenthttp.end(); // TODO remove bc waste of time?
	}
	else {
		Serial.println("Error on Weather HTTP request:" + weatherHttpCode);
		Sleep();
	}

	//geolocate.region_code.replace(" ", "%20");
	//city = savedSettings.city;

	wifisection = millis() - wifisection;
	displaysection = millis();
	Serial.println("Parsed weather");
	// TODO andymule add other output when error 
	if (weatherHttpCode == 200) {
		//gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
		gfx.setCursor(0, 0);
		gfx.print(savedSettings.city);

		gfx.setCursor(gfx.width()- gfx.width()/9 + 2, 0);
		gfx.print(CurrentTime);

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
		gfx.setCursor(gfx.width() / 2 - 26, 79);
		gfx.println(WeatherDays[0].SkyText);

		gfx.setFont(font12);
		gfx.setCursor(gfx.width() / 2 - 15, 30);
		//gfx.println(CurrentTemp);

		addsun(gfx.width() / 2, 52, 7);	// TODO andymule use bitmap prolly

		DrawDaysAhead(6);

		//gfx.update();
		gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
	}
	displaysection = millis() - displaysection;
	Serial.println("Wifi took:	 " + String(wifisection / 1000.0) + " secs");
	Serial.println("Display took:" + String(displaysection / 1000.0) + " secs");
	Sleep();
}

void Sleep()
{
	gfx.powerDown();	// saves power probably?? lololol
	int result = esp_sleep_enable_timer_wakeup(SleepTimeMicroseconds);
	if (result == ESP_ERR_INVALID_ARG)
	{
		Serial.print("FAILED to sleep:");
	}
	else
	{
		Serial.print("SUCCESS SLEEPING:");
	}
	gettimeofday(&sleep_enter_time, NULL);
	EnableWakeOnTilt();	// actually allows wake on pin touch???
	esp_deep_sleep_start(); // REALLY DEEP sleep and save power
}

void touchpadCallback() {
	Serial.println("TOUCHPAD CALLBACK");
}

void EnableWakeOnTilt()
{
	// TODO andymule detect current orientation and set interupts accordingly
	//esp_sleep_enable_ext1_wakeup(T1, ESP_EXT1_WAKEUP_ANY_HIGH);
	//esp_sleep_enable_ext0_wakeup(T1, ESP_EXT1_WAKEUP_ANY_HIGH);
	//touchAttachInterrupt(T1, touchpadCallback, Threshold);
	touchAttachInterrupt(T2, touchpadCallback, Threshold);
	esp_sleep_enable_touchpad_wakeup();
	//(uint32_t)esp_sleep_get_touchpad_wakeup_status();
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
	char c1 = WeatherDays[daysAfterToday].DayOfWeek[0];
	char c2 = WeatherDays[daysAfterToday].DayOfWeek[1];
	char c3 = WeatherDays[daysAfterToday].DayOfWeek[2];
	String s;
	s = s + c1 + c2 + c3;
	//char* smallDay = "   "; //blank 3 chars
	//strncpy(smallDay, WeatherDays[daysAfterToday].DayOfWeek, 3); // TODO why does strncpy crash esp?
	gfx.println(s);
	gfx.setCursor(width*(daysAfterToday - 1), heightStart + fontHeight);

	String CheckText = String(WeatherDays[daysAfterToday].SkyText);
	//String CheckText = String(WeatherDays[daysAfterToday].PrecipText);
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

//void ParseIntoWeatherObjects(DynamicJsonDocument* root)
//{
//	Serial.println( (*root).as<String>() );
//
//	for (int i = 0; i <= 7; i++)
//	{
//		WeatherDays[i].UTCTime		= (*root)["dailyForecasts"]["forecastLocation"]["forecast"][i]["utcTime"];
//		WeatherDays[i].DayOfWeek	= (*root)["dailyForecasts"]["forecastLocation"]["forecast"][i]["weekday"];
//		WeatherDays[i].High			= (*root)["dailyForecasts"]["forecastLocation"]["forecast"][i]["highTemperature"];
//		WeatherDays[i].Low			= (*root)["dailyForecasts"]["forecastLocation"]["forecast"][i]["lowTemperature"];
//		WeatherDays[i].Text			= (*root)["dailyForecasts"]["forecastLocation"]["forecast"][i]["skyDescription"];
//		//WeatherDays[i].Text			= root["dailyForecasts"]["forecastLocation"]["forecast"][i]["precipitationDesc"];
//
//		Serial.println(String(i) + ":" +
//			String(WeatherDays[i].UTCTime) + " " +
//			String(WeatherDays[i].DayOfWeek) + " " +
//			String(WeatherDays[i].High) + " " +
//			String(WeatherDays[i].Low) + " " +
//			String(WeatherDays[i].Text));
//	}
//}

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

void StartWiFi() {
	int connAttempts = 0;
	Serial.print(F("\r\nConnecting to: "));
	Serial.println(String(ssid));
	//WiFi.disconnect();
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	//WiFi.waitForConnectResult();
	//WiFi.setAutoReconnect(true);
	while (WiFi.status() != WL_CONNECTED) {
		delay(50); Serial.print(F("."));
		if (connAttempts > 200) {	// give it 10 seconds?
			Serial.println("WiFi down? Failed to connect. RESTARTING DEVICE?");
			// wifi prolly down, try again later
			ESP.restart();	// TODO handle no wifi or can't connect better than this. prompt screen?
			Sleep();
		}
		connAttempts++;
	}
	Serial.println("WiFi connected at: " + String(WiFi.localIP()) + " ms" + (connAttempts*50));
}
//#########################################################################################
void StopWiFi() {
	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);
	//esp_wifi_stop(); // this is the old way 
	//Serial.println("Wifi Off");
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

//void UpdateSettings()
//{
//	SPIFFS.begin();
//	File settingsFile = SPIFFS.open(SETTINGS_FILE, "r+");//overwrite if found
//	if (!settingsFile)
//	{
//		settingsFile = SPIFFS.open(SETTINGS_FILE, "w");	// make a new file if not found		
//	}
//	Serial.println("writing file..");
//	unsigned char * data = reinterpret_cast<unsigned char*>(&savedSettings); // use unsigned char, as uint8_t is not guarunteed to be same width as char...
//	size_t bytes = settingsFile.write(data, sizeof(SavedSettings));
//	settingsFile.close();
//	yield();
//}
//
//void ReloadSavedSettings()
//{
//	SPIFFS.begin(true);
//	//int ret = esp_spiffs_info(NULL, &total, &used);
//
//	File settingsFile = SPIFFS.open(SETTINGS_FILE, "r");
//	yield;
//	if (settingsFile && settingsFile.available())
//	{
//		if (settingsFile.size() == sizeof(SavedSettings))
//		{
//			uint8_t loadedSettings[sizeof(SavedSettings)];
//			for (int i = 0; i < sizeof(SavedSettings); i++)
//			{
//				loadedSettings[i] = settingsFile.read();
//			}
//			SavedSettings *loadedSettingsPtr = (SavedSettings *)loadedSettings;
//			SavedSettings *savedSettingsPtr = &savedSettings;
//			memcpy(savedSettingsPtr, loadedSettingsPtr, sizeof(SavedSettings));
//			Serial.println(savedSettings.lat);
//			Serial.println(savedSettings.lon);
//			Serial.println(savedSettings.city);
//		}
//		else // file exists but size is wrong, probably bc of firmware change. delete it.
//		{
//			Serial.println("OOP FORMAT");
//			SPIFFS.format();
//		}
//	}
//	else // file not found, oh well for now. We'll make one when we change settings
//	{
//		Serial.println("File not found! formatting...");
//		SPIFFS.format();
//	}
//	settingsFile.close();
//	yield();
//}