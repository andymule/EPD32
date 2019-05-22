#pragma once
void GetGeolocationFromNet();
void ParseWeatherAndTime();
void ParseGeoLocation();

HTTPClient weathercurrenthttp, geolocatehttp;
// original strings:
// http://api.openweathermap.org/data/2.5/weather?&appid=ba42e4a918f7e742d3143c5e8fff9210&lat=59.3307&lon=18.0718&units=metric
//https://weather.api.here.com/weather/1.0/report.json?app_id=JoN1SBsEzJ5pWD5OkXwN&app_code=J9XdgHlHuUKzV2j5GqxlVg&product=forecast_7days_simple&latitude=59.3307&longitude=18.0718

//const String weatherCurrent = "http://api.openweathermap.org/data/2.5/weather?&appid=ba42e4a918f7e742d3143c5e8fff9210&lat=";
const String weatherCurrent = "https://weather.api.here.com/weather/1.0/report.json?app_id=JoN1SBsEzJ5pWD5OkXwN&app_code=J9XdgHlHuUKzV2j5GqxlVg&product=forecast_7days_simple&product=observation&oneobservation=true&latitude=";
const String weatherAndLon = "&longitude=";
const String weatherNoMetric = "&metric=false";	
const String weatherMetric = "&metric=true";	
const String weatherLanguage = "&language=";	// https://developer.here.com/documentation/weather/topics/supported-languages.html

const String geolocatestring = "http://api.ipstack.com/check?access_key=d0dfe9b52fa3f5bb2a5ff47ce435c7d8"; //key=ab925796fd105310f825bbdceece059e

void GetGeolocationFromNet()
{
	pp("getting geolocation from internet");
	geolocatehttp.setTimeout(SITE_TIMEOUT_MS);
	EnsureWiFiIsStarted();
	geolocatehttp.begin(geolocatestring);
	int geoHttpCode = geolocatehttp.GET();
	if (geoHttpCode == 200)
	{
		ParseGeoLocation();
	}
	else if (geoHttpCode != 200) { // FAILED TO CONNECT TO GEOLOCATE SITE
		DrawFailedToConnectToSite();	// draws to epaper
		AtmoDeepSleep();
	}
}

int RequestWeatherData()
{
	String weatherCall = weatherCurrent + prefs.getFloat(PREF_LAT_FLOAT) + weatherAndLon + prefs.getFloat(PREF_LON_FLOAT);
	if (prefs.getBool(PREF_METRIC_BOOL))
	{
		weatherCall = weatherCall + weatherMetric;
	}
	else
	{
		weatherCall = weatherCall + weatherNoMetric;
	}
	weathercurrenthttp.setTimeout(SITE_TIMEOUT_MS);
	EnsureWiFiIsStarted();
	weathercurrenthttp.begin(weatherCall); //Specify the URL
	return weathercurrenthttp.GET();
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

void SetClockAndDriftCompensate() // TODO put on second core?
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
	if (oldTime < 1579461559)
		timeDriftSeconds = 0;	// TODO make this whole thing not terrible, set time in hard wake eg
	SleepDriftCompensation = (uint64_t)timeDriftSeconds * OneSecond;
}


void ParseWeatherAndTime()
{
	DynamicJsonDocument weatherCurrentDoc(12000);
	DeserializationError error = deserializeJson(weatherCurrentDoc, weathercurrenthttp.getString()); //optimize doc size

	if (error) {
		Serial.print(F("deserializeJson() failed22: "));
		Serial.println(error.c_str());
		AtmoDeepSleep();
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
		WeatherDays[i].PrecipChance = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["precipitationProbability"];
		WeatherDays[i].SkyText = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["skyDescription"];
		WeatherDays[i].PrecipText = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["precipitationDesc"];
	}

	TodayHigh = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["highTemperature"];
	TodayLow = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["lowTemperature"];
	TodaySky = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["skyDescription"].as<String>();
	TodayTempDesc = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["temperatureDesc"].as<String>();
	//TodayTempDesc.toLowerCase();
	//TodaySky.toLowerCase();
	CurrentTemp = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["temperature"];
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
		AtmoDeepSleep();
	}
	String city = geoDoc["city"].as<String>();
	city.replace(" ", "%20");
	prefs.putBool(PREF_VALID_BOOL, true);
	prefs.putFloat(PREF_LAT_FLOAT, geoDoc["latitude"]);
	prefs.putFloat(PREF_LON_FLOAT, geoDoc["longitude"]);
	prefs.putString(PREF_CITY_STRING, city);
}
