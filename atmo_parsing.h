#pragma once


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
	//pp(oldTime);
	//pp(newTime);
	int64_t timeDriftSeconds = newTime - oldTime; // negative if i woke up early, bc oldtime was too fast
	SleepDriftWasTooFast = (timeDriftSeconds < 0);
	if (SleepDriftWasTooFast)
		timeDriftSeconds *= -1;
	if (oldTime < 1579461559)
		timeDriftSeconds = 0;	// TODO make this whole thing not terrible, set time in hard wake eg
	SleepDriftCompensation = (uint64_t)timeDriftSeconds * OneSecond;
}


void ParseWeatherAndTime(String currentWeatherString)
{
	DynamicJsonDocument weatherCurrentDoc(12000);
	DeserializationError error = deserializeJson(weatherCurrentDoc, currentWeatherString); //optimize doc size

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


void ParseGeoLocation(String geoString)
{
	// Enough space for:
			// + 1 object with 3 members
			// + 2 objects with 1 member
			//const int capacity = JSON_OBJECT_SIZE(3) + 2 * JSON_OBJECT_SIZE(1);
			//StaticJsonDocument<capacity> doc;
			// TODO look into this for making exactly sized JSON docs
			//"Of course, if the JsonDocument were bigger, it would make sense to move it the heap" (<- from PDF)
	DynamicJsonDocument geoDoc(900);
	DeserializationError error = deserializeJson(geoDoc, geoString);  //optimize doc size
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
