#include "sdl_display.h"
// FreeFont sources:
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Fonts/FreeMono24pt7b.h
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Fonts/FreeSerif18pt7b.h
// 5x7 font source (Adafruit GFX default):
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/glcdfont.c
#include "../fonts/FreeMono24pt7b.h"
#include "../fonts/FreeSerif18pt7b.h"
#include "../fonts/Adafruit5x7.h"
#include <algorithm>
#include <iterator>
#if __has_include(<SDL2/SDL.h>)
  #include <SDL2/SDL.h>
#else
  #include <SDL.h>
#endif
#if __has_include(<SDL2/SDL2_gfxPrimitives.h>)
  #include <SDL2/SDL2_gfxPrimitives.h>
#else
  #include <SDL2_gfxPrimitives.h>
#endif
#include <iostream>

SDLDisplay::SDLDisplay(int w, int h, const char* title)
    : w_(w), h_(h), title_(title) {}

SDLDisplay::~SDLDisplay() {
  if (render_target_) {
    if (renderer_) {
      SDL_SetRenderTarget(renderer_, nullptr);
    }
    SDL_DestroyTexture(render_target_);
    render_target_ = nullptr;
  }
  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  if (owns_sdl_) SDL_Quit();
}

void SDLDisplay::begin() {
  if (SDL_WasInit(0) == 0) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
      return;
    }
    owns_sdl_ = true;
  }

  window_ = SDL_CreateWindow(title_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           w_ * window_scale_, h_ * window_scale_, SDL_WINDOW_SHOWN);
  if (!window_) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
    return;
  }

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer_) {
    std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
    return;
  }

    // Force nearest neighbor scaling (no blur)
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  // Create render target for low-res rendering
  render_target_ = SDL_CreateTexture(
      renderer_,
      SDL_PIXELFORMAT_RGBA8888,
      SDL_TEXTUREACCESS_TARGET,
      w_, h_
  );

  // Set renderer to draw into the small texture
  SDL_SetRenderTarget(renderer_, render_target_);


  clear(IGfxColor::Black());
}

void SDLDisplay::clear(IGfxColor color) {
  if (!renderer_) return;
  setDrawColor(renderer_, color);
  SDL_RenderClear(renderer_);
}

void SDLDisplay::drawPixel(int x, int y, IGfxColor color) {
  if (!renderer_) return;
  if (x < 0 || x >= w_ || y < 0 || y >= h_) return;
  setDrawColor(renderer_, color);
  SDL_RenderDrawPoint(renderer_, x, y);
}

void SDLDisplay::drawText(int x, int y, const char* text) {
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
      if (c < 0x20 || c > 0x7F) {
        glyph = '?' - 0x20;
      } else {
        glyph = static_cast<unsigned char>(c - 0x20);
      }
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

void SDLDisplay::drawGfxGlyph(int x, int y, unsigned char glyph_idx) {
  if (!renderer_ || !gfx_font_) return;
  uint16_t glyph_count = gfx_font_->last - gfx_font_->first + 1;
  if (glyph_idx >= glyph_count) return;

  const GFXglyph& glyph = gfx_font_->glyph[glyph_idx];
  uint16_t bo = glyph.bitmapOffset;
  const uint8_t* bitmap = gfx_font_->bitmap;
  int w = glyph.width;
  int h = glyph.height;
  int xo = glyph.xOffset;
  int yo = glyph.yOffset;

  setDrawColor(renderer_, text_color_);
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
          SDL_RenderDrawPoint(renderer_, px, py);
        }
      }
      bits <<= 1;
      --bit_count;
    }
  }
}

void SDLDisplay::drawImage(int x, int y, const uint16_t* pixels, int w, int h) {
  if (!renderer_ || !pixels) return;
  for (int row = 0; row < h; ++row) {
    int dst_y = y + row;
    if (dst_y < 0 || dst_y >= h_) continue;
    for (int col = 0; col < w; ++col) {
      int dst_x = x + col;
      if (dst_x < 0 || dst_x >= w_) continue;
      uint16_t src = pixels[row * w + col];
      setDrawColor565(renderer_, src);
      SDL_RenderDrawPoint(renderer_, dst_x, dst_y);
    }
  }
}

void SDLDisplay::fillRect(int x, int y, int w, int h, IGfxColor color) {
  if (!renderer_) return;
  SDL_Rect r{ x, y, w, h };
  setDrawColor(renderer_, color);
  SDL_RenderFillRect(renderer_, &r);
}

void SDLDisplay::setRotation(int rot) {
  (void)rot; // rotation not applicable for SDL stub
}

void SDLDisplay::setTextColor(IGfxColor color) {
  text_color_ = color;
}

SDLDisplay::FontMetrics SDLDisplay::computeMetrics(const GFXfont& font) const {
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

void SDLDisplay::setFont(GfxFont font) {
  font_ = font;
  gfx_font_ = nullptr;
  switch (font) {
    case GfxFont::kFont5x7:
      break;
    case GfxFont::kFreeSerif18pt:
      gfx_font_ = &FreeSerif18pt7b;
      break;
    case GfxFont::kFreeMono24pt:
      gfx_font_ = &FreeMono24pt7b;
      break;
  }
  if (gfx_font_) gfx_metrics_ = computeMetrics(*gfx_font_);
}

void SDLDisplay::startWrite() {
  // SDL renderer path does not need explicit transaction bracketing.
}

void SDLDisplay::endWrite() {
  if (!renderer_) return;

  // Go back to window
  SDL_SetRenderTarget(renderer_, nullptr);

  // Destination rectangle (scaled to window)
  SDL_Rect dst{0, 0, w_ * window_scale_, h_ * window_scale_};

  // Copy small render texture to window
  SDL_RenderCopy(renderer_, render_target_, nullptr, &dst);

  SDL_RenderPresent(renderer_);

  // Switch back to render target for next frame's drawing
  SDL_SetRenderTarget(renderer_, render_target_);
}

void SDLDisplay::flush() {
}

void SDLDisplay::drawGlyph5x7(int x, int y, unsigned char glyph_idx) {
  if (!renderer_) return;
  const uint8_t* bitmap = adafruit_5x7::kFont5x7[glyph_idx];
  for (int col = 0; col < 5; ++col) {
    uint8_t bits = bitmap[col];
    for (int row = 0; row < 7; ++row) {
      if (bits & (1 << row)) {
        drawPixel(x + col, y + row, text_color_);
      }
    }
  }
}

void SDLDisplay::drawRect(int x, int y, int w, int h, IGfxColor color) {
  if (!renderer_) return;
  SDL_Rect r{ x, y, w, h };
  setDrawColor(renderer_, color);
  SDL_RenderDrawRect(renderer_, &r);
}

void SDLDisplay::fillCircle(int x, int y, int radius, IGfxColor color) {
  if (!renderer_ || radius <= 0) return;
  setDrawColor(renderer_, color);

  // Midpoint circle algorithm for filled circle
  // We draw horizontal lines between the points on the circumference
  int cx = x;
  int cy = y;
  int xo = radius;
  int yo = 0;
  int err = 0;

  auto drawHLine = [&](int x1, int x2, int y) {
      if (x1 > x2) std::swap(x1, x2);
      SDL_RenderDrawLine(renderer_, x1, y, x2, y);
  };

  while (xo >= yo) {
      drawHLine(cx - xo, cx + xo, cy + yo);
      drawHLine(cx - xo, cx + xo, cy - yo);
      drawHLine(cx - yo, cx + yo, cy + xo);
      drawHLine(cx - yo, cx + yo, cy - xo);

      if (err <= 0) {
          yo += 1;
          err += 2 * yo + 1;
      }
      if (err > 0) {
          xo -= 1;
          err -= 2 * xo + 1;
      }
  }
}

void SDLDisplay::drawCircle(int x, int y, int r, IGfxColor color) {
  if (!renderer_) return;
  uint32_t packed = color.color24();
  uint8_t rr = (packed >> 16) & 0xFF;
  uint8_t gg = (packed >> 8) & 0xFF;
  uint8_t bb = packed & 0xFF;
  circleRGBA(renderer_, x, y, r, rr, gg, bb, 255);
}

void SDLDisplay::drawKnobFace(int cx, int cy, int radius, IGfxColor ringColor,
                              IGfxColor bgColor) {
  if (!renderer_ || radius <= 0) return;
  uint16_t ring565 = ringColor.color16();
  uint16_t bg565 = bgColor.color16();

  auto it = std::find_if(knob_faces_.begin(), knob_faces_.end(),
                         [&](const KnobFaceCache& cache) {
                           return cache.matches(radius, ring565, bg565);
                         });

  if (it == knob_faces_.end()) {
    knob_faces_.push_back({});
    KnobFaceCache& cache = knob_faces_.back();
    cache.radius = radius;
    cache.ring_color = ring565;
    cache.bg_color = bg565;
    int size = radius * 2 + 1;
    cache.pixels.assign(size * size, bg565);

    auto plot = [&](int px, int py) {
      if (px >= 0 && px < size && py >= 0 && py < size) {
        cache.pixels[py * size + px] = ring565;
      }
    };

    int cx0 = radius;
    int cy0 = radius;
    int f = 1 - radius;
    int ddF_x = 1;
    int ddF_y = -2 * radius;
    int xx = 0;
    int yy = radius;
    plot(cx0, cy0 + radius);
    plot(cx0, cy0 - radius);
    plot(cx0 + radius, cy0);
    plot(cx0 - radius, cy0);

    while (xx < yy) {
      if (f >= 0) {
        yy--;
        ddF_y += 2;
        f += ddF_y;
      }
      xx++;
      ddF_x += 2;
      f += ddF_x;
      plot(cx0 + xx, cy0 + yy);
      plot(cx0 - xx, cy0 + yy);
      plot(cx0 + xx, cy0 - yy);
      plot(cx0 - xx, cy0 - yy);
      plot(cx0 + yy, cy0 + xx);
      plot(cx0 - yy, cy0 + xx);
      plot(cx0 + yy, cy0 - xx);
      plot(cx0 - yy, cy0 - xx);
    }

    it = std::prev(knob_faces_.end());
  }

  const KnobFaceCache& cache = *it;
  int size = cache.radius * 2 + 1;
  drawImage(cx - cache.radius, cy - cache.radius, cache.pixels.data(), size, size);
}

void SDLDisplay::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, IGfxColor color) {
  if (!renderer_) return;
  setDrawColor(renderer_, color);
  SDL_RenderDrawLine(renderer_, x0, y0, x1, y1);
}

int SDLDisplay::textWidth(const char* text) const {
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
      (void)c;
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

int SDLDisplay::fontHeight() const {
  if (!gfx_font_) return adafruit_5x7::kFont5x7GlyphHeight;
  return gfx_metrics_.line_height;
}

void SDLDisplay::setDrawColor(SDL_Renderer* renderer, IGfxColor color) {
  uint32_t rgb = color.color24();
  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >> 8) & 0xFF;
  uint8_t b = rgb & 0xFF;
  SDL_SetRenderDrawColor(renderer, r, g, b, 255);
}

void SDLDisplay::setDrawColor565(SDL_Renderer* renderer, uint16_t rgb565) {
  uint8_t r = ((rgb565 >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((rgb565 >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (rgb565 & 0x1F) * 255 / 31;
  SDL_SetRenderDrawColor(renderer, r, g, b, 255);
}
