#pragma once
#include "../display.h"
#include "../gfx_font.h"
#include <vector>

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

class SDLDisplay : public IGfx {
public:
  SDLDisplay(int w = 320, int h = 240, const char* title = "AcidCardputer SDL");
  ~SDLDisplay() override;

  void begin() override;
  void clear(IGfxColor color) override;
  void drawPixel(int x, int y, IGfxColor color) override;
  void drawText(int x, int y, const char* text) override;
  void drawImage(int x, int y, const uint16_t* pixels, int w, int h) override;
  void fillRect(int x, int y, int w, int h, IGfxColor color) override;
  void drawRect(int x, int y, int w, int h, IGfxColor color) override;
  void drawCircle(int x, int y, int r, IGfxColor color) override;
  void fillCircle(int x, int y, int r, IGfxColor color) override;
  void drawKnobFace(int cx, int cy, int radius, IGfxColor ringColor,
                    IGfxColor bgColor) override;
  void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, IGfxColor color) override;
  void setRotation(int rot) override;
  void setTextColor(IGfxColor color) override;
  void setFont(GfxFont font) override;
  void startWrite() override;
  void endWrite() override;
  void flush() override;
  int textWidth(const char* text) const override;
  int fontHeight() const override;
  int width() const override { return w_; }
  int height() const override { return h_; }
  int windowScale() const { return window_scale_; }

private:
  struct FontMetrics {
    int line_height = 0;
    int ascent = 0;
    int descent = 0;
  };
  struct KnobFaceCache {
    int radius = 0;
    uint16_t ring_color = 0;
    uint16_t bg_color = 0;
    std::vector<uint16_t> pixels;

    bool matches(int r, uint16_t ring, uint16_t bg) const {
      return radius == r && ring_color == ring && bg_color == bg && !pixels.empty();
    }
  };

  int w_;
  int h_;
  const char* title_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  IGfxColor text_color_ = IGfxColor::White();
  bool owns_sdl_ = false;
  GfxFont font_ = GfxFont::kFont5x7;
  const GFXfont* gfx_font_ = nullptr;
  FontMetrics gfx_metrics_;
  std::vector<KnobFaceCache> knob_faces_;
  int window_scale_ = 2;
  SDL_Texture* render_target_ = nullptr;

  static void setDrawColor(SDL_Renderer* renderer, IGfxColor color);
  static void setDrawColor565(SDL_Renderer* renderer, uint16_t rgb565);
  void drawGlyph5x7(int x, int y, unsigned char glyph_idx);
  void drawGfxGlyph(int x, int y, unsigned char glyph_idx);
  FontMetrics computeMetrics(const GFXfont& font) const;
};
