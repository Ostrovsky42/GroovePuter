// modeled after a minimal subset from https://docs.m5stack.com/en/arduino/m5gfx/m5gfx_functions
#pragma once
#ifndef GROOVEPUTER_DISPLAY_H
#define GROOVEPUTER_DISPLAY_H
#include <cstdint>
class IGfxColor {
public:

    // ===== Constructors =====
    IGfxColor() : color_(0x000000) {}                        // Default: black (RGB888)
    constexpr IGfxColor(uint32_t rgb888) : color_(rgb888) {} // From 24-bit RGB888

    // ===== Public Methods =====

    /// Convert internal 24-bit color to RGB565 (optimized)
    constexpr uint16_t color16() const {
        return rgb888_to_565(color_);
    }

    constexpr uint16_t toCardputerColor() const {
        uint16_t c = color16();
        return (c >> 8) | (c << 8);   // swap byte order for cardputer
    }

    constexpr uint32_t color24() const {
        return color_;
    }

    // ===== Common Colors =====
    static constexpr IGfxColor Black()   { return IGfxColor(0x000000); }
    static constexpr IGfxColor White()   { return IGfxColor(0xFFFFFF); }
    static constexpr IGfxColor Red()     { return IGfxColor(0xFF0000); }
    static constexpr IGfxColor Green()   { return IGfxColor(0x00FF00); }
    static constexpr IGfxColor Blue()    { return IGfxColor(0x0000FF); }
    static constexpr IGfxColor Yellow()  { return IGfxColor(0xFFFF00); }
    static constexpr IGfxColor Cyan()    { return IGfxColor(0x00FFFF); }
    static constexpr IGfxColor Magenta() { return IGfxColor(0xFF00FF); }
    static constexpr IGfxColor Gray()    { return IGfxColor(0x808080); }
    static constexpr IGfxColor DarkGray(){ return IGfxColor(0x404040); }
    static constexpr IGfxColor Orange()  { return IGfxColor(0xFFA500); }
  static constexpr IGfxColor Purple()  { return IGfxColor(0x800080); }

private:
    uint32_t color_;  // Stored as 24-bit RGB888 (0xRRGGBB)

    // ===== Private Compile-Time Converter =====
    static constexpr uint16_t rgb888_to_565(uint32_t c) {
        uint8_t r = (c >> 16) & 0xFF;
        uint8_t g = (c >>  8) & 0xFF;
        uint8_t b =  c        & 0xFF;
        return ((r >> 3) << 11) |
               ((g >> 2) << 5)  |
               (b >> 3);
    }
};

enum class GfxFont {
    kFont5x7 = 0,
    kFreeSerif18pt,
    kFreeMono24pt,
};

class IGfx {
public:
    virtual void begin() = 0;
    virtual void clear(IGfxColor color) = 0;
    virtual void drawPixel(int x, int y, IGfxColor color) = 0;
    virtual void drawText(int x, int y, const char* text) = 0;
    virtual void drawImage(int x, int y, const uint16_t* pixels,
                           int w, int h) = 0;
    virtual void drawRect(int x, int y, int w, int h, IGfxColor color) = 0;
    virtual void drawCircle(int x, int y, int r, IGfxColor color) = 0;
    virtual void drawKnobFace(int cx, int cy, int radius, IGfxColor ringColor,
                              IGfxColor bgColor) = 0;
    virtual void fillRect(int x, int y, int w, int h, IGfxColor color) = 0;
    virtual void fillCircle(int x, int y, int r, IGfxColor color) = 0;
    virtual void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, IGfxColor color) = 0;
    virtual void setRotation(int rot) = 0;
    virtual void setTextColor(IGfxColor color) = 0;
    virtual void setTextColor(uint16_t color) = 0;
    virtual void setFont(GfxFont font) = 0;
    virtual void startWrite() = 0;
    virtual void endWrite() = 0;
    virtual void flush() = 0; // present buffered content, if applicable
    virtual int textWidth(const char* text) const = 0;
    virtual int measureText(const char* text) const { return textWidth(text); } // UI Kit compatibility alias
    virtual int fontHeight() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual ~IGfx() = default;
};

#endif // GROOVEPUTER_DISPLAY_H
