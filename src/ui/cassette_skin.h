#pragma once

#include <cstdint>
#include "display.h"
#include "ui_core.h"
#include "ui_themes.h"
#include <vector>

// ============================================================================
// Cassette Skin - Visual wrapper for vintage tape deck aesthetic
// ============================================================================
// Usage:
//   skin.drawBackground();
//   skin.drawHeader(state);
//   skin.drawPanelFrame(bounds);
//   page.drawContent();
//   skin.drawFooterReels(footerState);

struct HeaderState {
    const char* sceneName = "A01";
    int bpm = 120;
    const char* patternName = "D1";
    bool isNavMode = true;      // NAV vs EDIT
    bool isRecording = false;
    bool tapeEnabled = false;
    bool fxEnabled = false;
    bool songMode = false;
    int swingPercent = 50;
};

struct FooterState {
    int currentStep = 0;        // 0-15
    int totalSteps = 16;
    int songPosition = 0;       // in bars
    int loopPosition = -1;      // -1 = no loop
    bool isPlaying = false;
    uint8_t animFrame = 0;      // 0-2 for reel animation
};

class CassetteSkin {
public:
    CassetteSkin(IGfx& gfx, CassetteTheme theme = CassetteTheme::WarmTape);

    // Set active theme
    void setTheme(CassetteTheme theme);
    CassetteTheme theme() const { return theme_; }
    const CassettePalette& palette() const { return *palette_; }

    // Drawing methods
    void drawBackground();
    void drawHeader(const HeaderState& state);
    void drawPanelFrame(const Rect& bounds);
    void drawFooterReels(const FooterState& state);
    void drawFocusRect(const Rect& rect, bool editMode);

    // Layout helpers
    int headerHeight() const { return 16; }  // Reduced from 18 for more content space
    int footerHeight() const { return 10; }  // Minimized - every pixel matters!
    Rect contentBounds() const;

    // Call once per frame to advance reel animation
    void tick();
    uint8_t animFrame() const { return animFrame_; }

private:
    void drawDitherPattern(const Rect& bounds, IGfxColor base, IGfxColor dark);
    void drawReel(int cx, int cy, int radius);
    void drawTapeProgress(int x, int y, int width, int progress, int total);
    void drawLedDot(int x, int y, bool active);
    void drawDoubleBorder(const Rect& bounds);
    void drawCornerScrew(int x, int y);

    IGfx& gfx_;
    CassetteTheme theme_;
    const CassettePalette* palette_;
    uint8_t animFrame_ = 0;
    uint8_t tickCounter_ = 0;

    // Memory-safe background caching (Line-based)
    std::vector<uint16_t> linePlain_; // Plain base color
    std::vector<uint16_t> lineEven_;  // Dither Line 0 (Dark at 0, 2, 4...)
    std::vector<uint16_t> lineOdd_;   // Dither Line 2 (Dark at 1, 3, 5...)
    uint32_t lastBgColor_ = 0xFFFFFFFF;
};
