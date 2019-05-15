#define NO_METRIC 1
// TODO detect time offset when awake and adjust to stay close to 24hrs (bc drifts in deepsleep)
// TODO remove FREERTOS thing and try swapping cores, does it save time?
#include <SPI.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
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
#include <time.h>
#include <sys/time.h>
#include <TimeLib.h>
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/FreeMonoBold12pt7b.h>
// make your own fonts here: http://oleddisplay.squix.ch/
//#include "Icon2.h"

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16);	// arbitrary selection of 17, 16
GxEPD_Class gfx(io, /*RST=*/ 16, /*BUSY=*/ 4);				 // arbitrary selection of (16), 4

HTTPClient weathercurrenthttp, geolocatehttp;
TaskHandle_t* WiFiTask;
static bool WiFiStarted = false;
int wifisection, displaysection;
unsigned long lastConnectionTime = 0;          // Last time you connected to the server, in milliseconds
// 1000000LL is one second

const uint64_t OneSecond = 1000000LL;
const uint64_t Time60 = 60LL;		/*60 min = 1 hour*/
const uint64_t OneMinute = OneSecond * Time60;	/*6000000 uS = 1 minute*/
const uint64_t OneHour = OneMinute * Time60;
const uint64_t OneDay = OneHour * 24;
// TODO andymule this number weirdly lets me wake up every 12 hours or so ? This hasn't proven true and needs more work

//const uint64_t SleepTimeMicroseconds = OneHour * 24LL;
bool SleepDriftWasTooFast = false;	// flag to handle math bc we're using uint64 and need to handle negatives
uint64_t SleepDriftCompensation = 0;

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
//static RTC_DATA_ATTR struct timeval sleep_enter_time;	// TODO exploit this for saving preferences??

// original strings:
// http://api.openweathermap.org/data/2.5/weather?&appid=ba42e4a918f7e742d3143c5e8fff9210&lat=59.3307&lon=18.0718&units=metric
//https://weather.api.here.com/weather/1.0/report.json?app_id=JoN1SBsEzJ5pWD5OkXwN&app_code=J9XdgHlHuUKzV2j5GqxlVg&product=forecast_7days_simple&latitude=59.3307&longitude=18.0718

//const String weatherCurrent = "http://api.openweathermap.org/data/2.5/weather?&appid=ba42e4a918f7e742d3143c5e8fff9210&lat=";
const String weatherCurrent = "https://weather.api.here.com/weather/1.0/report.json?app_id=JoN1SBsEzJ5pWD5OkXwN&app_code=J9XdgHlHuUKzV2j5GqxlVg&product=forecast_7days_simple&product=observation&oneobservation=true&latitude=";
const String weatherAndLon = "&longitude=";
const String weatherNoMetric = "&metric=false";	// todo is this autoresolved by here api?
const String weatherLanguage = "&language=";	// https://developer.here.com/documentation/weather/topics/supported-languages.html

const String geolocatestring = "http://api.ipstack.com/check?access_key=d0dfe9b52fa3f5bb2a5ff47ce435c7d8"; //key=ab925796fd105310f825bbdceece059e

const char* ssid = "slow";
const char* password = "bingbangbong";

class SavedSettings
{
public:
	bool valid;
	String city;
	float lat;
	float lon;
};
Preferences prefs;	// used to save and load from memory
SavedSettings savedSettings; // loads from memory into RAM, used for refernce afterward

class WeatherDay
{
public:
	int High;
	int Low;
	const char* SkyText;
	const char* PrecipText;
	//const char* UTCTime;
	const char* DayOfWeek;
};
WeatherDay WeatherDays[10] = {};

String CurrentTime;
int CurrentTemp;
String TodaySky;
String TodayTempDesc;
int TodayHigh;
int TodayLow;

// useful tools, like bitmap converter, fonts, and font converters
// https://github.com/cesanta/arduino-drivers/tree/master/Adafruit-GFX-Library
//#include GxEPD_BitmapExamples

//const int width = 296;
//const int height = 128;
#define SITE_TIMEOUT_MS 5000 // timeout for site request
#define WIFI_DELAY_CHECK_TIME_MS 50
#define WIFI_TIMEOUT_MS 10000 // 10 sec to connect to wifi?
const GFXfont* font9 = &FreeSans9pt7b;		// TODO andymule use ishac fonts
const GFXfont* font12 = &FreeSans12pt7b;

// TODO andymule FOR RELEASE BUILD Disabling all logging and holding the UART disable pin high only increases boot time by around 20 ms?
void setup() {
	wifisection = millis();
	bool runForever = true;
	xTaskCreatePinnedToCore(StartWiFi, "StartWiFi", 2048, &runForever, 1, WiFiTask, 0); // start wifi on other core

	//struct timeval wakeTime;
	//gettimeofday(&wakeTime, NULL);
	//int sleep_time_ms = (wakeTime.tv_sec - sleep_enter_time.tv_sec) * 1000 + (wakeTime.tv_usec - sleep_enter_time.tv_usec) / 1000;
	Serial.begin(128000);
	//Serial.print("Time spent in deep sleep : ");
	//Serial.print(sleep_time_ms);
	//Serial.println(" ms");

	gfx.init();
	gfx.setRotation(3);
	prefs.begin("settings");

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
		Serial.println("Wake up from timer.");
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

	DrawUpdating();

	//StartWiFi(nullptr);
	lastConnectionTime = millis();

	savedSettings.valid = prefs.getBool("valid");
	if (savedSettings.valid)
	{
		ReloadSavedSettings();
	}
	else
	{
		Serial.println("getting geolocation from internet");
		geolocatehttp.setTimeout(SITE_TIMEOUT_MS);
		EnsureWiFiIsStarted();
		geolocatehttp.begin(geolocatestring);
		int geoHttpCode = geolocatehttp.GET();

		if (geoHttpCode == 200)
		{
			ParseGeoLocation();
			//geolocatehttp.end(); 
			// we dont end the geolocatehttp to save time TODO does this actually save time?
		}
		else { // FAILED TO CONNECT TO GEOLOCATE SITE
			FailedToConnectToSite();	// draws to epaper
			Sleep();
		}
	}
	String weatherCall = weatherCurrent + savedSettings.lat + weatherAndLon + savedSettings.lon;
	if (NO_METRIC)
	{
		weatherCall = weatherCall + weatherNoMetric;
	}
	weathercurrenthttp.setTimeout(SITE_TIMEOUT_MS);
	EnsureWiFiIsStarted();
	weathercurrenthttp.begin(weatherCall); //Specify the URL
	int weatherHttpCode = weathercurrenthttp.GET();

	StopWiFi(); // stop wifi and reduces power consumption

	if (weatherHttpCode == 200)
	{
		ParseWeatherAndTime();
		// we dont end the weathercurrenthttp to save time TODO does this actually save time?
	}
	else {
		FailedToConnectToSite();
		Serial.println("Error on Weather HTTP request:" + weatherHttpCode);
		Sleep();
	}

	wifisection = millis() - wifisection;
	displaysection = millis();
	if (weatherHttpCode == 200) {
		DrawDisplay();
	}
	displaysection = millis() - displaysection;
	Serial.println("Wifi took:	 " + String(wifisection / 1000.0) + " secs");
	Serial.println("Display took:" + String(displaysection / 1000.0) + " secs");
	Sleep();
}

// ############################# END MAIN CODE ###################################################

void Sleep()
{
	gfx.powerDown();	// saves power probably?? TODO check
	EnableWakeOnTilt();	// actually allows wake on pin touch???
	uint64_t sleepTime = 0;
	if (SleepDriftWasTooFast)
		sleepTime = OneDay + SleepDriftCompensation; // woke up early from being too fast, so sleep longer next time
	else
		sleepTime = OneDay - SleepDriftCompensation;
	int result = esp_sleep_enable_timer_wakeup(sleepTime);
	//gettimeofday(&sleep_enter_time, NULL);
	esp_deep_sleep_start();
}

void touchpadCallback() {
	//Serial.println("TOUCHPAD CALLBACK");
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

int HalfWidthOfText(String text, int size)
{
	return text.length()*size / 2;
}

void DrawDaysAhead(int daysAhead)
{
	for (int i = 0; i < daysAhead; i++)
	{
		DrawDayOfWeek(i, (gfx.width() / daysAhead), 90, 9);
	}
}

void DrawDayOfWeek(int daysAfterToday, int width, int heightStart, int fontHeight)
{
	gfx.setFont();	// resets to default little font guy
	gfx.setCursor(width*(daysAfterToday), heightStart);
	char c1 = WeatherDays[daysAfterToday].DayOfWeek[0];
	char c2 = WeatherDays[daysAfterToday].DayOfWeek[1];
	char c3 = WeatherDays[daysAfterToday].DayOfWeek[2];
	String s;
	s = s + c1 + c2 + c3;
	//char* smallDay = "   "; //blank 3 chars
	//strncpy(smallDay, WeatherDays[daysAfterToday].DayOfWeek, 3); // TODO why does strncpy crash esp?
	gfx.println(s);
	gfx.setCursor(width*(daysAfterToday), heightStart + fontHeight);

	String CheckText = String(WeatherDays[daysAfterToday].SkyText);
	//String CheckText = String(WeatherDays[daysAfterToday].PrecipText);
	String PrintText = String(CheckText);	// TODO smarter way to display just one word, need a real parser
	if (CheckText.indexOf(" ") > 0)
	{
		PrintText = CheckText.substring(CheckText.lastIndexOf(" ") + 1, CheckText.length());
	}
	gfx.println(PrintText);

	gfx.setCursor(width*(daysAfterToday), heightStart + fontHeight * 2);
	gfx.println(WeatherDays[daysAfterToday].High);
	gfx.setCursor(width*(daysAfterToday), heightStart + fontHeight * 3);
	gfx.println(WeatherDays[daysAfterToday].Low);
}

void FailedToConnectToSite()
{
	Serial.println("Error on Geolocate request");
	gfx.setFont(font9);
	gfx.setCursor(0, 60 + 9);
	gfx.println("Failed to connect to sites.");
	gfx.println("Check your internet connection.");
	//gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
	gfx.update();
}

void FailedToConnectToWiFi()
{
	//gfx.init();
	//gfx.eraseDisplay(true);
	//gfx.eraseDisplay();
	gfx.setFont(font9);
	gfx.setCursor(0, 60 + 9);
	gfx.println("Failed to connect to WiFi.");
	gfx.println("Check your router.");
	gfx.update();
}

void EnsureWiFiIsStarted()
{
	bool deleteTask = false;
	while (WiFiStarted == false)
	{
		deleteTask = true;
		Serial.print(".");
		delay(10);
	}
	if (deleteTask) {
		vTaskDelete(WiFiTask);
		Serial.println();
	}
}

void loop() { // this will never run!
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

void StartWiFi(void *loopForever) {
	int connAttempts = 0;
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(WIFI_DELAY_CHECK_TIME_MS); //Serial.print(F("."));
		if (connAttempts > WIFI_TIMEOUT_MS / WIFI_DELAY_CHECK_TIME_MS) {
			FailedToConnectToWiFi();
			Sleep();
		}
		connAttempts++;
	}
	WiFiStarted = true;
	while (loopForever)
		delay(1000);
}

void StopWiFi() {
	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);
}

//#########################################################################################

void ParseWeatherAndTime()
{
	DynamicJsonDocument weatherCurrentDoc(12000);
	DeserializationError error = deserializeJson(weatherCurrentDoc, weathercurrenthttp.getString()); //optimize doc size

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
	SetClockAndDriftCompensate();
	ParseCurrentTimeForDisplay(timezone);

	for (int i = 0; i <= 7; i++)
	{
		//WeatherDays[i].UTCTime = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["utcTime"];
		WeatherDays[i].DayOfWeek = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["weekday"].as<char*>();
		WeatherDays[i].High = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["highTemperature"];
		WeatherDays[i].Low = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["lowTemperature"];
		WeatherDays[i].SkyText = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["skyDescription"];
		WeatherDays[i].PrecipText = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["precipitationDesc"];
	}

	TodayHigh = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["highTemperature"];
	TodayLow = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["lowTemperature"];
	TodaySky = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["skyDescription"].as<String>();
	TodayTempDesc = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["temperatureDesc"].as<String>();
	CurrentTemp = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["temperature"];
}

void SetClockAndDriftCompensate()
{	// CurrentTime should be freshly parsed from JSON at this point
	// 0YYY-5M-8DThh:mm:ssfZ
	int YYYY = CurrentTime.substring(0, 4).toInt();
	int MM = CurrentTime.substring(5, 2).toInt();
	int DD = CurrentTime.substring(8, 2).toInt();
	int hh = CurrentTime.substring(11, 2).toInt();
	int mm = CurrentTime.substring(14, 2).toInt();
	int ss = CurrentTime.substring(17, 2).toInt();
	tmElements_t tm;
	tm.Year = CalendarYrToTm(YYYY);
	tm.Month = MM;
	tm.Day = DD;
	tm.Hour = hh;
	tm.Minute = mm;
	tm.Second = ss;
	time_t oldTime = now();
	time_t newTime = makeTime(tm);
	setTime(newTime);
	int64_t timeDriftSeconds = newTime - oldTime; // negative if i woke up early, bc oldtime was too fast
	SleepDriftWasTooFast = (timeDriftSeconds < 0);
	if (SleepDriftWasTooFast)
		timeDriftSeconds *= -1;
	SleepDriftCompensation = (uint64_t)timeDriftSeconds * OneSecond;
}

void ParseCurrentTimeForDisplay(int timezone)
{
	int startI = CurrentTime.indexOf('T');
	int hourI = CurrentTime.indexOf(':');
	int minuteI = hourI + 3;
	int hour = CurrentTime.substring(startI + 1, hourI).toInt() + timezone; // TODO add timezone 4real -- use lib tho haha
	String minutes = CurrentTime.substring(hourI + 1, minuteI);
	CurrentTime = String(hour) + ":" + minutes;
	if (CurrentTime.length() < 5)
		CurrentTime = " " + CurrentTime;	// prepend whitespace to keep time in the corner
}

void ParseGeoLocation()
{
	// Enough space for:
			// + 1 object with 3 members
			// + 2 objects with 1 member
			//const int capacity = JSON_OBJECT_SIZE(3) + 2 * JSON_OBJECT_SIZE(1);
			//StaticJsonDocument<capacity> doc;
			// TODO look into this for making exactly sized JSON docs
			//"Of course, if the JsonDocument were bigger, it would make sense to move it the heap" (<- from PDF)
	DynamicJsonDocument geoDoc(900);
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
}

// ########################################################################################

void ReloadSavedSettings()
{
	Serial.println("loaded saved lat and lon");
	savedSettings.lat = prefs.getFloat("lat");
	savedSettings.lon = prefs.getFloat("lon");
	savedSettings.city = prefs.getString("city");
}

// ########################################################################################
void DrawDisplay()
{
	int setback = 0;
	// city in top left
	gfx.setFont();	// uses tiny default font
	gfx.setCursor(0, 0);
	gfx.print(savedSettings.city);

	// time in top right
	gfx.setCursor(gfx.width() - gfx.width() / 9 + 2, 0);
	gfx.print(CurrentTime);

	//day of week
	//gfx.setFont(font12);
	//gfx.setCursor(gfx.width() / 2 - 16, 30);
	//gfx.println(WeatherDays[0].DayOfWeek);

	gfx.setFont(font12);
	setback = HalfWidthOfText(String(WeatherDays[0].Low), 12);
	gfx.setCursor(gfx.width() / 4 - setback, 35);
	//gfx.print(" Low:");
	gfx.print(WeatherDays[0].Low);
	//gfx.println("°");	// TODO andymule draw degrees // TODO turn centering boilerplate into method
	setback = HalfWidthOfText(String(WeatherDays[0].High), 12);
	gfx.setCursor(gfx.width() - gfx.width() / 4 - setback, 35);
	//gfx.print("High:");
	gfx.print(WeatherDays[0].High);
	//gfx.println(String("°"));	// TODO andymule draw degrees // TODO turn centering boilerplate into method

	gfx.setFont(font9);
	setback = HalfWidthOfText(TodayTempDesc, 9);
	gfx.setCursor(gfx.width() / 2 - setback, 53);
	gfx.println(TodayTempDesc);

	setback = HalfWidthOfText(TodaySky, 9);
	gfx.setCursor(gfx.width() / 2 - setback, 73);
	gfx.println(TodaySky);

	gfx.setFont(font9);
	setback = HalfWidthOfText(String(CurrentTemp), 9);
	gfx.setCursor(gfx.width() / 2 - setback, 20 + 9);
	gfx.println(CurrentTemp);

	//addsun(gfx.width() / 2, 17, 7);	// TODO andymule use bitmap prolly

	DrawDaysAhead(6);

	//gfx.update();
	gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
}

void DrawUpdating()
{
	int setback = 0;
	gfx.setTextColor(GxEPD_BLACK);
	gfx.setFont(font9);
	setback = HalfWidthOfText("updating", 9);
	gfx.setCursor(gfx.width() / 2 - setback, 20 + 9);
	gfx.println("updating");
	gfx.updateWindow(gfx.width() / 2 - setback, 20, setback * 2, 9, true);
	gfx.fillRect(gfx.width() / 2 - setback, 20 - 3, setback * 2 + 3, 9 * 2, GxEPD_WHITE);	// cover it up though
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


/*
https://developer.here.com/documentation/weather/topics/resource-type-weather-items.html
skyInfo	String	Sky descriptor value.
If the element is in the response and it contains a value, there is an Integer in the String. If the element is in the response and it does not contain a value, there is an asterisk (*) in the String.

The available values are as follows:
*/

String shortWeatherType(int skyInfo)
{
	switch (skyInfo)
	{
	case 1:		//1 – Sunny
		return "sunny";
	case 2:		//2 – Clear
		return "clear";
	case 3:		//3 – Mostly Sunny
		return "sunny";
	case 4:		//4 – Mostly Clear
		return "clear";
	case 5:		//5 – Hazy Sunshine
		return "hazysun"; // todo make sure 7 can fit
	case 6:		//6 – Haze
		return "hazy";
	case 7:		//7 – Passing Clouds
		return "clouds";
	case 8://8 – More Sun than Clouds
		return "sunny";
	case 9://9 – Scattered Clouds
		return "cloudy";
	case 10://10 – Partly Cloudy
		return "cloudy";
	case 11://11 – A Mixture of Sun and Clouds
		return "cloudy";
	case 12://12 – High Level Clouds
		return "clouds";
	case 13://13 – More Clouds than Sun
		return "clouds";
	case 14://14 – Partly Sunny
		return "sunnish";
	case 15://15 – Broken Clouds
		return "cloudish";
	case 16://16 – Mostly Cloudy
		return "cloudy";
	case 17://17 – Cloudy
		return "cloudy";
	case 18://18 – Overcast
		return "overcast";
	case 19://19 – Low Clouds
		return "overcast";
	case 20://20 – Light Fog
		return "foggy";
	case 21://21 – Fog
		return "fog";
	case 22://22 – Dense Fog
		return "FOG";
	case 23://23 – Ice Fog
		return "icefog";
	case 24://24 – Sandstorm
		return "sandstorm";
	case 25://25 – Duststorm
		return "duststorm";
	case 26://26 – Increasing Cloudiness
		return "clouds";
	case 27://27 – Decreasing Cloudiness
		return "clouds";
	case 28://28 – Clearing Skies
		return "clear";
	case 29://29 – Breaks of Sun Later
		return "cloudy";
	case 30://30 – Early Fog Followed by Sunny Skies
		return "sun";
	case 31://31 – Afternoon Clouds
		return "clouds";
	case 32://32 – Morning Clouds
		return "clouds";
	case 33://33 – Smoke
		return "smoke";
	case 34://34 – Low Level Haze
		return "hazy";
	}
	return "";
}
