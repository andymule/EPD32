#pragma once

GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> gfx(GxEPD2_290(/*CS=*/ 5, /*DC=*/ 19, /*RST=*/ 12, /*BUSY=*/ 4));
const GFXfont* font9 = &FreeSans9pt7b;
const GFXfont* font12 = &FreeSans12pt7b;

class Pattern
{
public:
	Pattern() : width(0), height(0), pattern(NULL) {}
	Pattern(int w, int h) : width(w), height(h), pattern(NULL) {}
	int width;
	int height;
	const uint8_t* pattern;
};

Pattern PatternSparseDots(4, 4);
const uint8_t _sparseDots[] = {
	1,0,0,0,
	0,0,0,0,
	0,0,0,0,
	0,0,0,0,
};

Pattern PatternZigZagH(10, 8);
const uint8_t _zigZagH[] = {
	0,0,0,0,1,0,0,0,0,0,
	0,0,0,1,0,1,0,0,0,0,
	0,0,1,0,0,0,1,0,0,0,
	0,1,0,0,0,0,0,1,0,0,
	1,0,0,0,0,0,0,0,1,0,
	0,0,0,0,0,0,0,0,0,1,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
};

void MakePatterns()
{
	PatternSparseDots.pattern = _sparseDots;
	PatternZigZagH.pattern = _zigZagH;
}

bool PatternPixelSample(const Pattern& p, int x, int y)
{
	return p.pattern[(y % p.height) * p.width + x % p.width] == 1;
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
	gfx.println("Umlaut ùùùùùùù");
	gfx.nextPage();
}

int GetTextWidth(const String& text)
{
	int16_t x1, y1;
	uint16_t w, h;
	gfx.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
	return (int)w;
}

void DrawDayOfWeek(int daysAfterToday, int width, int heightStart, int fontHeight)
{
	gfx.setFont();	// resets to default little font guy
	gfx.setTextColor(GxEPD_BLACK);
	gfx.setCursor(width*(daysAfterToday), heightStart);
	String s = WeatherDays[daysAfterToday].DayOfWeek.substring(0, 3);
	gfx.println(s);
	gfx.setCursor(width*(daysAfterToday), heightStart + fontHeight);

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

void DrawCenteredString(int fontsize, int y_pos, const String& s, int nudge)
{
	switch (fontsize)
	{
	case 9:  gfx.setFont(font9);  break;
	case 12: gfx.setFont(font12); break;
	default: gfx.setFont();       break;
	}
	int tw = GetTextWidth(s);
	gfx.setCursor(gfx.width() / 2 - tw / 2 + nudge, y_pos);
	gfx.println(s);
}

void DrawWeather()
{
	gfx.setPartialWindow(0, 0, gfx.width(), gfx.height());
	gfx.fillScreen(GxEPD_WHITE);
	int setback = 0;
	gfx.setTextColor(GxEPD_BLACK);

	gfx.setFont();
	gfx.setCursor(0, 0);
	gfx.print(Prefs.getString(PREF_CITY_STRING));

	gfx.setCursor(gfx.width() - gfx.width() / 9 + 2, 0);
	gfx.print(CurrentTime);

	gfx.setFont(font12);
	setback = GetTextWidth(String(WeatherDays[0].Low)) / 2;
	gfx.setCursor(gfx.width() / 4 - setback, 35);
	gfx.print(WeatherDays[0].Low);

	setback = GetTextWidth(String(WeatherDays[0].High)) / 2;
	gfx.setCursor(gfx.width() - gfx.width() / 4 - setback, 35);
	gfx.print(WeatherDays[0].High);

	DrawCenteredString(9, 53, TodayTempDesc, 0);
	DrawCenteredString(9, 73, TodaySky, 0);
	DrawCenteredString(9, 20, String(CurrentTemp), 0);
	DrawDaysAhead(FORECAST_DAYS);
	gfx.nextPage();
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
	gfx.nextPage();
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
	gfx.nextPage();
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
	gfx.nextPage();
}

void DrawUpdating()
{
	gfx.setFont(font9);
	gfx.setTextColor(GxEPD_BLACK);
	int16_t bx, by;
	uint16_t bw, bh;
	gfx.getTextBounds("updating", 0, 0, &bx, &by, &bw, &bh);
	int cx = gfx.width() / 2;
	int cy = 20;
	int pad = 4;
	int wx = cx - bw / 2 - pad;
	int wy = cy + by - pad;
	int ww = bw + pad * 2;
	int wh = bh + pad * 2;
	gfx.setPartialWindow(wx, wy, ww, wh);
	gfx.fillScreen(GxEPD_WHITE);
	gfx.setCursor(cx - bw / 2, cy);
	gfx.print("updating");
	gfx.nextPage();
	gfx.fillRect(wx, wy, ww, wh, GxEPD_WHITE);
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