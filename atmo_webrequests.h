#pragma once
void GetGeolocationFromNet();
void ParseWeatherAndTime();
void ParseGeoLocation();

const int HTTP_MAX_RETRIES = 3;
const int HTTP_RETRY_DELAY_MS = 1000;

HTTPClient weathercurrenthttp, geolocatehttp;
const char* weatherCurrent = "https://weather.api.here.com/weather/1.0/report.json?app_id=JoN1SBsEzJ5pWD5OkXwN&app_code=J9XdgHlHuUKzV2j5GqxlVg&product=forecast_7days_simple&product=observation&oneobservation=true&latitude=";
const char* weatherAndLon = "&longitude=";
const char* weatherNoMetric = "&metric=false";
const char* weatherMetric = "&metric=true";

const char* geolocatestring = "http://api.ipstack.com/check?access_key=d0dfe9b52fa3f5bb2a5ff47ce435c7d8&fields=city,latitude,longitude";

void GetGeolocationFromNet()
{
	pp("getting geolocation from internet");
	EnsureWiFiIsStarted();
	geolocatehttp.setTimeout(SITE_TIMEOUT_MS);
	int geoHttpCode = -1;
	for (int attempt = 0; attempt < HTTP_MAX_RETRIES; attempt++) {
		geolocatehttp.begin(geolocatestring);
		geoHttpCode = geolocatehttp.GET();
		if (geoHttpCode == 200) break;
		geolocatehttp.end();
		pp("Geo attempt " + String(attempt + 1) + " failed: " + String(geoHttpCode));
		delay(HTTP_RETRY_DELAY_MS);
	}
	if (geoHttpCode == 200) {
		ParseGeoLocation();
		pp("got location:" + Prefs.getString(PREF_CITY_STRING));
	} else {
		DrawFailedToConnectToSite();
		AtmoDeepSleep();
	}
}

int RequestWeatherData()
{
	String weatherCall = weatherCurrent;
	weatherCall += Prefs.getFloat(PREF_LAT_FLOAT);
	weatherCall += weatherAndLon;
	weatherCall += Prefs.getFloat(PREF_LON_FLOAT);
	weatherCall += Prefs.getBool(PREF_METRIC_BOOL) ? weatherMetric : weatherNoMetric;
	pp(weatherCall);
	EnsureWiFiIsStarted();
	weathercurrenthttp.setTimeout(SITE_TIMEOUT_MS);
	int httpCode = -1;
	for (int attempt = 0; attempt < HTTP_MAX_RETRIES; attempt++) {
		weathercurrenthttp.begin(weatherCall);
		weathercurrenthttp.addHeader("Accept-Encoding", "gzip");
		httpCode = weathercurrenthttp.GET();
		if (httpCode == 200) break;
		weathercurrenthttp.end();
		pp("Weather attempt " + String(attempt + 1) + " failed: " + String(httpCode));
		delay(HTTP_RETRY_DELAY_MS);
	}
	return httpCode;
}



void ParseCurrentTimeForDisplay(int timezone)
{
	int startI = CurrentTime.indexOf('T');
	int hourI = CurrentTime.indexOf(':');
	int minuteI = hourI + 3;
	int hour = CurrentTime.substring(startI + 1, hourI).toInt() + timezone;
	String minutes = CurrentTime.substring(hourI + 1, minuteI);
	CurrentTime = String(hour) + ":" + minutes;
	if (CurrentTime.length() < 5)
		CurrentTime = " " + CurrentTime;	// prepend whitespace to keep time in the corner
}

void SetClockAndDriftCompensate()
{	// CurrentTime should be freshly parsed from JSON at this point
	// Format: YYYY-MM-DDThh:mm:ssZ  (ISO 8601)
	int YYYY = CurrentTime.substring(0, 4).toInt();
	int MM = CurrentTime.substring(5, 7).toInt();
	int DD = CurrentTime.substring(8, 10).toInt();
	int hh = CurrentTime.substring(11, 13).toInt();
	int mm = CurrentTime.substring(14, 16).toInt();
	int ss = CurrentTime.substring(17, 19).toInt();
	tmElements_t tm;
	tm.Year = CalendarYrToTm(YYYY);
	tm.Month = MM;
	tm.Day = DD;
	tm.Hour = hh;
	tm.Minute = mm;
	tm.Second = ss;
	time_t newTime = makeTime(tm);
	setTime(newTime);

	// Drift = actual elapsed time vs intended sleep duration
	const time_t JAN_20_2020 = 1579461559;
	if (lastServerTime > JAN_20_2020 && lastSleepDuration > 0)
	{
		int64_t actualElapsedSec = (int64_t)(newTime - lastServerTime);
		int64_t intendedElapsedSec = (int64_t)(lastSleepDuration / OneSecond);
		int64_t driftSeconds = actualElapsedSec - intendedElapsedSec;
		SleepDriftWasTooFast = (driftSeconds < 0);
		if (SleepDriftWasTooFast)
			driftSeconds *= -1;
		SleepDriftCompensation = (uint64_t)driftSeconds * OneSecond;
	}
	lastServerTime = newTime;
}


void ParseWeatherAndTime()
{
	DynamicJsonDocument weatherCurrentDoc(12000);
	DeserializationError error = deserializeJson(weatherCurrentDoc, weathercurrenthttp.getString());

	if (error) {
		pp("Weather JSON parse failed: " + String(error.c_str()));
		AtmoDeepSleep();
	}
	String ObservationTime = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["utcTime"].as<String>();
	int tI = ObservationTime.indexOf('T');
	int plusI = ObservationTime.indexOf('+', tI);
	int minusI = ObservationTime.indexOf('-', tI + 1);
	int timezone = 0;
	if (plusI > tI) {
		int colonI = ObservationTime.indexOf(':', plusI);
		timezone = ObservationTime.substring(plusI + 1, colonI).toInt();
	} else if (minusI > tI) {
		int colonI = ObservationTime.indexOf(':', minusI);
		timezone = -ObservationTime.substring(minusI + 1, colonI).toInt();
	}
	CurrentTime = weatherCurrentDoc["feedCreation"].as<String>();
	SetClockAndDriftCompensate();
	ParseCurrentTimeForDisplay(timezone);

	for (int i = 0; i < FORECAST_DAYS; i++)
	{
		WeatherDays[i].DayOfWeek = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["weekday"].as<String>();
		WeatherDays[i].High = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["highTemperature"];
		WeatherDays[i].Low = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["lowTemperature"];
		WeatherDays[i].PrecipChance = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["precipitationProbability"];
		WeatherDays[i].SkyText = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["skyDescription"].as<String>();
		WeatherDays[i].PrecipText = weatherCurrentDoc["dailyForecasts"]["forecastLocation"]["forecast"][i]["precipitationDesc"].as<String>();
	}

	TodaySky = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["skyDescription"].as<String>();
	TodayTempDesc = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["temperatureDesc"].as<String>();
	CurrentTemp = weatherCurrentDoc["observations"]["location"][0]["observation"][0]["temperature"];
}


void ParseGeoLocation()
{
	StaticJsonDocument<200> geoDoc;
	DeserializationError error = deserializeJson(geoDoc, geolocatehttp.getString());
	if (error) {
		pp("Geolocation JSON parse failed: " + String(error.c_str()));
		AtmoDeepSleep();
	}
	String city = geoDoc["city"].as<String>();
	city.replace(" ", "%20");
	pp(city);
	Prefs.putBool(PREF_VALID_BOOL, true);
	Prefs.putFloat(PREF_LAT_FLOAT, geoDoc["latitude"]);
	Prefs.putFloat(PREF_LON_FLOAT, geoDoc["longitude"]);
	Prefs.putString(PREF_CITY_STRING, city);
}
