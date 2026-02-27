#pragma once
Preferences Prefs;	// used to save and load from memory
#define PREF_VALID_BOOL "valid"
#define PREF_METRIC_BOOL "useMetric"
#define PREF_LAT_FLOAT "lat"
#define PREF_LON_FLOAT "lon"
#define PREF_CITY_STRING "city"
#define PREF_PASSWORD_STRING "wifi_password"
#define PREF_SSID_STRING "wifi_ssid"

class WeatherDay
{
public:
	int High;
	int Low;
	int PrecipChance;
	String SkyText;
	String PrecipText;
	String DayOfWeek;
};
const int FORECAST_DAYS = 6;
WeatherDay WeatherDays[FORECAST_DAYS] = {};

String CurrentTime;
int CurrentTemp;
String TodaySky;
String TodayTempDesc;

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