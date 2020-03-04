#pragma once
// getTextBounds
GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> gfx(GxEPD2_290(/*CS=5*/ 5, /*DC=*/ 19, /*RST=*/ 12, /*BUSY=*/ 4));
const GFXfont* font9 = &FreeSans9pt7b;		// TODO andymule use ishac fonts
const GFXfont* font12 = &FreeSans12pt7b;

// useful tools, like bitmap converter, fonts, and font converters
// https://github.com/cesanta/arduino-drivers/tree/master/Adafruit-GFX-Library
// showFont("FreeMonoBold9pt7b", &FreeMonoBold9pt7b); // draws all chars

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

class Pattern;

class Pattern
{
public:
	Pattern() {}
	Pattern(int w, int h)
	{
		width = w;
		height = h;
	}
	int height;
	int width;
	int* pattern;
};

Pattern PatternSparseDots(4, 4);
int _sparseDots[] =
{
	1,0,0,0,
	0,0,0,0,
	0,0,0,0,
	0,0,0,0,
};

Pattern PatternZigZagH(10, 8);
int _zigZagH[] =
{
	0,0,0,0,1,0,0,0,0,0,
	0,0,0,1,0,1,0,0,0,0,
	0,0,1,0,0,0,1,0,0,0,
	0,1,0,0,0,0,0,1,0,0,
	1,0,0,0,0,0,0,0,1,0,
	0,0,0,0,0,0,0,0,0,1,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
};

void MakePatterns() // srsly dont know why this is necessary but whatever not a huge deal - can't do global for some reason
{
	PatternSparseDots.pattern = _sparseDots;
	PatternZigZagH.pattern = _zigZagH;
}

bool PatternPixelSample(Pattern p, int x, int y) // returns true if a pixel is here on our pattern
{
	return p.pattern[(y%p.height)*p.width + x % p.width] == 1;
}

void DrawPattern(Pattern p, int yStart)
{
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	for (int i = 0; i < gfx.width(); i += 1)
	{
		for (int j = yStart; j < gfx.height(); j += 1)
		{
			if (PatternPixelSample(p, i, j)) // if pixel is in pattern
				gfx.drawPixel(i, j, GxEPD_BLACK);
		}
	}
	gfx.nextPage();
}

void DrawPatternUnderSpline(Pattern p)
{
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	gfx.setTextColor(GxEPD_BLACK);
	//gfx.fillScreen(GxEPD_WHITE);
	MakePatterns();
	std::vector<double> X(10), Y(10);
	for (int i = 0; i < 10; i++)
	{
		X[i] = gfx.width() / 10 * i;
		if (i % 2 == 1)
			Y[i] = gfx.height();
		else
			Y[i] = gfx.height() / 10 * i;
	}
	tk::spline s;
	//s.set_boundary(tk::spline::second_deriv, 0.0, tk::spline::first_deriv, -2.0, true);
	s.set_points(X, Y);    // currently it is required that X is already sorted

	for (float x1 = 0; x1 < gfx.width(); x1 += 1)
	{
		int y1 = s(x1);
		int x2 = x1 + 1;
		int y2 = s(x2);
		gfx.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
	}

	for (int i = 0; i < gfx.width(); i += 1)
	{
		for (int j = 0; j < gfx.height(); j += 1)
		{
			if (PatternPixelSample(p, i, j)) // if pixel is in pattern
				if (j > (s(i)))	// and if it's below our spline line
					gfx.drawPixel(i, j, GxEPD_BLACK);
		}
	}
	gfx.nextPage();
}

void DrawSplines() // drawing experiement to be resumed
{
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	gfx.setTextColor(GxEPD_BLACK);
	gfx.fillScreen(GxEPD_WHITE);

	std::vector<double> X(10), Y(10);
	for (int i = 0; i < 10; i++)
	{
		X[i] = gfx.width() / 10 * i;
		if (i % 2 == 1)
			Y[i] = gfx.height();
		else
			Y[i] = gfx.height() / 10 * i;
	}
	tk::spline s;
	//s.set_boundary(tk::spline::second_deriv, 0.0, tk::spline::first_deriv, -2.0, true);
	s.set_points(X, Y);    // currently it is required that X is already sorted

	for (float x1 = 0; x1 < gfx.width(); x1 += 1)
	{
		int y1 = s(x1);
		int x2 = x1 + 1;
		int y2 = s(x2);
		gfx.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
	}
	gfx.nextPage();
}

void DrawSpecks()	// drawing experiement to be resumed
{
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	gfx.setTextColor(GxEPD_BLACK);
	gfx.fillScreen(GxEPD_WHITE);
	for (int i = 0; i < gfx.width(); i += 1)
	{
		for (int j = (i*i) % 10; j < gfx.height(); j += 10)
		{
			gfx.drawPixel(i, j, GxEPD_BLACK);
		}
	}
	gfx.nextPage();
}

void DrawSpecksUnderSpline()
{
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	gfx.setTextColor(GxEPD_BLACK);
	gfx.fillScreen(GxEPD_WHITE);

	std::vector<double> X(10), Y(10);
	for (int i = 0; i < 10; i++)
	{
		X[i] = gfx.width() / 10 * i;
		if (i % 2 == 1)
			Y[i] = gfx.height();
		else
			Y[i] = gfx.height() / 10 * i;
	}
	tk::spline s;
	//s.set_boundary(tk::spline::second_deriv, 0.0, tk::spline::first_deriv, -2.0, true);
	s.set_points(X, Y);    // currently it is required that X is already sorted

	for (float x1 = 0; x1 < gfx.width(); x1 += 1)
	{
		int y1 = s(x1);
		int x2 = x1 + 1;
		int y2 = s(x2);
		gfx.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
	}

	for (int i = 0; i < gfx.width(); i += 1)
	{
		for (int j = (i*i) % 10; j < gfx.height(); j += 10)
		{
			if (j > (s(i)))	// if under the line, draw the pattern
				gfx.drawPixel(i, j, GxEPD_BLACK);
		}
	}
	gfx.nextPage();
}

void DrawFont(const GFXfont* font)
{
	gfx.setRotation(2);
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	gfx.fillScreen(GxEPD_WHITE);
	gfx.setTextColor(GxEPD_BLACK);
	gfx.setCursor(0, 0);
	gfx.println();
	gfx.setFont(font);
	gfx.println(" !\"#$%&'()*+,-./");
	gfx.println("0123456789:;<=>?");
	gfx.println("@ABCDEFGHIJKLMNO");
	gfx.println("PQRSTUVWXYZ[\\]^_");
	gfx.println("`abcdefghijklmno");
	gfx.println("pqrstuvwxyz{|}~ ");
	gfx.println("Umlaut ÄÖÜäéöü");
	gfx.nextPage();
}

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

	//String CheckText = String(WeatherDays[daysAfterToday].SkyText);
	//String CheckText = String(WeatherDays[daysAfterToday].PrecipText);
	//String PrintText = String(CheckText);	// TODO smarter way to display just one word, need a real parser
	//if (CheckText.indexOf(" ") > 0)
	//{
		//PrintText = CheckText.substring(CheckText.lastIndexOf(" ") + 1, CheckText.length());
	//}
	String PrecipChanceString = String(WeatherDays[daysAfterToday].PrecipChance);
	gfx.println(PrecipChanceString + "%");

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

void DrawCenteredString(int fontsize, int y_pos, String s, int nudge) // requires color set beforehand
{
	switch (fontsize)
	{
	case 9:
		gfx.setFont(font9);
		break;
	case 12:
		gfx.setFont(font12);
		break;
	default:
		fontsize = 8;
		gfx.setFont();
		break;
	}
	int setback = HalfWidthOfText(s, fontsize);
	gfx.setCursor(gfx.width() / 2 - setback + nudge, y_pos);
	gfx.println(s);
}

void DrawWeather()
{
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	gfx.fillScreen(GxEPD_WHITE);
	int setback = 0;
	gfx.setTextColor(GxEPD_BLACK);
	// city in top left
	gfx.setFont();	// uses tiny default font
	gfx.setCursor(0, 0);
	gfx.print( Prefs.getString(PREF_CITY_STRING) );

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
	DrawCenteredString(9, 53, TodayTempDesc, 0);
	DrawCenteredString(9, 73, TodaySky, 0);
	DrawCenteredString(9, 20, String(CurrentTemp), 0);
	DrawDaysAhead(6);
	//gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
	gfx.nextPage();// TODO partial update
}

void DrawConnectionInstructions()
{
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	gfx.fillScreen(GxEPD_WHITE);
	gfx.setFont(font9);
	gfx.setTextColor(GxEPD_BLACK);
	gfx.setCursor(0, 19);
	gfx.println("Connect to ATMO WiFi network.");
	gfx.println("Then, browse to at.mo");
	gfx.println();
	gfx.println("Configure and enjoy!");
	//gfx.update
	//gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
	gfx.nextPage();// TODO partial update
}

void DrawFailedToConnectToSite()
{
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	gfx.fillScreen(GxEPD_WHITE);
	gfx.setFont(font9);
	gfx.setTextColor(GxEPD_BLACK);
	gfx.setCursor(0, 60 + 9);
	gfx.println("Failed to connect to sites.");
	gfx.println("Check your internet connection.");
	//gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
	gfx.nextPage();// TODO partial update
}

void DrawFailedToConnectToWiFi()
{
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	gfx.fillScreen(GxEPD_WHITE);
	gfx.setFont(font9);
	gfx.setTextColor(GxEPD_BLACK);
	gfx.setCursor(0, 30 + 9);
	gfx.println("Failed to connect to WiFi.");
	gfx.println("Check your router or Atmo settings.");
	//gfx.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
	gfx.nextPage();// TODO partial update
}

void DrawUpdating()
{
	int fontsize = 9;
	int startpoint = 20;
	int setback = 36;
	gfx.setPartialWindow(gfx.width() / 2 - setback - 15, startpoint - 15, setback * 2 + 22, 9 + 4);
	gfx.fillScreen(GxEPD_WHITE);
	gfx.setTextColor(GxEPD_BLACK);
	DrawCenteredString(fontsize, startpoint, "updating", 0);
	setback = HalfWidthOfText("updating", fontsize);
	//gfx.fillRect(gfx.width() / 2 - setback, startpoint - 15, setback * 2 + 3, 9 + 4 * 2, GxEPD_BLACK);	// cover it up though
	//gfx.updateWindow(gfx.width() / 2 - setback, startpoint - 5, setback * 2, 9+4, true);
	//pp("SETBACK");
	//pp(setback);
	//gfx.
	gfx.nextPage();	// TODO partial update
	gfx.fillRect(gfx.width() / 2 - setback - 7, startpoint - 5, setback * 2 + 15, 9 + 4, GxEPD_WHITE);	// cover it up though
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