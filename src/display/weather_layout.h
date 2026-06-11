// Resolution-independent weather layout, shared verbatim between the desktop
// preview harness and the device. Templated on the GFX object (HostGFX or the
// GxEPD2/Adafruit_GFX panel), with fonts and the data view injected so the file
// has no Arduino/STL dependency. Every coordinate is derived from width()/
// height(), so the same code adapts to 296x128, 200x200, 800x480, etc.
#pragma once
#include <stdint.h>
#include <stdio.h>

#include "gfxfont.h"

#ifndef GxEPD_BLACK
#define GxEPD_BLACK 0x0000
#define GxEPD_WHITE 0xFFFF
#endif

namespace atmo {

// ---- Injected resources -----------------------------------------------------

// Fonts ordered small->large. Provide bold + regular at each step; the layout
// picks per element by fitting to the available box.
struct FontSet {
  const GFXfont* f9;
  const GFXfont* f9b;
  const GFXfont* f12;
  const GFXfont* f12b;
  const GFXfont* f18;
  const GFXfont* f18b;
  const GFXfont* f24;
  const GFXfont* f24b;
};

struct DayView {
  const char* label;  // "Mon"
  int code;           // WMO weather code
  int hi;
  int lo;
  int precip;         // %
};

struct WeatherView {
  const char* city;
  const char* date;        // preformatted, e.g. "Wed Nov 3"
  const char* condition;   // e.g. "Drizzle"
  int temperature;
  int code;                // current WMO code
  const DayView* days;
  int dayCount;
  const int* hourly;       // next ~24h temperatures (may be null)
  int hourlyCount;
};

// ---- Small helpers (no STL: Arduino redefines min/max as macros) ------------

inline int imin(int a, int b) { return a < b ? a : b; }
inline int imax(int a, int b) { return a > b ? a : b; }
inline int iclamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct Pick {
  const GFXfont* font;
  uint8_t size;
};

template <class G>
inline void measure(G& g, const GFXfont* f, uint8_t sz, const char* s, int* w, int* h) {
  g.setFont(f);
  g.setTextSize(sz);
  int16_t x1, y1;
  uint16_t ww, hh;
  g.getTextBounds(s, 0, 0, &x1, &y1, &ww, &hh);
  *w = ww;
  *h = hh;
}

// Largest (font,size) from `cands` whose rendered text fits maxW x maxH.
template <class G>
inline Pick fitText(G& g, const Pick* cands, int n, const char* s, int maxW, int maxH) {
  for (int i = 0; i < n; i++) {
    int w, h;
    measure(g, cands[i].font, cands[i].size, s, &w, &h);
    if (w <= maxW && h <= maxH) return cands[i];
  }
  return cands[n - 1];
}

// Draw text with alignment. ax: 0=left,1=center,2=right around `x`. Vertically
// centered on `cy`. Uses the glyph bounds so it's exact for any font/size.
template <class G>
inline void drawText(G& g, const GFXfont* f, uint8_t sz, int x, int cy, int ax,
                     const char* s, uint16_t color) {
  g.setFont(f);
  g.setTextSize(sz);
  int16_t x1, y1;
  uint16_t w, h;
  g.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  int cursorX = x - x1;
  if (ax == 1) cursorX = x - x1 - w / 2;
  else if (ax == 2) cursorX = x - x1 - w;
  int baselineY = cy - y1 - h / 2;
  g.setTextColor(color);
  g.setCursor(cursorX, baselineY);
  g.print(s);
}

// ---- Weather icons (thin-line outline style; centered in a box `s`) ---------

template <class G>
inline void thickLine(G& g, int x0, int y0, int x1, int y1, int t, uint16_t c) {
  for (int o = -(t / 2); o <= t / 2; o++) {
    g.drawLine(x0 + o, y0, x1 + o, y1, c);
    g.drawLine(x0, y0 + o, x1, y1 + o, c);
  }
}

inline int strokeW(int s) { return imax(2, s * 8 / 100); }

template <class G>
inline void iconSun(G& g, int cx, int cy, int s, uint16_t c) {
  int sw = strokeW(s);
  int r = s * 22 / 100;
  g.fillCircle(cx, cy, r, c);                 // ring (outer)
  g.fillCircle(cx, cy, r - sw, GxEPD_WHITE);  // hollow centre
  int ri = r + s * 10 / 100, ro = r + s * 24 / 100;
  static const int dirs[8][2] = {{0, -10}, {0, 10}, {-10, 0}, {10, 0},
                                 {-7, -7}, {7, -7}, {-7, 7}, {7, 7}};
  for (int i = 0; i < 8; i++) {
    int dx = dirs[i][0], dy = dirs[i][1];
    thickLine(g, cx + dx * ri / 10, cy + dy * ri / 10, cx + dx * ro / 10,
              cy + dy * ro / 10, sw, c);
  }
}

// Filled cloud blob centered at (cx,cy); coords scale linearly with s so that an
// inset (smaller-s) white copy leaves a uniform outline.
template <class G>
inline void cloudBlob(G& g, int cx, int cy, int s, uint16_t c) {
  int yb = cy + s * 14 / 100;
  g.fillCircle(cx - s * 24 / 100, yb - s * 16 / 100, s * 16 / 100, c);
  g.fillCircle(cx + s * 26 / 100, yb - s * 18 / 100, s * 18 / 100, c);
  g.fillCircle(cx, yb - s * 24 / 100, s * 22 / 100, c);
  g.fillRoundRect(cx - s * 46 / 100, yb - s * 16 / 100, s * 92 / 100, s * 16 / 100,
                  s * 8 / 100, c);
}

template <class G>
inline void iconCloud(G& g, int cx, int cy, int s, uint16_t c) {
  int sw = strokeW(s);
  cloudBlob(g, cx, cy, s, c);
  cloudBlob(g, cx, cy, s - sw * 5 / 2, GxEPD_WHITE);  // hollow out -> outline
}

template <class G>
inline void rainStreaks(G& g, int cx, int cy, int s, uint16_t c, bool snow) {
  int y0 = cy + s * 22 / 100;
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * s * 20 / 100;
    if (snow)
      g.fillCircle(x, y0 + s * 6 / 100, imax(1, s * 5 / 100), c);
    else
      thickLine(g, x + s * 4 / 100, y0, x - s * 4 / 100, y0 + s * 18 / 100,
                imax(2, s * 7 / 100), c);
  }
}

template <class G>
inline void iconRain(G& g, int cx, int cy, int s, uint16_t c) {
  iconCloud(g, cx, cy - s * 16 / 100, s * 82 / 100, c);
  rainStreaks(g, cx, cy, s, c, false);
}

template <class G>
inline void iconSnow(G& g, int cx, int cy, int s, uint16_t c) {
  iconCloud(g, cx, cy - s * 16 / 100, s * 82 / 100, c);
  rainStreaks(g, cx, cy, s, c, true);
}

template <class G>
inline void iconStorm(G& g, int cx, int cy, int s, uint16_t c) {
  iconCloud(g, cx, cy - s * 16 / 100, s * 82 / 100, c);
  int t = imax(2, s * 8 / 100);
  thickLine(g, cx + s * 6 / 100, cy + s * 12 / 100, cx - s * 9 / 100, cy + s * 28 / 100, t, c);
  thickLine(g, cx - s * 9 / 100, cy + s * 28 / 100, cx + s * 9 / 100, cy + s * 28 / 100, t, c);
  thickLine(g, cx + s * 9 / 100, cy + s * 28 / 100, cx - s * 4 / 100, cy + s * 44 / 100, t, c);
}

template <class G>
inline void iconFog(G& g, int cx, int cy, int s, uint16_t c) {
  iconCloud(g, cx, cy - s * 18 / 100, s * 80 / 100, c);
  int t = imax(2, s * 7 / 100);
  int y0 = cy + s * 18 / 100;
  for (int i = 0; i < 3; i++) {
    int w = s * (56 - i * 8) / 100;
    g.fillRoundRect(cx - w / 2, y0 + i * (t + imax(2, s / 16)), w, t, t / 2, c);
  }
}

template <class G>
inline void iconPartly(G& g, int cx, int cy, int s, uint16_t c) {
  iconSun(g, cx + s * 18 / 100, cy - s * 18 / 100, s * 58 / 100, c);
  iconCloud(g, cx - s * 8 / 100, cy + s * 10 / 100, s * 88 / 100, c);
}

template <class G>
inline void drawIcon(G& g, int cx, int cy, int s, int code, uint16_t c) {
  switch (code) {
    case 0: case 1: iconSun(g, cx, cy, s, c); break;
    case 2: iconPartly(g, cx, cy, s, c); break;
    case 3: iconCloud(g, cx, cy, s, c); break;
    case 45: case 48: iconFog(g, cx, cy, s, c); break;
    case 51: case 53: case 55: case 56: case 57:
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82: iconRain(g, cx, cy, s, c); break;
    case 71: case 73: case 75: case 77: case 85: case 86:
      iconSnow(g, cx, cy, s, c); break;
    case 95: case 96: case 99: iconStorm(g, cx, cy, s, c); break;
    default: iconCloud(g, cx, cy, s, c); break;
  }
}

// Degree ring drawn to the upper-right of a number ending at (rightX, topY).
template <class G>
inline void drawDegree(G& g, int rightX, int topY, int r, uint16_t c) {
  int t = imax(1, r / 2);
  g.fillCircle(rightX, topY + r, r, c);
  g.fillCircle(rightX, topY + r, r - t, GxEPD_WHITE);
}

// Hourly temperature trend: a thin axis with hour ticks and the curve below,
// auto-scaled to the data. Inspired by the reference design's day curve.
template <class G>
inline void drawTrend(G& g, int x, int y, int w, int h, const int* t, int n) {
  if (!t || n < 2 || w < 8 || h < 10) return;
  int mn = t[0], mx = t[0];
  for (int i = 1; i < n; i++) { mn = imin(mn, t[i]); mx = imax(mx, t[i]); }
  if (mx - mn < 1) mx = mn + 1;

  int axisY = y + h * 20 / 100;
  int cTop = y + h * 32 / 100;
  int cBot = y + h - imax(1, h / 16);  // margin so the curve/dot don't bleed below
  int cH = cBot - cTop;
  if (cH < 3) return;

  g.drawFastHLine(x, axisY, w, GxEPD_BLACK);
  for (int hh = 0; hh < n; hh += 6) {        // tick every 6 hours
    int px = x + (w - 1) * hh / (n - 1);
    g.drawFastVLine(px, axisY - 2, 5, GxEPD_BLACK);
  }

  int lineT = iclamp(h / 40, 2, 4);  // slim curve regardless of band height
  int prevX = 0, prevY = 0, firstY = 0;
  for (int i = 0; i < n; i++) {
    int px = x + (w - 1) * i / (n - 1);
    int py = cTop + (cH - 1) - (t[i] - mn) * (cH - 1) / (mx - mn);
    if (i == 0) firstY = py;
    if (i > 0) thickLine(g, prevX, prevY, px, py, lineT, GxEPD_BLACK);
    prevX = px;
    prevY = py;
  }
  g.fillCircle(x, firstY, iclamp(h / 28, 2, 5), GxEPD_BLACK);  // "now" marker
}

// Draws an integer temperature centered at (cx,cy) with a trailing degree ring,
// the whole number+ring block centered horizontally.
template <class G>
inline void drawTempBlock(G& g, const GFXfont* f, uint8_t sz, int cx, int cy, int value) {
  char b[8];
  snprintf(b, sizeof(b), "%d", value);
  int w, h;
  measure(g, f, sz, b, &w, &h);
  int degR = imax(2, h * 12 / 100);
  int gap = imax(1, degR / 2);
  int numCX = cx - (gap + 2 * degR) / 2;
  drawText(g, f, sz, numCX, cy, 1, b, GxEPD_BLACK);
  int numLeft = numCX - w / 2;
  int numTop = cy - h / 2;
  drawDegree(g, numLeft + w + gap + degR, numTop, degR, GxEPD_BLACK);
}

// ---- Composable regions -----------------------------------------------------

template <class G>
inline void drawHeader(G& g, int x, int y, int w, int h, const WeatherView& v, const FontSet& F) {
  const Pick hc[] = {{F.f24, 2}, {F.f24, 1}, {F.f18, 1}, {F.f12, 1}, {F.f9, 1}};
  int cy = y + h / 2;
  Pick p = fitText(g, hc, 5, v.city, w * 60 / 100, h - 6);
  int cw, dh;
  measure(g, p.font, p.size, v.city, &cw, &dh);
  drawText(g, p.font, p.size, x, cy, 0, v.city, GxEPD_BLACK);
  if (v.date && v.date[0]) {
    Pick pd = fitText(g, hc, 5, v.date, w * 38 / 100, h - 6);
    int dw;
    measure(g, pd.font, pd.size, v.date, &dw, &dh);
    // Only show the date if it won't collide with a long city name.
    if (cw + dw + imax(4, w / 40) <= w)
      drawText(g, pd.font, pd.size, x + w, cy, 2, v.date, GxEPD_BLACK);
  }
  g.fillRect(x, y + h - 1, w, imax(1, h / 22), GxEPD_BLACK);
}

// Current conditions block. Arranges itself by box aspect: a tall box stacks
// icon / temperature / condition vertically; a wide box puts the icon left and
// the temperature + condition to its right.
template <class G>
inline void drawHeroBlock(G& g, int x, int y, int w, int h, const WeatherView& v, const FontSet& F) {
  const Pick tempC[] = {{F.f24b, 4}, {F.f24b, 3}, {F.f24b, 2}, {F.f24b, 1}, {F.f18b, 1}, {F.f12b, 1}};
  const Pick condC[] = {{F.f18, 2}, {F.f18, 1}, {F.f12, 1}, {F.f9, 1}};
  char tbuf[8];
  snprintf(tbuf, sizeof(tbuf), "%d", v.temperature);

  if (h * 10 >= w * 9) {  // tall-ish box -> vertical stack
    int iconBox = imin(w * 62 / 100, h * 30 / 100);
    drawIcon(g, x + w / 2, y + h * 19 / 100, iconBox, v.code, GxEPD_BLACK);
    Pick tp = fitText(g, tempC, 6, tbuf, w * 84 / 100, h * 32 / 100);
    drawTempBlock(g, tp.font, tp.size, x + w / 2, y + h * 56 / 100, v.temperature);
    Pick cp = fitText(g, condC, 4, v.condition, w, h * 16 / 100);
    drawText(g, cp.font, cp.size, x + w / 2, y + h * 84 / 100, 1, v.condition, GxEPD_BLACK);
  } else {  // wide box -> icon left, text right
    int pad = imax(2, w / 40);
    int iconBox = imin(h - pad * 2, w * 40 / 100);
    drawIcon(g, x + pad + iconBox / 2, y + h / 2, iconBox, v.code, GxEPD_BLACK);
    int colX = x + pad + iconBox + pad;
    int colW = (x + w) - colX;
    int colCX = colX + colW / 2;
    Pick tp = fitText(g, tempC, 6, tbuf, colW * 82 / 100, h * 50 / 100);
    drawTempBlock(g, tp.font, tp.size, colCX, y + h * 40 / 100, v.temperature);
    Pick cp = fitText(g, condC, 4, v.condition, colW, h * 26 / 100);
    drawText(g, cp.font, cp.size, colCX, y + h * 82 / 100, 1, v.condition, GxEPD_BLACK);
  }
}

template <class G>
inline void drawStrip(G& g, int x, int y, int w, int h, const WeatherView& v, const FontSet& F) {
  const Pick dayC[] = {{F.f12b, 1}, {F.f9b, 1}, {F.f9, 1}};
  const Pick hiC[] = {{F.f12b, 1}, {F.f9b, 1}};
  const Pick loC[] = {{F.f12, 1}, {F.f9, 1}};

  int cols = iclamp(w / 44, 2, v.dayCount);
  int colW = w / cols;
  g.fillRect(x, y, w, imax(1, h / 48), GxEPD_BLACK);  // top rule

  // Usable height leaves a small bottom margin so the low row never clips.
  const int uh = h - imax(2, h / 12);
  // Drop per-day icons when the band is too short for label + icon + two temps.
  const bool showIcon = uh >= 58;
  const int yLabel = y + uh * (showIcon ? 14 : 18) / 100;
  const int yHi = y + uh * (showIcon ? 70 : 56) / 100;
  const int yLo = y + uh * (showIcon ? 88 : 88) / 100;
  const int tempMaxH = uh * (showIcon ? 18 : 28) / 100;

  for (int i = 0; i < cols; i++) {
    const DayView& d = v.days[i];
    int cx = x + colW * i + colW / 2;

    Pick dp = fitText(g, dayC, 3, d.label, colW - 4, uh * 20 / 100);
    drawText(g, dp.font, dp.size, cx, yLabel, 1, d.label, GxEPD_BLACK);

    if (showIcon) {
      int iconS = imin(colW - 6, uh * 32 / 100);
      drawIcon(g, cx, y + uh * 44 / 100, iconS, d.code, GxEPD_BLACK);
    }

    char hb[8], lb[8];
    snprintf(hb, sizeof(hb), "%d", d.hi);
    snprintf(lb, sizeof(lb), "%d", d.lo);
    Pick hp = fitText(g, hiC, 2, "888", colW / 2 - 1, tempMaxH);
    Pick lp = fitText(g, loC, 2, "888", colW / 2 - 1, tempMaxH);
    drawText(g, hp.font, hp.size, cx, yHi, 1, hb, GxEPD_BLACK);
    drawText(g, lp.font, lp.size, cx, yLo, 1, lb, GxEPD_BLACK);
  }
}

// ---- Main layout (aspect-aware) ---------------------------------------------

template <class G>
inline void renderWeather(G& g, const WeatherView& v, const FontSet& F) {
  const int W = g.width();
  const int H = g.height();
  g.fillScreen(GxEPD_WHITE);
  g.setTextColor(GxEPD_BLACK);
  g.setTextWrap(false);

  const int M = imax(4, W / 36);
  const int headerH = iclamp(H * 14 / 100, 16, 80);
  const bool hasTrend = v.hourly && v.hourlyCount >= 2;
  const bool wide = W * 10 > H * 16;  // aspect ratio > 1.6

  drawHeader(g, M, 0, W - 2 * M, headerH, v, F);
  const int bodyY = headerH;
  const int bodyH = H - headerH;

  if (wide) {
    // Landscape: a feature band (current conditions on the left, hourly trend on
    // the right) over a full-width daily strip — keeps all forecast days while
    // adding the trend curve.
    int stripH = iclamp(H * 32 / 100, 40, 200);
    int featureH = bodyH - stripH;
    int gap = M;
    if (hasTrend) {
      int leftW = (W - 2 * M - gap) * 46 / 100;
      int rightX = M + leftW + gap;
      int rightW = (W - M) - rightX;
      drawHeroBlock(g, M, bodyY, leftW, featureH, v, F);
      drawTrend(g, rightX, bodyY, rightW, featureH, v.hourly, v.hourlyCount);
    } else {
      drawHeroBlock(g, M, bodyY, W - 2 * M, featureH, v, F);
    }
    drawStrip(g, M, bodyY + featureH, W - 2 * M, stripH, v, F);
  } else {
    // Vertical stack (portrait/square): header, hero, trend, daily strip.
    int stripH = iclamp(H * 34 / 100, 40, 200);
    int trendH = hasTrend ? iclamp(H * 18 / 100, 22, 120) : 0;
    int heroH = bodyH - trendH - stripH;
    drawHeroBlock(g, M, bodyY, W - 2 * M, heroH, v, F);
    if (hasTrend) drawTrend(g, M, bodyY + heroH, W - 2 * M, trendH, v.hourly, v.hourlyCount);
    drawStrip(g, M, bodyY + heroH + trendH, W - 2 * M, stripH, v, F);
  }
}

}  // namespace atmo
