#pragma once

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16);	// arbitrary selection of 17, 16
GxEPD_Class gfx(io, /*RST=*/ 16, /*BUSY=*/ 4);				 // arbitrary selection of (16), 4
const GFXfont* font9 = &FreeSans9pt7b;		// TODO andymule use ishac fonts
const GFXfont* font12 = &FreeSans12pt7b;

// useful tools, like bitmap converter, fonts, and font converters
// https://github.com/cesanta/arduino-drivers/tree/master/Adafruit-GFX-Library
//#include GxEPD_BitmapExamples

//const int width = 296;
//const int height = 128;

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

int HalfWidthOfText(String text, int size)
{
	return text.length()*size / 2;
}

void DrawDayOfWeek(int daysAfterToday, int width, int heightStart, int fontHeight)
{
	gfx.setFont();	// resets to default little font guy
	gfx.setTextColor(GxEPD_BLACK);
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

void DrawDaysAhead(int daysAhead)
{
	for (int i = 0; i < daysAhead; i++)
	{
		DrawDayOfWeek(i, (gfx.width() / daysAhead), 90, 9);
	}
}

void DrawDisplay()
{
	int setback = 0;
	gfx.setTextColor(GxEPD_BLACK);
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

	gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
	//gfx.update();
	//gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, true);
}

void DrawConnectionInstructions()
{
	gfx.setFont(font9);
	gfx.setTextColor(GxEPD_BLACK);
	gfx.setCursor(0, 19);
	gfx.println("Connect to ATMO WiFi network.");
	gfx.println("Then, browse to at.mo");
	gfx.println();
	gfx.println("Configure and enjoy!");
	gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
	//gfx.update();
}

void DrawFailedToConnectToSite()
{
	gfx.setFont(font9);
	gfx.setTextColor(GxEPD_BLACK);
	gfx.setCursor(0, 60 + 9);
	gfx.println("Failed to connect to sites.");
	gfx.println("Check your internet connection.");
	gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
	//gfx.update();
}

void DrawFailedToConnectToWiFi()
{
	gfx.setFont(font9);
	gfx.setTextColor(GxEPD_BLACK);
	gfx.setCursor(0, 30 + 9);
	gfx.println("Failed to connect to WiFi.");
	gfx.println("Check your router or Atmo settings.");
	gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
	//gfx.update();
}

void DrawUpdating()
{
	int setback = 0;
	gfx.setFont(font9);
	gfx.setTextColor(GxEPD_BLACK);
	setback = HalfWidthOfText("updating", 9);
	gfx.setCursor(gfx.width() / 2 - setback, 20 + 9);
	gfx.println("updating");
	gfx.updateWindow(gfx.width() / 2 - setback, 20 - 5, setback * 2, 9, true);
	gfx.fillRect(gfx.width() / 2 - setback, 20 - 3 - 5, setback * 2 + 3, 9 * 2, GxEPD_WHITE);	// cover it up though
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