#include "cardputer_display.h"
// FreeFont sources:
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Fonts/FreeMono24pt7b.h
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Fonts/FreeSerif18pt7b.h
// 5x7 font source (Adafruit GFX default):
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/glcdfont.c
#if defined(ARDUINO) && __has_include(<M5GFX.h>)
  #include <M5GFX.h>
  namespace cardputer_fonts = lgfx::v1::fonts;
#else
  #include "fonts/FreeMono24pt7b.h"
  #include "fonts/FreeSerif18pt7b.h"
  namespace cardputer_fonts {
    using ::FreeMono24pt7b;
    using ::FreeSerif18pt7b;
  }
#endif
#include "fonts/Adafruit5x7.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>

#if defined(ARDUINO)
  #if __has_include(<M5Cardputer.h>)
    #include <M5Cardputer.h>
  #elif __has_include(<M5Stack.h>)
    #include <M5Stack.h>
  #endif
#endif

CardputerDisplay::CardputerDisplay() : w_(320), h_(240) {}

CardputerDisplay::~CardputerDisplay() = default;

void CardputerDisplay::begin() {
#if defined(ARDUINO) && __has_include(<M5Cardputer.h>)
  // M5Cardputer initialization is normally done by the sketch; just query size
  w_ = M5Cardputer.Display.width();
  h_ = M5Cardputer.Display.height();
#elif defined(ARDUINO) && __has_include(<M5Stack.h>)
  M5.begin();
  #if defined(M5_LCD_AVAILABLE)
  w_ = M5.Lcd.width();
  h_ = M5.Lcd.height();
  #endif
#else
  // Desktop / fallback: nothing to initialize
#endif
  frame_.assign(w_ * h_, IGfxColor::Black().toCardputerColor());
  flush();
}

void CardputerDisplay::clear(IGfxColor color) {
  uint16_t c = color.toCardputerColor();
  std::fill(frame_.begin(), frame_.end(), c);
}

void CardputerDisplay::drawPixel(int x, int y, IGfxColor color) {
  if (x < 0 || x >= w_ || y < 0 || y >= h_) return;
  if (frame_.empty()) return;
  frame_[y * w_ + x] = color.toCardputerColor();
}

void CardputerDisplay::drawGlyph5x7(int x, int y, unsigned char glyph_idx) {
  const uint8_t* bitmap = adafruit_5x7::kFont5x7[glyph_idx];
  for (int col = 0; col < 5; ++col) {
    uint8_t bits = bitmap[col];
    for (int row = 0; row < 7; ++row) {
      if (bits & (1 << row)) {
        int px = x + col;
        int py = y + row;
        if (px >= 0 && px < w_ && py >= 0 && py < h_ && !frame_.empty()) {
          frame_[py * w_ + px] = text_color565_;
        }
      }
    }
  }
}

void CardputerDisplay::drawText(int x, int y, const char* text) {
  if (!text) return;
  if (!gfx_font_) {
    int cursor_x = x;
    int cursor_y = y;
    while (*text) {
      char c = *text++;
      if (c == '\n') {
        cursor_x = x;
        cursor_y += adafruit_5x7::kFont5x7GlyphHeight;
        continue;
      }
      unsigned char glyph = 0;
      if (c < 0x20 || c > 0x7F) glyph = '?' - 0x20;
      else glyph = static_cast<unsigned char>(c - 0x20);
      drawGlyph5x7(cursor_x, cursor_y, glyph);
      cursor_x += adafruit_5x7::kFont5x7GlyphWidth;
    }
    return;
  }

  int cursor_x = x;
  int cursor_y = y + gfx_metrics_.ascent;
  while (*text) {
    char c = *text++;
    if (c == '\n') {
      cursor_x = x;
      cursor_y += gfx_metrics_.line_height;
      continue;
    }
    if (c < gfx_font_->first || c > gfx_font_->last) c = '?';
    unsigned char glyph = static_cast<unsigned char>(c - gfx_font_->first);
    drawGfxGlyph(cursor_x, cursor_y, glyph);
    const GFXglyph& g = gfx_font_->glyph[glyph];
    cursor_x += g.xAdvance;
  }
}

void CardputerDisplay::drawGfxGlyph(int x, int y, unsigned char glyph_idx) {
  if (!gfx_font_ || frame_.empty()) return;
  uint16_t glyph_count = gfx_font_->last - gfx_font_->first + 1;
  if (glyph_idx >= glyph_count) return;

  const GFXglyph& glyph = gfx_font_->glyph[glyph_idx];
  uint16_t bo = glyph.bitmapOffset;
  const uint8_t* bitmap = gfx_font_->bitmap;
  int w = glyph.width;
  int h = glyph.height;
  int xo = glyph.xOffset;
  int yo = glyph.yOffset;

  uint8_t bits = 0;
  int bit_count = 0;
  for (int yy = 0; yy < h; ++yy) {
    for (int xx = 0; xx < w; ++xx) {
      if (bit_count == 0) {
        bits = pgm_read_byte(bitmap + bo++);
        bit_count = 8;
      }
      if (bits & 0x80) {
        int px = x + xo + xx;
        int py = y + yo + yy;
        if (px >= 0 && px < w_ && py >= 0 && py < h_) {
          frame_[py * w_ + px] = text_color565_;
        }
      }
      bits <<= 1;
      --bit_count;
    }
  }
}

void CardputerDisplay::drawImage(int x, int y, const uint16_t* pixels, int w, int h) {
  if (!pixels || frame_.empty()) return;
  for (int row = 0; row < h; ++row) {
    int dst_y = y + row;
    if (dst_y < 0 || dst_y >= h_) continue;
    for (int col = 0; col < w; ++col) {
      int dst_x = x + col;
      if (dst_x < 0 || dst_x >= w_) continue;
      frame_[dst_y * w_ + dst_x] = pixels[row * w + col];
    }
  }
}

void CardputerDisplay::drawRect(int x, int y, int w, int h, IGfxColor color) {
  if (frame_.empty()) return;
  uint16_t c = color.toCardputerColor();
  int x0 = std::max(0, x);
  int y0 = std::max(0, y);
  int x1 = std::min(w_ - 1, x + w - 1);
  int y1 = std::min(h_ - 1, y + h - 1);
  for (int xx = x0; xx <= x1; ++xx) {
    frame_[y0 * w_ + xx] = c;
    frame_[y1 * w_ + xx] = c;
  }
  for (int yy = y0; yy <= y1; ++yy) {
    frame_[yy * w_ + x0] = c;
    frame_[yy * w_ + x1] = c;
  }
}

void CardputerDisplay::drawCircle(int x, int y, int r, IGfxColor color) {
  if (r < 0) return;
  uint16_t c = color.toCardputerColor();
  if (frame_.empty()) return;

  auto plot = [&](int px, int py) {
    if (px >= 0 && px < w_ && py >= 0 && py < h_) frame_[py * w_ + px] = c;
  };

  int f = 1 - r;
  int ddF_x = 1;
  int ddF_y = -2 * r;
  int xx = 0;
  int yy = r;
  plot(x, y + r);
  plot(x, y - r);
  plot(x + r, y);
  plot(x - r, y);

  while (xx < yy) {
    if (f >= 0) {
      yy--;
      ddF_y += 2;
      f += ddF_y;
    }
    xx++;
    ddF_x += 2;
    f += ddF_x;
    plot(x + xx, y + yy);
    plot(x - xx, y + yy);
    plot(x + xx, y - yy);
    plot(x - xx, y - yy);
    plot(x + yy, y + xx);
    plot(x - yy, y + xx);
    plot(x + yy, y - xx);
    plot(x - yy, y - xx);
  }
}

void CardputerDisplay::drawKnobFace(int cx, int cy, int radius, IGfxColor ringColor,
                                    IGfxColor bgColor) {
  if (radius <= 0) return;
  if (frame_.empty()) return;

  // Keep knob rendering allocation-free on Cardputer to avoid runtime aborts
  // in DRAM-constrained states (e.g. page transitions).
  fillCircle(cx, cy, radius, bgColor);
  drawCircle(cx, cy, radius, ringColor);
}

void CardputerDisplay::fillRect(int x, int y, int w, int h, IGfxColor color) {
  if (frame_.empty()) return;
  uint16_t c = color.toCardputerColor();
  int x0 = std::max(0, x);
  int y0 = std::max(0, y);
  int x1 = std::min(w_ - 1, x + w - 1);
  int y1 = std::min(h_ - 1, y + h - 1);
  for (int yy = y0; yy <= y1; ++yy) {
    for (int xx = x0; xx <= x1; ++xx) {
      frame_[yy * w_ + xx] = c;
    }
  }
}

void CardputerDisplay::fillCircle(int x, int y, int r, IGfxColor color) {
  if (r < 0) return;
  if (frame_.empty()) return;
  uint16_t c = color.toCardputerColor();

  auto drawHLine = [&](int x0, int x1, int py) {
    if (py < 0 || py >= h_) return;
    int startX = std::max(0, x0);
    int endX = std::min(w_ - 1, x1);
    for (int px = startX; px <= endX; ++px) {
      frame_[py * w_ + px] = c;
    }
  };

  int f = 1 - r;
  int ddF_x = 1;
  int ddF_y = -2 * r;
  int xx = 0;
  int yy = r;
  
  drawHLine(x - r, x + r, y);
  
  while (xx < yy) {
    if (f >= 0) {
      yy--;
      ddF_y += 2;
      f += ddF_y;
    }
    xx++;
    ddF_x += 2;
    f += ddF_x;
    
    drawHLine(x - xx, x + xx, y + yy);
    drawHLine(x - xx, x + xx, y - yy);
    drawHLine(x - yy, x + yy, y + xx);
    drawHLine(x - yy, x + yy, y - xx);
  }
}

void CardputerDisplay::setRotation(int rot) {
#if defined(ARDUINO) && __has_include(<M5Cardputer.h>)
  M5Cardputer.Display.setRotation(rot);
  w_ = M5Cardputer.Display.width();
  h_ = M5Cardputer.Display.height();
#elif defined(ARDUINO) && __has_include(<M5Stack.h>) && defined(M5_LCD_AVAILABLE)
  M5.Lcd.setRotation(rot);
  w_ = M5.Lcd.width();
  h_ = M5.Lcd.height();
#else
  (void)rot;
  return;
#endif
  frame_.assign(w_ * h_, IGfxColor::Black().toCardputerColor());
}

void CardputerDisplay::setTextColor(IGfxColor color) {
  text_color_ = color;
  text_color565_ = text_color_.toCardputerColor();
}

void CardputerDisplay::setTextColor(uint16_t color) {
  // Construct best-effort IGfxColor? No, stored separately.
  // Actually, text_color565_ is what matters for drawing.
  text_color565_ = color;
  // We can't easily reconstruct IGfxColor (RGB888) from 565 without loss/assumptions,
  // but text_color_ is mostly for getters if we had them.
  // We can convert 565 back to 888 for consistency.
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = color & 0x1F;
  // Expand to 888
  uint32_t rgb = ((r * 255 / 31) << 16) | ((g * 255 / 63) << 8) | (b * 255 / 31);
  text_color_ = IGfxColor(rgb);
}

CardputerDisplay::FontMetrics CardputerDisplay::computeMetrics(const GFXfont& font) const {
  FontMetrics m;
  m.line_height = font.yAdvance;
  int ascent = 0;
  int descent = 0;
  uint16_t count = font.last - font.first + 1;
  for (uint16_t i = 0; i < count; ++i) {
    const GFXglyph& g = font.glyph[i];
    ascent = std::max(ascent, -static_cast<int>(g.yOffset));
    descent = std::max(descent, static_cast<int>(g.height) + static_cast<int>(g.yOffset));
  }
  m.ascent = ascent;
  m.descent = descent;
  if (m.line_height == 0) m.line_height = ascent + descent;
  return m;
}

void CardputerDisplay::setFont(GfxFont font) {
  font_ = font;
  gfx_font_ = nullptr;
  switch (font) {
    case GfxFont::kFont5x7:
      break;
    case GfxFont::kFreeSerif18pt:
      gfx_font_ = &cardputer_fonts::FreeSerif18pt7b;
      break;
    case GfxFont::kFreeMono24pt:
      gfx_font_ = &cardputer_fonts::FreeMono24pt7b;
      break;
  }
  if (gfx_font_) gfx_metrics_ = computeMetrics(*gfx_font_);
}

void CardputerDisplay::startWrite() {
#if defined(ARDUINO) && __has_include(<M5Cardputer.h>)
  M5Cardputer.Display.startWrite();
#elif defined(ARDUINO) && __has_include(<M5Stack.h>) && defined(M5_LCD_AVAILABLE)
  M5.Lcd.startWrite();
#endif
}

void CardputerDisplay::endWrite() {
#if defined(ARDUINO) && __has_include(<M5Cardputer.h>)
  M5Cardputer.Display.endWrite();
#elif defined(ARDUINO) && __has_include(<M5Stack.h>) && defined(M5_LCD_AVAILABLE)
  M5.Lcd.endWrite();
#endif
}

void CardputerDisplay::flush() {
  if (frame_.empty()) return;
#if defined(ARDUINO) && __has_include(<M5Cardputer.h>)
  M5Cardputer.Display.pushImage(0, 0, w_, h_, frame_.data());
#elif defined(ARDUINO) && __has_include(<M5Stack.h>) && defined(M5_LCD_AVAILABLE)
  M5.Lcd.pushImage(0, 0, w_, h_, frame_.data());
#endif
}

void CardputerDisplay::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, IGfxColor color) {
  uint16_t c = color.toCardputerColor();
  if (frame_.empty()) return;
  int dx = abs((int)(x1 - x0));
  int sx = x0 < x1 ? 1 : -1;
  int dy = -abs((int)(y1 - y0));
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    if (x0 >= 0 && x0 < w_ && y0 >= 0 && y0 < h_) {
      frame_[y0 * w_ + x0] = c;
    }
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

int CardputerDisplay::textWidth(const char* text) const {
  if (!text) return 0;
  int max_w = 0;
  int line_w = 0;
  if (!gfx_font_) {
    while (*text) {
      char c = *text++;
      if (c == '\n') {
        if (line_w > max_w) max_w = line_w;
        line_w = 0;
        continue;
      }
      line_w += adafruit_5x7::kFont5x7GlyphWidth;
    }
  } else {
    while (*text) {
      char c = *text++;
      if (c == '\n') {
        if (line_w > max_w) max_w = line_w;
        line_w = 0;
        continue;
      }
      if (c < gfx_font_->first || c > gfx_font_->last) c = '?';
      const GFXglyph& g = gfx_font_->glyph[c - gfx_font_->first];
      line_w += g.xAdvance;
    }
  }
  if (line_w > max_w) max_w = line_w;
  return max_w;
}

int CardputerDisplay::fontHeight() const {
  if (!gfx_font_) return adafruit_5x7::kFont5x7GlyphHeight;
  return gfx_metrics_.line_height;
}

int CardputerDisplay::width() const { return w_; }
int CardputerDisplay::height() const { return h_; }
