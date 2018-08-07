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

const char* ssid1 = "ohm";
const char* password1 = "shesthebest";

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16);	// arbitrary selection of 17, 16
GxEPD_Class gfx(io, /*RST=*/ 16, /*BUSY=*/ 4);				 // arbitrary selection of (16), 4

unsigned long lastConnectionTime = 0;          // Last time you connected to the server, in milliseconds
//1000000L is one second

// TODO andymule this number weirdly lets me wake up every 6 hours or so ? This hasn't proven true and needs more work
const uint64_t UpdateInterval = 68719476736;//60L*60L*12L*1000000L*6L*12L; // Delay between updates, in microseconds

// TODO grab location from IP, for now it's this hard string to Seattle
String yahoo = "https://query.yahooapis.com/v1/public/yql?q=select%20*%20from%20weather.forecast%20where%20woeid%20in%20(select%20woeid%20from%20geo.places(1)%20where%20text%3D%22seattle%2C%20wa%22)&format=json&env=store%3A%2F%2Fdatatables.org%2Falltableswithkeys";
//unsigned long timeout = 10000;  // ms

HTTPClient http;
WiFiClient client; // wifi client object

//const int width = 296;
//const int height = 128;

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


// (someone else wrote this math)
// Wifi on section takes : 7.37 secs to complete at 119mA using a Lolin 32 (wifi on)
// Display section takes : 10.16 secs to complete at 40mA using a Lolin 32 (wifi off)
// Sleep current is 175uA using a Lolin 32
// Total power consumption per-hour based on 2 updates at 30-min intervals = (2x(7.37x119/3600 + 10.16x40/3600) + 0.15 x (3600-2x(7.37+10.16))/3600)/3600 = 0.887mAHr 
// A 2600mAhr battery will last for 2600/0.88 = 122 days 

// using his math, andymule does this: (TODO andymule didnt really check this dudes math)
// A 1000mAhr battery will last 1000/2600 * 122days = 47 days
// updating 3 times a day instead of 48 = 48/3*47 = 752 days OR 2.11 years

//#########################################################################################
void setup() {
	Serial.begin(128000);
	wifisection = millis();
	StartWiFi();
	//SetupTime();
	lastConnectionTime = millis();

	http.begin(yahoo); //Specify the URL
	int httpCode = http.GET();

	if (httpCode == 200)
	{
		//StaticJsonBuffer<6000> jsonBuffer;	// we're told to use this, but it doesn't parse so we're using the deprecated dynamicjsonbuffer
		DynamicJsonBuffer  jsonBuffer(6000);
		JsonObject& root = jsonBuffer.parseObject(http.getString());
		city = root["query"]["results"]["channel"]["location"]["city"];
		weathertime = root["query"]["results"]["channel"]["lastBuildDate"];
		//Serial.println(weathertime);
		ParseIntoWeatherObjects(root);	// TODO andymule maybe can do this after http.end and StopWiFi to save battery???
	}
	else {
		goto sleep;
		//weathertime = "HTTP ERROR:" + httpCode;
		//Serial.println("Error on HTTP request");
	}
	http.end();
	StopWiFi(); // stop wifi and reduces power consumption
	
	if (httpCode > 0) {
		//Received data OK at this point so turn off the WiFi to save power
		//displaysection = millis();
		gfx.init();
		gfx.setRotation(3);
		gfx.setTextColor(GxEPD_BLACK);

		gfx.setCursor(gfx.width() / 4, 2);
		gfx.println(weathertime);

		const GFXfont* font9  = &FreeSans9pt7b;		// TODO andymule use ishac fonts
		const GFXfont* font12 = &FreeSans12pt7b;
		gfx.setFont(font12);
		gfx.setCursor(gfx.width()/2-16, 30);
		//gfx.println(WeatherDays[0].DayOfWeek);

		gfx.setFont(font12);
		gfx.setCursor(8, 59);
		gfx.print(" Low:");
		gfx.print(WeatherDays[0].Low);
		//gfx.println(String("°"));	// TODO andymule draw degrees
		gfx.setCursor(gfx.width()-89, 59);
		gfx.print("High:");
		gfx.print(WeatherDays[0].High);
		//gfx.println(String("°")); // TODO andymule draw degrees

		gfx.setFont(font9);
		gfx.setCursor(gfx.width() / 2 - 26, 30);
		gfx.println(WeatherDays[0].Text);

		addsun(gfx.width() / 2, 62, 11);	// TODO andymule use bitmap prolly

		DrawDayOfWeek(1);	// TODO andymule not awesome code here, but its fine
		DrawDayOfWeek(2);
		DrawDayOfWeek(3);
		DrawDayOfWeek(4);
		DrawDayOfWeek(5);
		DrawDayOfWeek(6);
		DrawDayOfWeek(7);

		gfx.update();
		gfx.powerDown();	// saves power probably?? lololol
	}

	//Serial.println("   Wifi section took: "+ String(wifisection/1000.0)+" secs");
	//Serial.println("Display section took: "+ String((millis()-displaysection)/1000.0)+" secs");

	sleep:
		esp_sleep_enable_timer_wakeup(UpdateInterval);
		esp_deep_sleep_start(); // REALLY DEEP sleep and save power
}

void DrawDayOfWeek(int daysAfterToday)
{
	gfx.setFont();	// resets to default little font guy
	gfx.setCursor((gfx.width()/7)*(daysAfterToday-1), 90);
	gfx.println(WeatherDays[daysAfterToday].DayOfWeek);
	gfx.setCursor((gfx.width() / 7)*(daysAfterToday - 1), 99);

	String CheckText = String(WeatherDays[daysAfterToday].Text);
	String PrintText = String(CheckText);
	if (CheckText.indexOf(" ") > 0)
	{
		PrintText = CheckText.substring(CheckText.lastIndexOf(" ")+1, CheckText.length());
	}
	gfx.println(PrintText);

	gfx.setCursor((gfx.width() / 7)*(daysAfterToday - 1), 108);
	gfx.println(WeatherDays[daysAfterToday].High);
	gfx.setCursor((gfx.width() / 7)*(daysAfterToday - 1), 117);
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
		Serial.println(String(i) + ":" +
			String(WeatherDays[i].Date) + " " +
			String(WeatherDays[i].DayOfWeek) + " " +
			String(WeatherDays[i].High) + " " +
			String(WeatherDays[i].Low) + " " +
			String(WeatherDays[i].Text));
	}
}

void loop() { // this will never run!
	Serial.println("UH OH IN LOOP!");
	delay(3000);
	yield;
	esp_sleep_enable_timer_wakeup(UpdateInterval);
	esp_deep_sleep_start(); // Sleep for e.g. 30 minutes
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
	Serial.print(F("\r\nConnecting to: ")); Serial.println(String(ssid1));
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid1, password1);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500); Serial.print(F("."));
		if (connAttempts > 5) {
			// wifi prolly down, try again later
			esp_sleep_enable_timer_wakeup(UpdateInterval);
			esp_deep_sleep_start(); 
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
	wifisection = millis() - wifisection;
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
			if (scale  > 30) {
				gfx.drawLine(dxo + x + 0, dyo + y, dxi + x + 0, dyi + y, GxEPD_BLACK);
				gfx.drawLine(dxo + x + 1, dyo + y, dxi + x + 1, dyi + y, GxEPD_BLACK);
			}
		}
		if (i == 90 || i == 270) {
			gfx.drawLine(dxo + x, dyo + y - 1, dxi + x, dyi + y - 1, GxEPD_BLACK);
			if (scale  > 30) {
				gfx.drawLine(dxo + x, dyo + y + 0, dxi + x, dyi + y + 0, GxEPD_BLACK);
				gfx.drawLine(dxo + x, dyo + y + 1, dxi + x, dyi + y + 1, GxEPD_BLACK);
			}
		}
		if (i == 45 || i == 135 || i == 225 || i == 315) {
			gfx.drawLine(dxo + x - 1, dyo + y, dxi + x - 1, dyi + y, GxEPD_BLACK);
			if (scale  > 30) {
				gfx.drawLine(dxo + x + 0, dyo + y, dxi + x + 0, dyi + y, GxEPD_BLACK);
				gfx.drawLine(dxo + x + 1, dyo + y, dxi + x + 1, dyi + y, GxEPD_BLACK);
			}
		}
	}
}