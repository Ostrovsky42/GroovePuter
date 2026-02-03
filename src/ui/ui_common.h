#pragma once
#include "ui_core.h"
#include "layout_manager.h"
#include "ui_widgets.h"

// Forward declaration if needed, but ui_core/layout_manager should cover it
class MiniAcid;

namespace UI {
    
    /**
     * State for the waveform overlay (global toggle).
     */
    struct WaveformOverlayState {
        bool enabled = true;
        int colorIndex = 0;   // Current wave color index
    };

    // Global overlay state (extern, defined in ui_common.cpp)
    extern WaveformOverlayState waveformOverlay;
    extern VisualStyle currentStyle;

    // Standard wave colors used by both page and overlay
    constexpr int kNumWaveColors = 5;
    constexpr IGfxColor kWaveColors[kNumWaveColors] = {
        IGfxColor(0x00FF90), // COLOR_WAVE (Spring Green)
        IGfxColor::Cyan(),
        IGfxColor::Magenta(),
        IGfxColor::Yellow(),
        IGfxColor::White()
    };

    /**
     * Draws standard header with scene number, BPM, and recording status.
     */
    void drawStandardHeader(IGfx& gfx, MiniAcid& mini_acid, const char* title);

    /**
     * Draws standard footer with left and optional right text.
     */
    void drawStandardFooter(IGfx& gfx, const char* left, const char* right = nullptr);

    /**
     * Draws a vertical list of items with selection and focus highlighting.
     */
    void drawVerticalList(IGfx& gfx, int x, int y, int width,
                          const char* const* items, int itemCount,
                          int selectedIndex, bool hasFocus,
                          int iconIndex = -1);

    /**
     * Draws a horizontal bar reflecting track activity.
     */
    void drawChannelActivityBar(IGfx& gfx, int x, int y, int width, int height,
                                const bool* activeFlags, int channelCount);

    /**
     * Helper to draw a button grid with standard layout parameters.
     */
    void drawButtonGridHelper(IGfx& gfx, int x, int y,
                              const char* const* labels, int labelsCount,
                              int selectedIndex, bool hasFocus);

    /**
     * Draws a compact waveform overlay at the bottom of the screen.
     * Uses dimmed colors for pseudo-transparency.
     */
    void drawWaveformOverlay(IGfx& gfx, MiniAcid& mini_acid);

}
