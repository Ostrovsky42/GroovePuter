#pragma once
#include "display.h"
#include "gfx_font.h"
#include <vector>

class CardputerDisplay : public IGfx {
public:
  CardputerDisplay();
  ~CardputerDisplay() override;

  void begin() override;
  void clear(IGfxColor color) override;
  void drawPixel(int x, int y, IGfxColor color) override;
  void drawText(int x, int y, const char* text) override;
  void drawImage(int x, int y, const uint16_t* pixels, int w, int h) override;
  void drawRect(int x, int y, int w, int h, IGfxColor color) override;
  void drawCircle(int x, int y, int r, IGfxColor color) override;
  void drawKnobFace(int cx, int cy, int radius, IGfxColor ringColor,
                    IGfxColor bgColor) override;
  void fillRect(int x, int y, int w, int h, IGfxColor color) override;
  void fillCircle(int x, int y, int r, IGfxColor color) override;
  void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, IGfxColor color) override;
  void setRotation(int rot) override;
  void setTextColor(IGfxColor color) override;
  void setTextColor(uint16_t color) override;
  void setFont(GfxFont font) override;
  void startWrite() override;
  void endWrite() override;
  void flush() override;
  int textWidth(const char* text) const override;
  int fontHeight() const override;
  int width() const override;
  int height() const override;

private:
  struct FontMetrics {
    int line_height = 0;
    int ascent = 0;
    int descent = 0;
  };

  int w_;
  int h_;
  IGfxColor text_color_ = IGfxColor::White();
  uint16_t text_color565_ = text_color_.color16();
  std::vector<uint16_t> frame_; // RGB565 back buffer
  GfxFont font_ = GfxFont::kFont5x7;
  const GFXfont* gfx_font_ = nullptr;
  FontMetrics gfx_metrics_;

  void drawGlyph5x7(int x, int y, unsigned char glyph_idx);
  void drawGfxGlyph(int x, int y, unsigned char glyph_idx);
  FontMetrics computeMetrics(const GFXfont& font) const;
};
