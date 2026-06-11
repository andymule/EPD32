// Host-side reimplementation of the Adafruit GFX drawing API, faithful enough
// that text laid out here matches the e-paper panel pixel-for-pixel (it uses the
// real GFXfont data and the same glyph-rendering algorithm). Used only by the
// desktop preview harness; the device uses the real GxEPD2/Adafruit_GFX.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#define PROGMEM
#include "gfxfont.h"

// Match GxEPD: BLACK == 0x0000 draws an inked pixel, WHITE clears it.
#ifndef GxEPD_BLACK
#define GxEPD_BLACK 0x0000
#define GxEPD_WHITE 0xFFFF
#endif

class HostGFX {
 public:
  HostGFX(int w, int h) : W_(w), H_(h), buf_(static_cast<size_t>(w) * h, 255) {}

  int16_t width() const { return W_; }
  int16_t height() const { return H_; }
  const std::vector<uint8_t>& buffer() const { return buf_; }

  void fillScreen(uint16_t color) {
    std::fill(buf_.begin(), buf_.end(), color == GxEPD_BLACK ? 0 : 255);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || y < 0 || x >= W_ || y >= H_) return;
    buf_[static_cast<size_t>(y) * W_ + x] = (color == GxEPD_BLACK) ? 0 : 255;
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    for (int16_t i = 0; i < w; i++) drawPixel(x + i, y, color);
  }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    for (int16_t i = 0; i < h; i++) drawPixel(x, y + i, color);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    for (int16_t j = 0; j < h; j++) drawFastHLine(x, y + j, w, color);
  }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
  }

  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }
    int16_t dx = x1 - x0, dy = std::abs(y1 - y0);
    int16_t err = dx / 2, ystep = (y0 < y1) ? 1 : -1;
    for (; x0 <= x1; x0++) {
      if (steep) drawPixel(y0, x0, color); else drawPixel(x0, y0, color);
      err -= dy;
      if (err < 0) { y0 += ystep; err += dx; }
    }
  }

  void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corner, uint16_t color) {
    int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
    while (x < y) {
      if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
      x++; ddF_x += 2; f += ddF_x;
      if (corner & 0x4) { drawPixel(x0 + x, y0 + y, color); drawPixel(x0 + y, y0 + x, color); }
      if (corner & 0x2) { drawPixel(x0 + x, y0 - y, color); drawPixel(x0 + y, y0 - x, color); }
      if (corner & 0x8) { drawPixel(x0 - y, y0 + x, color); drawPixel(x0 - x, y0 + y, color); }
      if (corner & 0x1) { drawPixel(x0 - y, y0 - x, color); drawPixel(x0 - x, y0 - y, color); }
    }
  }
  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    drawPixel(x0, y0 + r, color); drawPixel(x0, y0 - r, color);
    drawPixel(x0 + r, y0, color); drawPixel(x0 - r, y0, color);
    drawCircleHelper(x0, y0, r, 0xF, color);
  }
  void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corner, int16_t delta, uint16_t color) {
    int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
    int16_t px = x, py = y;
    delta++;
    while (x < y) {
      if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
      x++; ddF_x += 2; f += ddF_x;
      if (x < (y + 1)) {
        if (corner & 1) drawFastVLine(x0 + x, y0 - y, 2 * y + delta, color);
        if (corner & 2) drawFastVLine(x0 - x, y0 - y, 2 * y + delta, color);
      }
      if (y != py) {
        if (corner & 1) drawFastVLine(x0 + py, y0 - px, 2 * px + delta, color);
        if (corner & 2) drawFastVLine(x0 - py, y0 - px, 2 * px + delta, color);
        py = y;
      }
      px = x;
    }
  }
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    drawFastVLine(x0, y0 - r, 2 * r + 1, color);
    fillCircleHelper(x0, y0, r, 3, 0, color);
  }

  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    int16_t maxr = ((w < h) ? w : h) / 2;
    if (r > maxr) r = maxr;
    drawFastHLine(x + r, y, w - 2 * r, color);
    drawFastHLine(x + r, y + h - 1, w - 2 * r, color);
    drawFastVLine(x, y + r, h - 2 * r, color);
    drawFastVLine(x + w - 1, y + r, h - 2 * r, color);
    drawCircleHelper(x + r, y + r, r, 1, color);
    drawCircleHelper(x + w - r - 1, y + r, r, 2, color);
    drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
    drawCircleHelper(x + r, y + h - r - 1, r, 8, color);
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    int16_t maxr = ((w < h) ? w : h) / 2;
    if (r > maxr) r = maxr;
    fillRect(x + r, y, w - 2 * r, h, color);
    fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
    fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
  }

  // --- Text (custom GFXfont path; matches Adafruit_GFX) ---
  void setFont(const GFXfont* f) { font_ = f; }
  void setTextSize(uint8_t s) { tsx_ = tsy_ = s ? s : 1; }
  void setTextColor(uint16_t c) { fg_ = c; bg_ = c; }
  void setTextColor(uint16_t c, uint16_t bg) { fg_ = c; bg_ = bg; }
  void setTextWrap(bool w) { wrap_ = w; }
  void setCursor(int16_t x, int16_t y) { cx_ = x; cy_ = y; }
  int16_t getCursorX() const { return cx_; }
  int16_t getCursorY() const { return cy_; }

  void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t, uint8_t sx, uint8_t sy) {
    if (!font_ || c < font_->first || c > font_->last) return;
    const GFXglyph* g = &font_->glyph[c - font_->first];
    const uint8_t* bmp = font_->bitmap;
    uint16_t bo = g->bitmapOffset;
    uint8_t w = g->width, h = g->height, bits = 0, bit = 0;
    int8_t xo = g->xOffset, yo = g->yOffset;
    for (uint8_t yy = 0; yy < h; yy++) {
      for (uint8_t xx = 0; xx < w; xx++) {
        if (!(bit++ & 7)) bits = bmp[bo++];
        if (bits & 0x80) {
          if (sx == 1 && sy == 1) drawPixel(x + xo + xx, y + yo + yy, color);
          else fillRect(x + (xo + xx) * sx, y + (yo + yy) * sy, sx, sy, color);
        }
        bits <<= 1;
      }
    }
  }

  size_t write(uint8_t c) {
    if (!font_) return 1;
    if (c == '\n') { cx_ = 0; cy_ += tsy_ * font_->yAdvance; }
    else if (c != '\r' && c >= font_->first && c <= font_->last) {
      const GFXglyph* g = &font_->glyph[c - font_->first];
      if (g->width > 0 && g->height > 0) {
        int16_t xo = g->xOffset;
        if (wrap_ && (cx_ + tsx_ * (xo + g->width)) > W_) { cx_ = 0; cy_ += tsy_ * font_->yAdvance; }
        drawChar(cx_, cy_, c, fg_, bg_, tsx_, tsy_);
      }
      cx_ += tsx_ * g->xAdvance;
    }
    return 1;
  }
  void print(const char* s) { while (*s) write((uint8_t)*s++); }
  void print(const std::string& s) { print(s.c_str()); }
  void print(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); print(b); }

  void getTextBounds(const char* str, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1, cx = x, cy = y;
    *x1 = x; *y1 = y; *w = 0; *h = 0;
    for (const char* p = str; *p; p++) charBounds(*p, &cx, &cy, &minx, &miny, &maxx, &maxy);
    if (maxx >= minx) { *x1 = minx; *w = (uint16_t)(maxx - minx + 1); }
    if (maxy >= miny) { *y1 = miny; *h = (uint16_t)(maxy - miny + 1); }
  }

 private:
  void charBounds(unsigned char c, int16_t* x, int16_t* y, int16_t* minx, int16_t* miny, int16_t* maxx, int16_t* maxy) {
    if (!font_) return;
    if (c == '\n') { *x = 0; *y += tsy_ * font_->yAdvance; return; }
    if (c == '\r' || c < font_->first || c > font_->last) return;
    const GFXglyph* g = &font_->glyph[c - font_->first];
    uint8_t gw = g->width, gh = g->height, xa = g->xAdvance;
    int8_t xo = g->xOffset, yo = g->yOffset;
    if (wrap_ && ((*x + ((int16_t)xo + gw) * tsx_) > W_)) { *x = 0; *y += tsy_ * font_->yAdvance; }
    int16_t tsx = tsx_, tsy = tsy_;
    int16_t gx1 = *x + xo * tsx, gy1 = *y + yo * tsy;
    int16_t gx2 = gx1 + gw * tsx - 1, gy2 = gy1 + gh * tsy - 1;
    if (gx1 < *minx) *minx = gx1;
    if (gy1 < *miny) *miny = gy1;
    if (gx2 > *maxx) *maxx = gx2;
    if (gy2 > *maxy) *maxy = gy2;
    *x += xa * tsx;
  }

  int W_, H_;
  std::vector<uint8_t> buf_;
  const GFXfont* font_ = nullptr;
  int16_t cx_ = 0, cy_ = 0;
  uint16_t fg_ = 0, bg_ = 0xFFFF;
  uint8_t tsx_ = 1, tsy_ = 1;
  bool wrap_ = true;
};
