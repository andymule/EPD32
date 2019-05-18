
#define NO_METRIC 1
// TODO consider clock reset from pin, make sure timer makes sense (doesn't currently)
// TODO remove FREERTOS thing and try swapping cores, does it save time?
// TODO detect wrong password? explicitly 
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
#include "atmo_parsing.h"

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);
HTTPClient weathercurrenthttp, geolocatehttp;
int wifisection, displaysection;
unsigned long lastConnectionTime = 0;          // Last time you connected to the server, in milliseconds
// 1000000LL is one second


#define pp Serial.println

// original strings:
// http://api.openweathermap.org/data/2.5/weather?&appid=ba42e4a918f7e742d3143c5e8fff9210&lat=59.3307&lon=18.0718&units=metric
//https://weather.api.here.com/weather/1.0/report.json?app_id=JoN1SBsEzJ5pWD5OkXwN&app_code=J9XdgHlHuUKzV2j5GqxlVg&product=forecast_7days_simple&latitude=59.3307&longitude=18.0718

//const String weatherCurrent = "http://api.openweathermap.org/data/2.5/weather?&appid=ba42e4a918f7e742d3143c5e8fff9210&lat=";
const String weatherCurrent = "https://weather.api.here.com/weather/1.0/report.json?app_id=JoN1SBsEzJ5pWD5OkXwN&app_code=J9XdgHlHuUKzV2j5GqxlVg&product=forecast_7days_simple&product=observation&oneobservation=true&latitude=";
const String weatherAndLon = "&longitude=";
const String weatherNoMetric = "&metric=false";	// todo is this autoresolved by here api?
const String weatherLanguage = "&language=";	// https://developer.here.com/documentation/weather/topics/supported-languages.html

const String geolocatestring = "http://api.ipstack.com/check?access_key=d0dfe9b52fa3f5bb2a5ff47ce435c7d8"; //key=ab925796fd105310f825bbdceece059e

const char* selfhostedWifiName = "ATMO";
String header;




bool HostSetupSite = false;
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
		prefs.putBool("valid", true); //invalidate location data // TODO indicate this on display
		Serial.println("Wakeup caused by touchpad");
		int pad = get_wakeup_gpio_touchpad();
		pp("PAD:" + String(pad));
		if (pad == 27)
		{
			HostSetupSite = true;
		}
		break;
	}
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by EXT0"); break;
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by EXT1"); break;
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_UNDEFINED:
	default: {
		gfx.eraseDisplay(true);
		gfx.eraseDisplay();
		Serial.println("Wake from RESET or other");
		//prefs.putBool("valid", false); //invalidate location data // TODO indicate this on display
		memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);
	}
	}

	savedSettings.valid = prefs.getBool("valid");
	if (savedSettings.valid)
	{
		ReloadSavedSettings();
	}

	if (HostSetupSite || savedSettings.wifi_ssid == "")
	{
		DrawConnectionInstructions();
		HostWebsiteForInit();
		return;
	}

	bool runForever = true;
	xTaskCreatePinnedToCore(StartWiFi, "StartWiFi", 2048, &runForever, 1, WiFiTask, 0); // start wifi on other core // TODO Move earlier but dont start if i host the server

	DrawUpdating();

	lastConnectionTime = millis();

	if (!savedSettings.valid)
	{
		Serial.println("getting geolocation from internet");
		geolocatehttp.setTimeout(SITE_TIMEOUT_MS);
		EnsureWiFiIsStarted();
		geolocatehttp.begin(geolocatestring);
		int geoHttpCode = geolocatehttp.GET();
		if (geoHttpCode == 200)
		{
			ParseGeoLocation(geolocatehttp.getString());
			//geolocatehttp.end(); 
			// we dont end the geolocatehttp to save time TODO does this actually save time?
		}
		else if (geoHttpCode != 200) { // FAILED TO CONNECT TO GEOLOCATE SITE
			DrawFailedToConnectToSite();	// draws to epaper
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
		ParseWeatherAndTime(weathercurrenthttp.getString());
		// we dont end the weathercurrenthttp to save time TODO does this actually save time?
	}
	else if (weatherHttpCode != 200) {
		DrawFailedToConnectToSite();
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

void loop() {
	//pp("doin it!");
	dnsServer.processNextRequest();
	delay(1);
	server.handleClient();
	delay(200);
	//TODO if press pad again, go back to normal operation
}

String randomExitHandle = "";
String RandomHandle()
{
	randomSeed(analogRead(0));
	int rand = random(1000, 1000000);
	return String(rand);
}

void handle_ExitSetup()
{
	Serial.println("EXIT SETUP!");
	for (int i = 0; i < server.args(); i++) {
		if (server.argName(i) == "n") // wifi name
		{
			savedSettings.wifi_ssid = server.arg(i);
			prefs.putString("wifi_ssid", savedSettings.wifi_ssid);
		}
		if (server.argName(i) == "p") // wifi name
		{
			savedSettings.wifi_password = server.arg(i);
			prefs.putString("wifi_password", savedSettings.wifi_password);
		}
	}
	//Serial.println(server.argName(i) + ":" + server.arg(i));
	//gfx.eraseDisplay();
	gfx.eraseDisplay(true);
	prefs.putBool("valid", true); //invalidate location data // TODO indicate this on display
	String ptr = "<!DOCTYPE html> <html>\n";
	ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
	ptr += "<title>Atmo Weather Friend</title>\n";
	ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
	ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
	ptr += "</style>\n";
	ptr += "</head>\n";
	ptr += "<body>\n";
	ptr += "<h1>Settings saved!!</h1> <br>";
	ptr += "<h3>Check your Atmo device to verify the connection.</h3>\n";
	ptr += "</body>\n";
	ptr += "</html>\n";
	server.send(200, "text/html", ptr);
	//delay(100);	// make sure we write
	server.close();
	delay(100);	// make sure we write
	esp_deep_sleep(1 * OneSecond);	// TODO is this best? prolly need to flag to avoid hard screen blanking
}

void handle_OnConnect() {
	Serial.println("New Connection!");
	//for (int i = 0; i < server.args(); i++) {
	//	if (server.argName(i) == "n") // wifi name
	//	{
	//		savedSettings.wifi_ssid = server.arg(i);
	//	}
	//	if (server.argName(i) == "p") // wifi name
	//	{
	//		savedSettings.wifi_password = server.arg(i);
	//	}
	//	//Serial.println(server.argName(i) + ":" + server.arg(i));
	//}
	server.send(200, "text/html", SendHTML());
}
void webHandleDefault();
void HostWebsiteForInit()
{
	WiFi.mode(WIFI_AP);
	WiFi.softAP(selfhostedWifiName);
	delay(50);
	IPAddress iip(1, 1, 1, 1);
	IPAddress igateway(1, 1, 1, 1);
	IPAddress isubnet(255, 255, 255, 0);
	WiFi.softAPConfig(iip, igateway, isubnet);
	dnsServer.start(DNS_PORT, "*", iip);

	// TODO detect first connection, always route to init connect instead of exit ?
	const String root = "/";
	//auto glambda = [](auto a, auto&& b) { return a < b; };
	//server.onNotFound(webHandleDefault);
	server.on("/", handle_OnConnect);	// this parses an error in intellisense but is totally fine

	randomExitHandle = RandomHandle();
	server.on("/" + randomExitHandle, handle_ExitSetup);	// this parses an error in intellisense but is totally fine

	//server.on(", handle_OnConnect);
	//DrawConnectionInstructions();

	server.begin();

	IPAddress IP = WiFi.softAPIP();
	Serial.print("page address: ");
	Serial.println(IP);
}

// TODO drop down auto-populate menu, option for text?
// TODO validate settings on exit?
String SendHTML() {
	String ptr = "<!DOCTYPE html> <html>\n";
	ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
	ptr += "<title>Atmo Weather Friend</title>\n";
	ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
	ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
	ptr += ".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
	ptr += ".button-on {background-color: #3498db;}\n";
	ptr += ".button-on:active {background-color: #2980b9;}\n";
	ptr += ".button-off {background-color: #34495e;}\n";
	ptr += ".button-off:active {background-color: #2c3e50;}\n";
	ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
	ptr += "</style>\n";
	ptr += "</head>\n";
	ptr += "<body>\n";
	ptr += "<h1>Atmo</h1>\n";
	ptr += "<h3>Configuration</h3>\n";

	ptr += "<form action=\"/";
	ptr += randomExitHandle;
	ptr += "\" method=GET>WiFi Network: <input type=text name=n value=\"";

	ptr += prefs.getString("wifi_ssid");
	ptr += "\"><br><br>";

	ptr += "WiFi Password: <input type=text name=p value=\"";
	ptr += prefs.getString("wifi_password");
	ptr += "\"><br><input type=submit></form>";

	//ptr += "<br><br><a class=\"button button-off\" href=\"/exit\">OFF</a>\n";
	//if (led1stat)
	//{
	//	
	//}
	//else
	//{
	//	ptr += "<p>LED1 Status: OFF</p><a class=\"button button-on\" href=\"/led1on\">ON</a>\n";
	//}

	//if (led2stat)
	//{
	//	ptr += "<p>LED2 Status: ON</p><a class=\"button button-off\" href=\"/led2off\">OFF</a>\n";
	//}
	//else
	//{
	//	ptr += "<p>LED2 Status: OFF</p><a class=\"button button-on\" href=\"/led2on\">ON</a>\n";
	//}

	ptr += "</body>\n";
	ptr += "</html>\n";
	return ptr;
}