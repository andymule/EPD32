// Resolution-independent weather layout, shared verbatim between the desktop
// preview harness and the device. Templated on the GFX object (HostGFX or the
// GxEPD2/Adafruit_GFX panel), with fonts and the data view injected so the file
// has no Arduino/STL dependency. Every coordinate is derived from width()/
// height(), so the same code adapts to 296x128, 200x200, 128x296, etc.
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
  const GFXfont* f6;
  const GFXfont* f6b;
  const GFXfont* f7;
  const GFXfont* f7b;
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
  const char* date;        // "Wed Jun 10" (day-of-week + month + day)
  const char* time;        // "9:53" (12h, no am/pm); may be null/empty
  const char* condition;   // e.g. "Drizzle"
  int temperature;
  int code;                // current WMO code
  const DayView* days;     // days[0] == today
  int dayCount;
  const int* temp;         // next ~24h temperatures (may be null)
  const int* precip;       // next ~24h precip probability % (may be null)
  int hourlyCount;         // shared length of temp[]/precip[]
  int hourStart;           // clock hour (0-23) of sample[0]; -1 to skip labels
  int sunriseHour;         // local clock hour of sunrise (0-23); -1 if unknown
  int sunsetHour;          // local clock hour of sunset (0-23); -1 if unknown
  int humidity = -1;       // relative humidity %, 0-100; <0 hides the field
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
//
// Icons are hollowed with `bg` so the same shapes render correctly on white or
// on a dark (inverted) band: pass bg=GxEPD_WHITE on white, bg=GxEPD_BLACK on a
// filled-black footer with c=GxEPD_WHITE.

template <class G>
inline void thickLine(G& g, int x0, int y0, int x1, int y1, int t, uint16_t c) {
  for (int o = -(t / 2); o <= t / 2; o++) {
    g.drawLine(x0 + o, y0, x1 + o, y1, c);
    g.drawLine(x0, y0 + o, x1, y1 + o, c);
  }
}

// Clear sky: a clean circle. Bold filled disc at hero sizes; a thin ring in the
// small day cells — matching the reference iconography (no rays). forceSolid
// keeps it a filled disc even at small sizes (used by the square layout).
template <class G>
inline void iconSun(G& g, int cx, int cy, int s, uint16_t c, uint16_t bg,
                    bool forceSolid = false) {
  int r = s * 38 / 100;
  g.fillCircle(cx, cy, r, c);
  if (s < 40 && !forceSolid) {           // small -> hollow ring
    int sw = imax(2, s * 13 / 100);
    g.fillCircle(cx, cy, r - sw, bg);
  }
}

// Puffy, flat-bottomed cloud silhouette. `erode` pulls every edge inward by a
// uniform amount, so drawing a second copy in `bg` with erode == stroke leaves a
// clean, even outline.
template <class G>
inline void cloudBody(G& g, int cx, int cy, int s, int erode, uint16_t c) {
  int by = cy + s * 16 / 100;                       // flat bottom line
  g.fillCircle(cx - s * 24 / 100, by - s * 15 / 100, imax(1, s * 15 / 100 - erode), c);
  g.fillCircle(cx + s * 26 / 100, by - s * 18 / 100, imax(1, s * 18 / 100 - erode), c);
  g.fillCircle(cx + s *  2 / 100, by - s * 26 / 100, imax(1, s * 24 / 100 - erode), c);
  int rectTop = by - s * 17 / 100 + erode;
  int rectBot = by - erode;
  int rectW = s * 90 / 100 - 2 * erode;
  if (rectBot > rectTop && rectW > 0)
    g.fillRoundRect(cx - rectW / 2, rectTop, rectW, rectBot - rectTop,
                    imax(1, s * 6 / 100), c);
}

template <class G>
inline void iconCloud(G& g, int cx, int cy, int s, uint16_t c, uint16_t bg) {
  int sw = imax(2, s * 9 / 100);
  cloudBody(g, cx, cy, s, 0, c);
  cloudBody(g, cx, cy, s, sw, bg);  // inset copy -> uniform outline
}

// Rain (dotted/dashed streaks, matching the boot-splash rain) or snow (dots)
// falling below the cloud.
template <class G>
inline void rainStreaks(G& g, int cx, int cy, int s, uint16_t c, bool snow) {
  if (snow) {
    int y0 = cy + s * 22 / 100;
    int len = s * 20 / 100;
    for (int i = -1; i <= 1; i++)
      g.fillCircle(cx + i * s * 22 / 100, y0 + len / 2, imax(1, s * 6 / 100), c);
    return;
  }
  // Rain: dotted dashes that start right at the cloud's flat bottom (so the rain
  // connects to the cloud), several columns across — the same airy look as the
  // boot-splash rain ("little dots mean rain").
  int top = cy - s * 4 / 100;          // ~cloud bottom -> rain hugs the cloud
  int len = imax(6, s * 46 / 100);
  int dx = imax(2, s * 14 / 100);
  int half = iclamp((s * 38 / 100) / dx, 1, 2);
  for (int col = -half; col <= half; col++) {
    int x = cx + col * dx;
    for (int y = 0; y < len; y++)
      if (((y + col + 8) & 3) < 2) g.drawPixel(x, top + y, c);  // 2-on/2-off dashes
  }
}

template <class G>
inline void iconRain(G& g, int cx, int cy, int s, uint16_t c, uint16_t bg) {
  iconCloud(g, cx, cy - s * 16 / 100, s * 82 / 100, c, bg);
  rainStreaks(g, cx, cy, s, c, false);
}

template <class G>
inline void iconSnow(G& g, int cx, int cy, int s, uint16_t c, uint16_t bg) {
  iconCloud(g, cx, cy - s * 16 / 100, s * 82 / 100, c, bg);
  rainStreaks(g, cx, cy, s, c, true);
}

template <class G>
inline void iconStorm(G& g, int cx, int cy, int s, uint16_t c, uint16_t bg) {
  iconCloud(g, cx, cy - s * 16 / 100, s * 82 / 100, c, bg);
  int t = imax(2, s * 8 / 100);
  thickLine(g, cx + s * 6 / 100, cy + s * 12 / 100, cx - s * 9 / 100, cy + s * 28 / 100, t, c);
  thickLine(g, cx - s * 9 / 100, cy + s * 28 / 100, cx + s * 9 / 100, cy + s * 28 / 100, t, c);
  thickLine(g, cx + s * 9 / 100, cy + s * 28 / 100, cx - s * 4 / 100, cy + s * 44 / 100, t, c);
}

template <class G>
inline void iconFog(G& g, int cx, int cy, int s, uint16_t c, uint16_t bg) {
  iconCloud(g, cx, cy - s * 18 / 100, s * 80 / 100, c, bg);
  int t = imax(2, s * 7 / 100);
  int y0 = cy + s * 18 / 100;
  for (int i = 0; i < 3; i++) {
    int w = s * (56 - i * 8) / 100;
    g.fillRoundRect(cx - w / 2, y0 + i * (t + imax(2, s / 16)), w, t, t / 2, c);
  }
}

template <class G>
inline void iconPartly(G& g, int cx, int cy, int s, uint16_t c, uint16_t bg,
                       bool forceSolid = false) {
  iconSun(g, cx + s * 18 / 100, cy - s * 18 / 100, s * 58 / 100, c, bg, forceSolid);
  iconCloud(g, cx - s * 8 / 100, cy + s * 10 / 100, s * 88 / 100, c, bg);
}

// A small solid teardrop (humidity marker): a bulbous round bottom tapering to
// a pointed top. Kept solid because a hollow outline collapses into an ambiguous
// little ring at the tiny sizes this is used at; solid reads clearly as a drop.
template <class G>
inline void iconDrop(G& g, int cx, int cy, int s, uint16_t c, uint16_t bg) {
  (void)bg;
  int r = imax(2, s * 36 / 100);
  int by = cy + s * 16 / 100;            // centre of the round bottom
  int apexY = cy - s * 50 / 100;         // pointed tip
  // The taper meets the bowl just above its centre, so the circle's shoulders
  // form the widest part — a teardrop, not a plain cone.
  int joinY = by - r / 2;
  int span = imax(1, joinY - apexY);
  for (int y = apexY; y <= joinY; y++) {
    int hw = (r * 92 / 100) * (y - apexY) / span;
    g.drawFastHLine(cx - hw, y, 2 * hw + 1, c);
  }
  g.fillCircle(cx, by, r, c);            // round bottom completes the teardrop
}

// Stacked horizontal zigzag lines — a "water/humidity" motif (the wavy texture
// from the reference design). `amp` is the zigzag half-height and `pitch` the
// centre-to-centre row spacing, so the clear gap between rows is pitch - 2*amp
// (uniform across every row). Centred on (cx, cy).
template <class G>
inline void iconWaves(G& g, int cx, int cy, int w, int amp, int pitch,
                      uint16_t c, int rows = 3) {
  if (rows < 1) rows = 1;
  if (amp < 1) amp = 1;
  int step = imax(2, w / 7);                 // horizontal peak<->valley spacing
  int left = cx - w / 2, right = cx + w / 2;
  int topY = cy - pitch * (rows - 1) / 2;    // evenly spaced, vertically centred
  for (int r = 0; r < rows; r++) {
    int ry = topY + r * pitch;
    int prevx = left, sign = -1, prevy = ry + sign * amp;
    for (int x = left + step; x <= right; x += step) {
      sign = -sign;
      int y = ry + sign * amp;
      g.drawLine(prevx, prevy, x, y, c);
      prevx = x;
      prevy = y;
    }
  }
}

template <class G>
inline void drawIcon(G& g, int cx, int cy, int s, int code, uint16_t c,
                     uint16_t bg, bool forceSolid = false) {
  switch (code) {
    case 0: case 1: iconSun(g, cx, cy, s, c, bg, forceSolid); break;
    case 2: iconPartly(g, cx, cy, s, c, bg, forceSolid); break;
    case 3: iconCloud(g, cx, cy, s, c, bg); break;
    case 45: case 48: iconFog(g, cx, cy, s, c, bg); break;
    case 51: case 53: case 55: case 56: case 57:
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82: iconRain(g, cx, cy, s, c, bg); break;
    case 71: case 73: case 75: case 77: case 85: case 86:
      iconSnow(g, cx, cy, s, c, bg); break;
    case 95: case 96: case 99: iconStorm(g, cx, cy, s, c, bg); break;
    default: iconCloud(g, cx, cy, s, c, bg); break;
  }
}

// Degree ring drawn to the upper-right of a number ending at (rightX, topY).
template <class G>
inline void drawDegree(G& g, int rightX, int topY, int r, uint16_t c, uint16_t bg) {
  int t = imax(1, r / 2);
  g.fillCircle(rightX, topY + r, r, c);
  g.fillCircle(rightX, topY + r, r - t, bg);
}

// ---- Texture --------------------------------------------------------------

// A light dotted halftone: rows of evenly spaced dots spanning the band, which
// reads as a soft "gray" texture on 1-bit panels while keeping big black numbers
// crisp on top. Drawn edge-to-edge across the device width.
template <class G>
inline void dottedBand(G& g, int x, int y, int w, int h, uint16_t c) {
  if (w < 4 || h < 4) return;
  const int rowPitch = iclamp(h / 6, 4, 6);
  const int dotPitch = 3;
  for (int ry = y + rowPitch; ry < y + h; ry += rowPitch)
    for (int dx = x + 1; dx < x + w; dx += dotPitch)
      g.drawPixel(dx, ry, c);
}

// Convert a 24h clock hour to a 12h label digit (no am/pm): 0->12, 13->1, etc.
inline int to12h(int h) { int v = h % 12; return v == 0 ? 12 : v; }

// Hourly precipitation-chance trend: a thin axis with small 12h labels, a bold
// hash at midnight, and the chance curve below on a fixed 0-100% scale (so a dry
// stretch correctly sits low rather than being exaggerated).
template <class G>
inline void drawTrend(G& g, int x, int y, int w, int h, const WeatherView& v,
                      const FontSet& F) {
  const int* t = v.precip;
  int n = v.hourlyCount;
  if (!t || n < 2 || w < 8 || h < 10) return;

  const bool labels = v.hourStart >= 0 && h >= 30;
  int axisY = y + h * (labels ? 32 : 20) / 100;
  int cTop = y + h * (labels ? 40 : 30) / 100;
  int cBot = y + h - imax(1, h / 14);  // margin so the curve/dot don't bleed
  int cH = cBot - cTop;
  if (cH < 3) return;

  g.drawFastHLine(x, axisY, w, GxEPD_BLACK);
  for (int hh = 0; hh < n; hh += 6) {        // small 12h label every 6 hours
    int px = x + (w - 1) * hh / (n - 1);
    g.drawFastVLine(px, axisY - 1, 3, GxEPD_BLACK);
    if (labels) {
      char hb[4];
      snprintf(hb, sizeof(hb), "%d", to12h(v.hourStart + hh));
      drawText(g, F.f7, 1, px, y + h * 14 / 100, 1, hb, GxEPD_BLACK);
    }
  }
  // A more substantial, unlabeled hash wherever midnight falls.
  for (int i = 0; i < n; i++) {
    if ((v.hourStart + i) % 24 == 0) {
      int px = x + (w - 1) * i / (n - 1);
      g.fillRect(px, axisY - 4, 2, 9, GxEPD_BLACK);
    }
  }

  int lineT = iclamp(h / 44, 2, 3);  // slim curve
  int prevX = 0, prevY = 0, firstY = 0;
  for (int i = 0; i < n; i++) {
    int px = x + (w - 1) * i / (n - 1);
    int pv = iclamp(t[i], 0, 100);
    int py = cTop + (cH - 1) - pv * (cH - 1) / 100;  // fixed 0-100% scale
    if (i == 0) firstY = py;
    if (i > 0) thickLine(g, prevX, prevY, px, py, lineT, GxEPD_BLACK);
    prevX = px;
    prevY = py;
  }
  g.fillCircle(x, firstY, iclamp(h / 30, 2, 4), GxEPD_BLACK);  // "now" marker
}

// Pen-advance width of a string (the typographic width, including side
// bearings) — unlike getTextBounds, which returns the tight ink box. Measured by
// advancing the cursor off-screen so it works identically on host and device.
template <class G>
inline int textAdvance(G& g, const GFXfont* f, uint8_t sz, const char* s) {
  g.setFont(f);
  g.setTextSize(sz);
  g.setCursor(-1000, 0);     // far off-screen: every glyph is clipped away
  g.print(s);
  return g.getCursorX() + 1000;
}

// Draws an integer temperature centered on (cx,cy) by its pen advance, so the
// digits' true midpoint sits on the axis (a tight ink box would let a narrow
// trailing digit like "1" pull the visual centre onto the previous digit). The
// degree mark hangs at the upper-right and is ignored for alignment.
template <class G>
inline void drawTempBlock(G& g, const GFXfont* f, uint8_t sz, int cx, int cy,
                          int value, uint16_t c, uint16_t bg) {
  char b[8];
  snprintf(b, sizeof(b), "%d", value);
  g.setFont(f);
  g.setTextSize(sz);
  int16_t x1, y1;
  uint16_t w, h;
  g.getTextBounds(b, 0, 0, &x1, &y1, &w, &h);
  int adv = textAdvance(g, f, sz, b);
  int penX = cx - adv / 2;                       // centre the advance box on cx
  g.setFont(f);
  g.setTextSize(sz);
  g.setTextColor(c);
  g.setCursor(penX, cy - y1 - h / 2);
  g.print(b);
  int degR = imax(1, h * 9 / 100);               // small, scales with the digits
  int gap = imax(1, degR / 2);
  drawDegree(g, penX + x1 + w + gap + degR, cy - h / 2, degR, c, bg);
}

// Width of the hi/lo block drawn by drawHiLo, so callers can center it within a
// region (matches the blockW formula in drawHiLo exactly).
template <class G>
inline int hiLoWidth(G& g, const Pick& hp, const Pick& lp, int hi, int lo) {
  char hb[8], lb[8];
  snprintf(hb, sizeof(hb), "%d", hi);
  snprintf(lb, sizeof(lb), "%d", lo);
  int hw, hh, lw, lh;
  measure(g, hp.font, hp.size, hb, &hw, &hh);
  measure(g, lp.font, lp.size, lb, &lw, &lh);
  int degR = imax(1, hh * 9 / 100);
  return imax(hw, lw) + degR * 2 + imax(1, degR / 2);
}

// A compact "26°/17°" hi/lo pair (hi bold over lo regular), right-aligned so its
// right edge sits at rightX, vertically centered on cy. Returns its width.
template <class G>
inline int drawHiLo(G& g, const Pick& hp, const Pick& lp, int rightX, int cy,
                    int hi, int lo, uint16_t c, uint16_t bg, int gapOverride = -1) {
  char hb[8], lb[8];
  snprintf(hb, sizeof(hb), "%d", hi);
  snprintf(lb, sizeof(lb), "%d", lo);
  int hw, hh, lw, lh;
  measure(g, hp.font, hp.size, hb, &hw, &hh);
  measure(g, lp.font, lp.size, lb, &lw, &lh);
  int degR = imax(1, hh * 9 / 100);     // small degree, consistent with the rest
  int loDegR = imax(1, lh * 9 / 100);
  int blockW = imax(hw, lw) + degR * 2 + imax(1, degR / 2);
  int gap = gapOverride >= 0 ? gapOverride : imax(4, hh * 35 / 100);
  int hiCY = cy - (hh + gap) / 2;
  int loCY = cy + (lh + gap) / 2;
  int leftX = rightX - blockW;

  drawText(g, hp.font, hp.size, leftX, hiCY, 0, hb, c);
  drawDegree(g, leftX + hw + imax(1, degR / 2) + degR, hiCY - hh / 2, degR, c, bg);
  drawText(g, lp.font, lp.size, leftX, loCY, 0, lb, c);
  drawDegree(g, leftX + lw + imax(1, loDegR / 2) + loDegR, loCY - lh / 2, loDegR, c, bg);
  return blockW;
}

// Draws the hi/lo block a fixed gap to the right of a temperature that was drawn
// (centered) at tempCx with font tp — so the pair hugs the number like the
// reference, rather than floating at the region's center. The block's right edge
// is clamped to maxRight so it never runs off the panel.
template <class G>
inline void drawHiLoAfterTemp(G& g, const Pick& tp, const Pick& hp, const Pick& lp,
                              int tempCx, int cy, int temperature, int hi, int lo,
                              int maxRight, int gap) {
  char tb[8];
  snprintf(tb, sizeof(tb), "%d", temperature);
  int tw, th;
  measure(g, tp.font, tp.size, tb, &tw, &th);
  int adv = textAdvance(g, tp.font, tp.size, tb);
  int degR = imax(1, th * 9 / 100);                 // matches drawTempBlock
  int tempRight = tempCx + adv / 2 + imax(1, degR / 2) + 2 * degR;
  int bw = hiLoWidth(g, hp, lp, hi, lo);
  int leftX = tempRight + gap;
  if (leftX + bw > maxRight) leftX = maxRight - bw;  // keep on-panel
  drawHiLo(g, hp, lp, leftX + bw, cy, hi, lo, GxEPD_BLACK, GxEPD_WHITE);
}

// ---- Composable regions -----------------------------------------------------

// Status-bar header: date on the left, 12h time on the right, hairline beneath.
// Both pieces share one font, chosen so the combined string fits the width — so
// the date and time can never overlap, on any panel size.
template <class G>
inline void drawHeader(G& g, int x, int y, int w, int h, const WeatherView& v,
                       const FontSet& F, bool rule = true) {
  const Pick hc[] = {{F.f24, 1}, {F.f18, 1}, {F.f12, 1}, {F.f9, 1}, {F.f7, 1}};
  const int NHC = 5;
  int cy = y + h / 2;
  bool hasDate = v.date && v.date[0];
  bool hasTime = v.time && v.time[0];
  const int minGap = imax(7, w / 16);
  const uint16_t BK = GxEPD_BLACK;

  // Single status-bar line: date left, time right, with a clear gap. Pick the
  // largest font that keeps them apart; fall back to two stacked lines only if
  // even 7pt can't fit them side by side.
  if (hasDate && hasTime) {
    Pick p = hc[NHC - 1];
    bool fits = false;
    for (int i = 0; i < NHC; i++) {
      int dw, tw, dh, th;
      measure(g, hc[i].font, hc[i].size, v.date, &dw, &dh);
      measure(g, hc[i].font, hc[i].size, v.time, &tw, &th);
      if (dw + tw + minGap <= w && imax(dh, th) <= h - 4) { p = hc[i]; fits = true; break; }
    }
    if (fits) {
      drawText(g, p.font, p.size, x, cy, 0, v.date, BK);
      drawText(g, p.font, p.size, x + w, cy, 2, v.time, BK);
    } else {
      Pick dp = fitText(g, hc, NHC, v.date, w, h * 52 / 100 - 2);
      Pick tp = fitText(g, hc, NHC, v.time, w, h * 40 / 100 - 2);
      drawText(g, dp.font, dp.size, x + w / 2, y + h * 30 / 100, 1, v.date, BK);
      drawText(g, tp.font, tp.size, x + w / 2, y + h * 72 / 100, 1, v.time, BK);
    }
  } else if (hasDate) {
    Pick dp = fitText(g, hc, NHC, v.date, w, h - 4);
    drawText(g, dp.font, dp.size, x, cy, 0, v.date, BK);
  } else if (hasTime) {
    Pick tp = fitText(g, hc, NHC, v.time, w, h - 4);
    drawText(g, tp.font, tp.size, x + w, cy, 2, v.time, BK);
  }
  if (rule) g.fillRect(x, y + h - 1, w, imax(1, h / 24), BK);
}

// Current conditions. Tall box: icon over a textured temp band (big temp + today
// hi/lo) over a "Today, <condition>" caption. Wide box: icon left, the same temp
// band on the right.
template <class G>
inline void drawHeroBlock(G& g, int x, int y, int w, int h, const WeatherView& v, const FontSet& F) {
  const Pick tempC[] = {{F.f24b, 4}, {F.f24b, 3}, {F.f24b, 2}, {F.f24b, 1}, {F.f18b, 1}, {F.f12b, 1}};
  const Pick hiC[]   = {{F.f12b, 1}, {F.f9b, 1}};
  const Pick loC[]   = {{F.f12, 1},  {F.f9, 1}};
  const Pick capC[]  = {{F.f9, 1}, {F.f7, 1}};
  char tbuf[8];
  snprintf(tbuf, sizeof(tbuf), "%d", v.temperature);
  bool hasDay = v.dayCount > 0;

  // Caption keeps the current reading unmistakably "today".
  char cap[40];
  if (v.condition && v.condition[0])
    snprintf(cap, sizeof(cap), "Today, %s", v.condition);
  else
    snprintf(cap, sizeof(cap), "Today");

  if (h * 5 >= w * 4) {  // tall-ish box (h/w >= 0.8) -> vertical stack
    int iconBox = imin(w * 56 / 100, h * 34 / 100);
    drawIcon(g, x + w / 2, y + h * 18 / 100, iconBox, v.code, GxEPD_BLACK, GxEPD_WHITE);

    int bandH = h * 30 / 100;
    int bandCY = y + h * 48 / 100;
    int bandY = bandCY - bandH / 2;

    // Dotted texture spans the full device width; the big number + hi/lo are
    // drawn on top so they stay crisp.
    int hiloW = w * 26 / 100;
    int tempW = w - hiloW - imax(6, w / 16);
    dottedBand(g, 0, bandY, g.width(), bandH, GxEPD_BLACK);
    Pick tp = fitText(g, tempC, 6, tbuf, tempW, bandH * 96 / 100);
    Pick hp = fitText(g, hiC, 2, "88", hiloW, bandH * 38 / 100);
    Pick lp = fitText(g, loC, 2, "88", hiloW, bandH * 38 / 100);
    drawTempBlock(g, tp.font, tp.size, x + tempW / 2, bandCY, v.temperature,
                  GxEPD_BLACK, GxEPD_WHITE);
    if (hasDay)
      drawHiLo(g, hp, lp, x + w, bandCY, v.days[0].hi, v.days[0].lo, GxEPD_BLACK,
               GxEPD_WHITE);

    Pick cp = fitText(g, capC, 2, cap, w, h * 13 / 100);
    drawText(g, cp.font, cp.size, x + w / 2, y + h * 82 / 100, 1, cap, GxEPD_BLACK);
  } else {  // wide box -> icon left, text right
    int pad = imax(2, w / 40);
    int iconBox = imin(h - pad * 2, w * 32 / 100);
    drawIcon(g, x + pad + iconBox / 2, y + h * 44 / 100, iconBox, v.code, GxEPD_BLACK, GxEPD_WHITE);
    int colX = x + pad + iconBox + pad;
    int colW = (x + w) - colX;
    int bandCY = y + h * 38 / 100;
    int hiloW = colW * 30 / 100;
    Pick tp = fitText(g, tempC, 6, tbuf, colW - hiloW - imax(4, colW / 20), h * 48 / 100);
    Pick hp = fitText(g, hiC, 2, "88", hiloW, h * 24 / 100);
    Pick lp = fitText(g, loC, 2, "88", hiloW, h * 24 / 100);
    drawTempBlock(g, tp.font, tp.size, colX + (colW - hiloW) * 46 / 100, bandCY,
                  v.temperature, GxEPD_BLACK, GxEPD_WHITE);
    if (hasDay)
      drawHiLo(g, hp, lp, colX + colW, bandCY, v.days[0].hi, v.days[0].lo,
               GxEPD_BLACK, GxEPD_WHITE);
    Pick cp = fitText(g, capC, 2, cap, colW, h * 22 / 100);
    drawText(g, cp.font, cp.size, colX + colW / 2, y + h * 86 / 100, 1, cap, GxEPD_BLACK);
  }
}

// Safe 2-letter abbreviation of a day label ("Thu" -> "Th"); never reads past a
// short or empty source string.
inline void twoCharLabel(char out[3], const char* s) {
  out[0] = (s && s[0]) ? s[0] : '\0';
  out[1] = (out[0] && s[1]) ? s[1] : '\0';
  out[2] = '\0';
}

// Forecast strip on a dark band: the days *ahead* (starting tomorrow), each with
// a 2-letter label, icon, and hi/lo — drawn white-on-black for contrast.
template <class G>
inline void drawStrip(G& g, int x, int y, int w, int h, const WeatherView& v, const FontSet& F) {
  const Pick dayC[] = {{F.f9b, 1}, {F.f7b, 1}};
  const Pick hiC[]  = {{F.f9b, 1}, {F.f7b, 1}};
  const Pick loC[]  = {{F.f9, 1},  {F.f7, 1}};

  g.fillRect(x, y, w, h, GxEPD_BLACK);  // the dark footer

  int ahead = v.dayCount - 1;           // skip today (index 0); show tomorrow on
  if (ahead < 1) return;
  int cols = iclamp(w / 26, 2, ahead);
  int colW = w / cols;
  int pad = imax(2, h / 16);
  int innerY = y + pad;
  int uh = h - pad * 2;

  // Adapt the rows to the band height so icons stay legible: tall bands show
  // icon + hi + lo; medium bands show a bigger icon + the high only (like the
  // reference design); short bands drop the icon.
  const bool showIcon = uh >= 50;
  const bool twoTemp = showIcon ? (uh >= 72) : (uh >= 42);
  int yLabel, yIcon, iconPct, yHi, yLo, tempMaxH;
  if (showIcon && twoTemp)       { yLabel = 11; yIcon = 40; iconPct = 24; yHi = 67; yLo = 86; tempMaxH = 15; }
  else if (showIcon)             { yLabel = 13; yIcon = 49; iconPct = 34; yHi = 85; yLo = 0;  tempMaxH = 18; }
  else if (twoTemp)              { yLabel = 20; yIcon = 0;  iconPct = 0;  yHi = 54; yLo = 82; tempMaxH = 22; }
  else                           { yLabel = 28; yIcon = 0;  iconPct = 0;  yHi = 70; yLo = 0;  tempMaxH = 30; }

  // Pick ONE label font that fits every column, so all day labels match size.
  Pick labelPick = dayC[1];
  for (int c = 0; c < 2; c++) {
    bool ok = true;
    for (int i = 0; i < cols && ok; i++) {
      char lab[3];
      twoCharLabel(lab, v.days[i + 1].label);
      int lw, lh;
      measure(g, dayC[c].font, dayC[c].size, lab, &lw, &lh);
      if (lw > colW - 4 || lh > uh * 24 / 100) ok = false;
    }
    if (ok) { labelPick = dayC[c]; break; }
  }
  Pick hp = fitText(g, hiC, 2, "888", colW / 2 - 1, tempMaxH);
  Pick lp = fitText(g, loC, 2, "888", colW / 2 - 1, tempMaxH);

  for (int i = 0; i < cols; i++) {
    const DayView& d = v.days[i + 1];   // +1: tomorrow onward
    int cx = x + colW * i + colW / 2;

    char lab[3];
    twoCharLabel(lab, d.label);   // "Thu" -> "Th"
    drawText(g, labelPick.font, labelPick.size, cx, innerY + uh * yLabel / 100, 1, lab, GxEPD_WHITE);

    if (showIcon) {
      int iconS = imin(colW - 6, uh * iconPct / 100);
      drawIcon(g, cx, innerY + uh * yIcon / 100, iconS, d.code, GxEPD_WHITE, GxEPD_BLACK);
    }

    char hb[8], lb[8];
    snprintf(hb, sizeof(hb), "%d", d.hi);
    snprintf(lb, sizeof(lb), "%d", d.lo);
    drawText(g, hp.font, hp.size, cx, innerY + uh * yHi / 100, 1, hb, GxEPD_WHITE);
    if (yLo)
      drawText(g, lp.font, lp.size, cx, innerY + uh * yLo / 100, 1, lb, GxEPD_WHITE);
  }
}

// ---- Dashboard layout (aspect-aware) ----------------------------------------

template <class G>
inline void renderDashboard(G& g, const WeatherView& v, const FontSet& F) {
  const int W = g.width();
  const int H = g.height();
  g.fillScreen(GxEPD_WHITE);
  g.setTextColor(GxEPD_BLACK);
  g.setTextWrap(false);

  const int M = imax(4, W / 36);
  const int headerH = iclamp(H * 15 / 100, 22, 72);
  const bool hasTrend = v.precip && v.hourlyCount >= 2;
  const bool wide = W * 10 > H * 16;  // aspect ratio > 1.6

  drawHeader(g, M, 0, W - 2 * M, headerH, v, F);
  const int bodyY = headerH;
  const int bodyH = H - headerH;

  if (wide) {
    // Landscape: feature band (current conditions left, precip trend right) over
    // a full-width forecast strip.
    int stripH = iclamp(H * 32 / 100, 40, 200);
    int featureH = bodyH - stripH;
    int gap = M;
    if (hasTrend) {
      int leftW = (W - 2 * M - gap) * 46 / 100;
      int rightX = M + leftW + gap;
      int rightW = (W - M) - rightX;
      drawHeroBlock(g, M, bodyY, leftW, featureH, v, F);
      drawTrend(g, rightX, bodyY, rightW, featureH, v, F);
    } else {
      drawHeroBlock(g, M, bodyY, W - 2 * M, featureH, v, F);
    }
    drawStrip(g, 0, bodyY + featureH, W, stripH, v, F);
  } else {
    // Vertical stack (portrait/square): header, hero, trend, forecast strip.
    int stripH = iclamp(H * 33 / 100, 40, 210);
    int trendH = hasTrend ? iclamp(H * 18 / 100, 24, 120) : 0;
    int heroH = bodyH - trendH - stripH;
    drawHeroBlock(g, M, bodyY, W - 2 * M, heroH, v, F);
    if (hasTrend) drawTrend(g, M, bodyY + heroH, W - 2 * M, trendH, v, F);
    drawStrip(g, 0, bodyY + heroH + trendH, W, stripH, v, F);
  }
}

// ---- Horizon layout ---------------------------------------------------------
//
// A calmer, more pictorial style: date/time pinned to the very top, a large
// clean hero icon + current temperature, and a solid dark mass whose *top edge
// is the next-24h temperature curve*. Dotted "rain" falls from that curve where
// precipitation is likely. The daily forecast (high only) sits along the bottom.

// Smooth y of the temp curve at screen column px: a Catmull-Rom spline through
// the hourly points (rounds the joints rather than the angular straight-line
// segments a linear interp gives). 64-bit intermediates keep it exact and
// overflow-safe; the result is clamped to the band so the spline can't overshoot
// into the hero or the day strip.
inline int horizonCurveY(int px, int x0, int w, const int* temp, int n, int mn,
                         int mx, int yTop, int yBot) {
  long long D = w - 1;
  long long num = (long long)(px - x0) * (n - 1);
  int seg = (int)(num / D);
  long long t = num - (long long)seg * D;     // 0..D within the segment
  if (seg > n - 2) { seg = n - 2; t = D; }
  if (seg < 0) { seg = 0; t = 0; }
  long long p1 = temp[seg];
  long long p2 = temp[seg + 1];
  long long p0 = temp[seg > 0 ? seg - 1 : 0];
  long long p3 = temp[seg + 2 < n ? seg + 2 : n - 1];
  long long D2 = D * D, D3 = D2 * D, t2 = t * t, t3 = t2 * t;
  // 2 * value * D^3 for the uniform Catmull-Rom basis.
  long long v2D3 = 2 * p1 * D3 + (p2 - p0) * t * D2 +
                   (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 * D +
                   (-p0 + 3 * p1 - 3 * p2 + p3) * t3;
  long long numer = (v2D3 - 2 * (long long)mn * D3) * (yBot - yTop);
  long long denom = 2 * D3 * (mx - mn);
  int y = yBot - (int)(numer / denom);
  return iclamp(y, yTop, yBot);
}

// Low-pass smooths hourly samples into a gentle curve (repeated [1,2,1] passes;
// endpoints anchored at "now" and "+24h"). dst must hold at least n ints.
inline void smoothSeries(const int* src, int* dst, int n, int passes) {
  for (int i = 0; i < n; i++) dst[i] = src[i];
  for (int p = 0; p < passes; p++) {
    int prev = dst[0];
    for (int i = 1; i < n - 1; i++) {
      int cur = dst[i];
      dst[i] = (prev + 2 * cur + dst[i + 1] + 2) / 4;
      prev = cur;
    }
  }
}

// A little celestial glyph that hangs above a curve marker, drawn black on the
// white sky. kind: 0 crescent moon (midnight), 1 sun (noon), 2 sunrise, 3
// sunset. Sunrise/sunset are a half-sun on the horizon, told apart by an up vs.
// down arrow above the dome.
template <class G>
inline void drawSkyGlyph(G& g, int kind, int gx, int gy, int r, uint16_t c,
                         uint16_t bg) {
  static const int ux[8] = {0, 7, 10, 7, 0, -7, -10, -7};  // unit dirs (tenths)
  static const int uy[8] = {-10, -7, 0, 7, 10, 7, 0, -7};
  if (kind == 0) {  // crescent moon (opens left, as before)
    int off = imax(2, r * 8 / 10);
    g.fillCircle(gx, gy, r, c);
    g.fillCircle(gx - off, gy, r, bg);
    return;
  }
  if (kind == 1) {  // full sun: disc + eight short rays
    int rd = imax(2, r * 6 / 10);
    g.fillCircle(gx, gy, rd, c);
    int ri = rd + imax(2, r / 3);
    int ro = ri + imax(2, r / 2);
    for (int i = 0; i < 8; i++)
      g.drawLine(gx + ux[i] * ri / 10, gy + uy[i] * ri / 10,
                 gx + ux[i] * ro / 10, gy + uy[i] * ro / 10, c);
    return;
  }
  // Sunrise (2) / sunset (3): a simple filled triangle — up for rise, down for
  // set. Drawn via horizontal scanlines (portable: no fillTriangle on the host
  // preview).
  bool rising = (kind == 2);
  int aw = imax(3, r);                 // half-width
  int ah = imax(4, r * 14 / 10);       // height
  int yTopA = gy - ah / 2;
  int yBotA = gy + ah / 2;
  int span = ah > 0 ? ah : 1;
  for (int y = yTopA; y <= yBotA; y++) {
    int d = rising ? (y - yTopA) : (yBotA - y);
    int hw = (aw * d + span / 2) / span;  // rounded -> clean 1px tip (no needle)
    g.drawFastHLine(gx - hw, y, 2 * hw + 1, c);
  }
}

// Fills the dark mass under the (smoothed) temperature curve, then draws the
// hourly rain-chance as dotted columns rising from the bottom. Each column is
// white while it overlays the dark mass and flips to black once it rises above
// the curve into the open sky, so it stays visible either way.
template <class G>
inline void drawTempHorizon(G& g, int x0, int w, int yTop, int yBot, int fillTo,
                            const WeatherView& v, bool square = false) {
  const int* temp = v.temp;
  int n = v.hourlyCount;
  if (!temp || n < 2 || w < 4) {  // no curve data -> plain dark mass
    g.fillRect(x0, yBot, w, imax(0, fillTo - yBot), GxEPD_BLACK);
    return;
  }
  // Smooth, then downsample to a few bin-averaged control points and spline
  // through those: long smooth arcs read as one calm wave (like the reference)
  // instead of a bumpy line chasing all 24 hourly points. Fidelity is
  // intentionally traded for a beautiful curve.
  const int MAXS = 48;
  int m = imin(n, MAXS);
  int sm[MAXS];
  smoothSeries(temp, sm, m, 4);

  const int K = 8;                       // control points: smooth but with shape
  int kk = imin(K, m);
  int cp[K];
  for (int k = 0; k < kk; k++) {
    int a = m * k / kk, b = m * (k + 1) / kk;
    if (b <= a) b = a + 1;
    if (b > m) b = m;
    long sum = 0;
    for (int i = a; i < b; i++) sum += sm[i];
    cp[k] = (int)(sum / (b - a));
  }
  int mn = cp[0], mx = cp[0];
  for (int i = 1; i < kk; i++) { mn = imin(mn, cp[i]); mx = imax(mx, cp[i]); }
  if (mx - mn < 1) mx = mn + 1;

  // The curve uses only part of the band height, so it stays a calm hill. Rain
  // columns use the full height below, so they can rise above the wave/invert.
  int band = yBot - yTop;
  int ampTop = yBot - band * 55 / 100;

  for (int px = x0; px < x0 + w; px++) {
    int y = horizonCurveY(px, x0, w, cp, kk, mn, mx, ampTop, yBot);
    g.drawFastVLine(px, y, fillTo - y, GxEPD_BLACK);
  }

  // Rain-chance columns: bottom-up, height scaled by chance; colour inverts at
  // the curve so the dotted line reads on both the dark mass and the sky.
  if (v.precip) {
    if (square) {
      // Even horizontal spacing: walk a fixed pixel pitch, interpolating the
      // chance at each column instead of deriving columns from the (unevenly-
      // rounded) hourly indices, so the dotted field stays regular.
      const int pitch = 4;
      for (int px = x0; px < x0 + w; px += pitch) {
        long pos = (long)(px - x0) * (m - 1);
        int i = (int)(pos / (w - 1));
        int rem = (int)(pos % (w - 1));
        int a = iclamp(v.precip[i], 0, 100);
        int b = iclamp(v.precip[imin(i + 1, m - 1)], 0, 100);
        int pv = a + (b - a) * rem / (w - 1);
        if (pv < 20) continue;
        int cy = horizonCurveY(px, x0, w, cp, kk, mn, mx, ampTop, yBot);
        int top = yBot - pv * band / 100;
        for (int y = yBot - 2; y >= top; y -= 3)
          g.drawPixel(px, y, (y > cy) ? GxEPD_WHITE : GxEPD_BLACK);
      }
    } else {
      for (int i = 0; i < m; i++) {
        int pv = iclamp(v.precip[i], 0, 100);
        if (pv < 20) continue;
        int px = x0 + (w - 1) * i / (m - 1);
        int cy = horizonCurveY(px, x0, w, cp, kk, mn, mx, ampTop, yBot);
        int top = yBot - pv * band / 100;
        for (int y = yBot - 2; y >= top; y -= 3)
          g.drawPixel(px, y, (y > cy) ? GxEPD_WHITE : GxEPD_BLACK);
      }
    }
  }

  // Markers where the curve crosses key times of day: a slim 2-wide x 4-tall
  // mark on the wave (pixels flip colour across the curve so it reads on both
  // sides), with a little celestial glyph hanging above — a crescent moon at
  // midnight, a sun at noon, and a half-sun (rising/setting) at sunrise/sunset.
  if (v.hourStart >= 0) {
    int hs = ((v.hourStart % 24) + 24) % 24;
    int sc = imax(1, w / 148);                    // 1 on small panels, 2 on 296
    int hw = sc;                                  // -> ~2*sc wide
    int hh = 2 * sc;                              // -> ~4*sc tall
    int gr = imax(4, w / 40);                     // glyph radius (matches moon)
    for (int i = 0; i < m; i++) {
      int hod = (hs + i) % 24;
      int kind = -1;
      if (hod == 0) kind = 0;                     // midnight -> moon
      else if (hod == 12) kind = 1;               // noon -> sun
      else if (hod == v.sunriseHour) kind = 2;    // sunrise
      else if (hod == v.sunsetHour) kind = 3;     // sunset
      if (kind < 0) continue;
      int px = x0 + (w - 1) * i / (m - 1);
      int cy = horizonCurveY(px, x0, w, cp, kk, mn, mx, ampTop, yBot);
      for (int yy = cy - hh; yy <= cy + hh; yy++) {  // +1px tail below the curve
        if (yy >= fillTo) break;
        for (int xx = px - hw; xx < px + hw; xx++)
          g.drawPixel(xx, yy, (yy > cy) ? GxEPD_WHITE : GxEPD_BLACK);
      }
      int marg = imax(2, sc) + 5;
      int gAbove = (cy - hh) - marg - gr;        // in the white sky
      int gBelow = (cy + hh) + marg + gr;        // in the dark mass
      bool canAbove = gAbove - gr * 3 / 2 >= yTop;
      int disc = gr * 8 / 5;
      if (square) {
        // When the temp is high (curve near the top) drop the glyph below the
        // line into the dark mass; the disc is filled to match its background
        // (white sky / black mass), so it just clears rain dots — no visible box.
        bool highHere = (cy - yTop) * 100 < (yBot - yTop) * 55;
        bool placeBelow = highHere && gBelow + disc < fillTo;
        if (placeBelow) {
          g.fillCircle(px, gBelow, disc, GxEPD_BLACK);
          drawSkyGlyph(g, kind, px, gBelow, gr, GxEPD_WHITE, GxEPD_BLACK);
        } else if (canAbove) {
          g.fillCircle(px, gAbove, disc, GxEPD_WHITE);
          drawSkyGlyph(g, kind, px, gAbove, gr, GxEPD_BLACK, GxEPD_WHITE);
        }
      } else if (canAbove) {  // locked layouts: unchanged
        drawSkyGlyph(g, kind, px, gAbove, gr, GxEPD_BLACK, GxEPD_WHITE);
      }
    }
  }
}

// Daily forecast for the horizon view: high temp, icon, and a single-letter
// weekday initial — white on the dark mass, for the days ahead (from tomorrow).
template <class G>
inline void drawHorizonDays(G& g, int x, int y, int w, int h, const WeatherView& v,
                            const FontSet& F, bool solidIcons = false,
                            bool lightText = false, bool showLow = false) {
  int ahead = v.dayCount - 1;
  if (ahead < 1) return;
  int cols = iclamp(w / 24, 2, ahead);
  int colW = w / cols;
  // lightText swaps the bold day fonts for regular weight (a calmer footer).
  const Pick hiCbold[]  = {{F.f9b, 1}, {F.f7b, 1}};
  const Pick hiClight[] = {{F.f9, 1},  {F.f7, 1}};
  const Pick* hiC = lightText ? hiClight : hiCbold;
  const GFXfont* letterFont = lightText ? F.f6 : F.f6b;

  auto letterOf = [](const DayView& d) {
    char c = (d.label && d.label[0]) ? d.label[0] : ' ';
    if (c >= 'a' && c <= 'z') c -= 32;
    return c;
  };

  if (showLow) {  // taller strip: weekday letter / icon / hi (bold) / lo (regular)
    const Pick hiBold[] = {{F.f9b, 1}, {F.f7b, 1}};
    const Pick loReg[]  = {{F.f9, 1},  {F.f7, 1}};
    Pick hpHi = fitText(g, hiBold, 2, "888", colW * 58 / 100, h * 19 / 100);
    Pick hpLo = fitText(g, loReg, 2, "888", colW * 58 / 100, h * 19 / 100);
    int yLetter = y + h * 10 / 100;
    int iconCY = y + h * 38 / 100;
    int iconS = 19;  // pixel-match the 128x296 daily icon (its colW-6) exactly
    int yHi = y + h * 68 / 100;
    int yLo = y + h * 88 / 100;
    for (int i = 0; i < cols; i++) {
      const DayView& d = v.days[i + 1];
      int cx = x + colW * i + colW / 2;
      char ltr[2] = {letterOf(d), '\0'};
      drawText(g, letterFont, 1, cx, yLetter, 1, ltr, GxEPD_WHITE);
      drawIcon(g, cx, iconCY, iconS, d.code, GxEPD_WHITE, GxEPD_BLACK, solidIcons);
      drawTempBlock(g, hpHi.font, hpHi.size, cx, yHi, d.hi, GxEPD_WHITE, GxEPD_BLACK);
      drawTempBlock(g, hpLo.font, hpLo.size, cx, yLo, d.lo, GxEPD_WHITE, GxEPD_BLACK);
    }
    return;
  }

  Pick hp = fitText(g, hiC, 2, "888", colW * 60 / 100, h * 26 / 100);
  int yHi = y + h * 15 / 100;
  int iconCY = y + h * 46 / 100;
  int iconS = imin(colW - 6, h * 36 / 100);
  int yLetter = y + h * 84 / 100;
  for (int i = 0; i < cols; i++) {
    const DayView& d = v.days[i + 1];
    int cx = x + colW * i + colW / 2;
    drawTempBlock(g, hp.font, hp.size, cx, yHi, d.hi, GxEPD_WHITE, GxEPD_BLACK);
    drawIcon(g, cx, iconCY, iconS, d.code, GxEPD_WHITE, GxEPD_BLACK, solidIcons);
    char ltr[2] = {letterOf(d), '\0'};
    drawText(g, letterFont, 1, cx, yLetter, 1, ltr, GxEPD_WHITE);
  }
}

template <class G>
inline void renderHorizon(G& g, const WeatherView& v, const FontSet& F) {
  const int W = g.width();
  const int H = g.height();
  g.fillScreen(GxEPD_WHITE);
  g.setTextColor(GxEPD_BLACK);
  g.setTextWrap(false);
  const int M = imax(4, W / 24);

  // Square-ish panels (e.g. 200x200) get a dedicated hero treatment so the open
  // sky doesn't read as empty; portrait/landscape keep their existing layout.
  const bool square = W * 100 >= H * 85 && W * 100 <= H * 118;

  // 1) Date/time pinned to the very top — small, no rule. The 6pt fits the date
  //    and time on one line with room to spare on every supported panel.
  int topH = iclamp(H * 8 / 100, 13, 34);
  int hcy = topH / 2;
  if (v.date && v.date[0]) drawText(g, F.f6, 1, M, hcy, 0, v.date, GxEPD_BLACK);
  if (v.time && v.time[0]) drawText(g, F.f6, 1, W - M, hcy, 2, v.time, GxEPD_BLACK);

  // 2) Day forecast sits in the dark mass at the bottom; reserve its band.
  //    Square panels use a taller strip so each day can show its low temp too;
  //    the extra height is taken from the hourly band (the hero stays put).
  int dayH = iclamp(H * 24 / 100, 46, 150);
  if (square) dayH = iclamp(H * 35 / 100, 60, 100);
  int dayTop = H - dayH;

  // 3) The temperature curve is the top edge of the dark mass. Its coldest
  //    point rests at dayTop; its warmest rises a band above. Square lifts the
  //    curve/rain 8px so there's a black breathing gap above the daily strip.
  int curveBot = square ? dayTop - 8 : dayTop;
  int curveTop = imax(topH + H * 6 / 100, dayTop - imax(40, H * 22 / 100));
  // Square: the hero is a short horizontal band, so the curve starts higher and
  // gets a tall, calm sky — which also gives the noon/sunrise sky glyphs the
  // headroom they need above the curve's peak. (Band ~10% taller than before.)
  if (square) curveTop = topH + iclamp(H * 33 / 100, 53, 77);
  // Square only: enlarge *just the curve's drawing region* (6px up, 3px down) so
  // the wave has more room to flow. curveTop itself is left intact below, so the
  // hero band height (heroH) and the daily strip don't move — nothing translates.
  int curveDrawTop = curveTop - (square ? 6 : 0);
  int curveDrawBot = curveBot + (square ? 3 : 0);
  drawTempHorizon(g, 0, W, curveDrawTop, curveDrawBot, H, v, square);

  // 4) Hero: a large, clean icon and the current temperature in the open area
  //    above the curve, with today's high/low in small text to its right (per
  //    the reference). Stack icon-over-temp when tall; place side-by-side when
  //    the band is short (e.g. landscape) so they never collide.
  int heroH = curveTop - topH;
  const Pick tempC[] = {{F.f24b, 3}, {F.f24b, 2}, {F.f24b, 1}, {F.f18b, 1}};
  const Pick hiC[] = {{F.f9b, 1}, {F.f7b, 1}, {F.f6b, 1}};
  const Pick loC[] = {{F.f9, 1}, {F.f7, 1}, {F.f6, 1}};
  const bool hasDay = v.days && v.dayCount > 0;
  if (square) {  // square panel (200x200): a horizontal hero band
    const int contentL = M;
    const int contentW = W - 2 * M;
    const int cy = topH + heroH / 2;
    const bool hasHum = v.humidity >= 0;

    // Layout: [icon] [current temp + today hi/lo, as one group] ... [humidity].
    // Humidity is pinned to the far right (small, in the header/time font); the
    // hi/lo hugs the temperature it describes rather than floating between them.
    //
    // Measure the humidity unit (wavy lines + %) first, so its column reserves
    // exactly the width it needs and the unit is right-aligned to the margin —
    // it can't run off the panel regardless of how wide the waves get.
    char humBuf[8];
    int humPw = 0, humPh = 0, humWavW = 0, humUnitW = 0, humAmp = 0, humPitch = 0;
    if (hasHum) {
      snprintf(humBuf, sizeof(humBuf), "%d%%", iclamp(v.humidity, 0, 100));
      measure(g, F.f6, 1, humBuf, &humPw, &humPh);
      humAmp = imax(2, humPh * 22 / 100);
      humPitch = 2 * humAmp + 2;                 // tighter: ~1px gap between rows
      humWavW = imax(12, humPw);                 // a touch narrower
      humUnitW = imax(humWavW, humPw);
    }
    const int humNudge = 12;                     // pull the unit in from the edge
    int iconW = contentW * 25 / 100;
    int humW = hasHum ? humUnitW + humNudge + imax(6, M) : 0;

    // Icon: ~15% larger than the prior pass, and forced solid (no hollow ring).
    int iconBox = imin(iconW, heroH) * 90 / 100;
    drawIcon(g, contentL + iconW / 2, cy, iconBox, v.code, GxEPD_BLACK,
             GxEPD_WHITE, /*forceSolid=*/true);

    // Current temperature. The space between the icon and humidity is the temp's
    // home; it sits left-of-centre there so its hi/lo can hug its right side.
    int midL = contentL + iconW;
    int midR = contentL + contentW - humW;
    int midW = midR - midL;
    int tempW = midW * 60 / 100;
    int tempCx = midL + tempW / 2;
    Pick tp = fitText(g, tempC, 4, "888", tempW * 98 / 100, heroH * 90 / 100);
    drawTempBlock(g, tp.font, tp.size, tempCx, cy, v.temperature, GxEPD_BLACK,
                  GxEPD_WHITE);

    if (hasDay) {  // hi/lo immediately to the right of the temperature
      Pick hp = fitText(g, hiC, 3, "88", midW * 26 / 100, heroH * 38 / 100);
      Pick lp = fitText(g, loC, 3, "88", midW * 26 / 100, heroH * 38 / 100);
      int tw, th;
      char tb[8];
      snprintf(tb, sizeof(tb), "%d", v.temperature);
      measure(g, tp.font, tp.size, tb, &tw, &th);
      int adv = textAdvance(g, tp.font, tp.size, tb);
      int degR = imax(1, th * 9 / 100);
      int tempRight = tempCx + adv / 2 + imax(1, degR / 2) + 2 * degR;
      int t2, hh;
      measure(g, hp.font, hp.size, "88", &t2, &hh);
      int vgap = imax(5, hh * 55 / 100);            // vertical air between hi/lo
      int hiloLeft = tempRight + imax(8, W / 18);   // small gap, grouped w/ temp
      int blockW = hiLoWidth(g, hp, lp, v.days[0].hi, v.days[0].lo);
      drawHiLo(g, hp, lp, hiloLeft + blockW, cy, v.days[0].hi, v.days[0].lo,
               GxEPD_BLACK, GxEPD_WHITE, vgap);
    }

    if (hasHum) {  // VStack: a couple of mini zigzag rows above the % (header font)
      const int rows = 2;
      int hcx = contentL + contentW - humNudge - humUnitW / 2;
      int zagsH = (rows - 1) * humPitch + 2 * humAmp;
      int gap = 2;
      int vtotal = zagsH + gap + humPh;
      int top = cy - vtotal / 2;
      iconWaves(g, hcx, top + zagsH / 2, humWavW, humAmp, humPitch, GxEPD_BLACK,
                rows);
      drawText(g, F.f6, 1, hcx, top + zagsH + gap + humPh / 2 + 2, 1, humBuf,
               GxEPD_BLACK);
    }
  } else if (heroH >= W * 34 / 100) {  // tall enough to stack icon over temp
    int iconBox = imin(W * 62 / 100, heroH * 56 / 100);
    drawIcon(g, W / 2, topH + heroH * 34 / 100, iconBox, v.code, GxEPD_BLACK, GxEPD_WHITE);
    int tempCY = topH + heroH * 80 / 100;
    if (hasDay) {  // temp in the left region, hi/lo anchored just to its right
      int contentL = M, contentW = W - 2 * M;
      int tempRegionW = contentW * 68 / 100;
      int hiloRegionW = contentW - tempRegionW;
      int tempCx = contentL + tempRegionW / 2;
      Pick tp = fitText(g, tempC, 4, "888", tempRegionW * 90 / 100, heroH * 36 / 100);
      Pick hp = fitText(g, hiC, 3, "88", hiloRegionW * 84 / 100, heroH * 17 / 100);
      Pick lp = fitText(g, loC, 3, "88", hiloRegionW * 84 / 100, heroH * 17 / 100);
      drawTempBlock(g, tp.font, tp.size, tempCx, tempCY, v.temperature, GxEPD_BLACK,
                    GxEPD_WHITE);
      drawHiLoAfterTemp(g, tp, hp, lp, tempCx, tempCY, v.temperature, v.days[0].hi,
                        v.days[0].lo, contentL + contentW, imax(8, W / 14));
    } else {
      Pick tp = fitText(g, tempC, 4, "888", W * 64 / 100, heroH * 36 / 100);
      drawTempBlock(g, tp.font, tp.size, W / 2, tempCY, v.temperature, GxEPD_BLACK,
                    GxEPD_WHITE);
    }
  } else {  // short band -> icon left, temp (+hi/lo) right, centered as a group
    int cy = topH + heroH / 2;
    int iconBox = imin(heroH * 84 / 100, W * 22 / 100);
    drawIcon(g, W * 28 / 100, cy, iconBox, v.code, GxEPD_BLACK, GxEPD_WHITE);
    if (hasDay) {
      int regionLeft = W * 40 / 100;
      int regionW = (W - M) - regionLeft;
      int tempRegionW = regionW * 64 / 100;
      int hiloRegionW = regionW - tempRegionW;
      int tempCx = regionLeft + tempRegionW / 2;
      Pick tp = fitText(g, tempC, 4, "888", tempRegionW * 92 / 100, heroH * 80 / 100);
      Pick hp = fitText(g, hiC, 3, "88", hiloRegionW * 88 / 100, heroH * 40 / 100);
      Pick lp = fitText(g, loC, 3, "88", hiloRegionW * 88 / 100, heroH * 40 / 100);
      drawTempBlock(g, tp.font, tp.size, tempCx, cy, v.temperature, GxEPD_BLACK,
                    GxEPD_WHITE);
      drawHiLoAfterTemp(g, tp, hp, lp, tempCx, cy, v.temperature, v.days[0].hi,
                        v.days[0].lo, W - M, imax(6, W / 22));
    } else {
      Pick tp = fitText(g, tempC, 4, "888", W * 34 / 100, heroH * 80 / 100);
      drawTempBlock(g, tp.font, tp.size, W * 64 / 100, cy, v.temperature, GxEPD_BLACK,
                    GxEPD_WHITE);
    }
  }

  // 5) Daily highs + icons along the bottom of the dark mass. On square panels,
  //    nudge the strip down ~2px for a touch more breathing room between it and
  //    the hourly curve/rain above.
  if (square)
    drawHorizonDays(g, 0, dayTop + 4, W, dayH - 4, v, F, /*solidIcons=*/false,
                    /*lightText=*/true, /*showLow=*/true);
  else
    drawHorizonDays(g, 0, dayTop, W, dayH, v, F);
}

// ---- Dispatch ---------------------------------------------------------------

enum class LayoutStyle { Dashboard, Horizon };

template <class G>
inline void renderWeather(G& g, const WeatherView& v, const FontSet& F,
                          LayoutStyle style = LayoutStyle::Dashboard) {
  switch (style) {
    case LayoutStyle::Horizon: renderHorizon(g, v, F); break;
    default: renderDashboard(g, v, F); break;
  }
}

}  // namespace atmo
