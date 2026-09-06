#pragma once
#include "ui_core.h"
#include "layout_manager.h"
#include "ui_widgets.h"
#include "ui_config.h"
#include "ui_status_chrome.h"
#include "ui_shell_frame.h"

// Forward declaration if needed, but ui_core/layout_manager should cover it
#include "src/dsp/miniacid_engine.h"

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
    
    // Page count moved to ui_config.h

    // Standard wave colors used by both page and overlay
    constexpr int kNumWaveColors = 5;
    constexpr IGfxColor kWaveColors[kNumWaveColors] = {
        COLOR_INFO,
        COLOR_ACCENT,
        COLOR_WARN,
        COLOR_DANGER,
        COLOR_TEXT
    };

    /**
     * Legacy page call-site retained during U1F migration. Global header pixels
     * are shell-owned, so this helper performs no drawing and no live reads.
     */
    void drawStandardHeader(IGfx& gfx, MiniAcid& mini_acid, const char* title);

    /**
     * Publishes the effective page footer into the active shell-frame model.
     * The shell owns the actual footer pixels after page composition completes.
     */
    void drawStandardFooter(IGfx& gfx, const char* left, const char* right = nullptr);

    /**
     * Binds one stack-local frame model while the active page composes. Page
     * helpers may only publish presentation data into this value; they do not
     * own shell pixels.
     */
    void beginShellFrameModel(UiShellFrameModel& model);
    void endShellFrameModel();
    void publishShellFooter(const char* left, const char* right = nullptr);
    void drawShellFooter(IGfx& gfx, const UiFooterModel& footer);

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

    /**
     * Draws a compact mutes/activity overlay.
     */
    void drawMutesOverlay(IGfx& gfx, MiniAcid& mini_acid);

    /**
     * Draws a compact FEEL overlay (Grid + Cycle length) with optional pulse.
     */
    void drawFeelOverlay(IGfx& gfx, MiniAcid& mini_acid, bool pulse);

    /**
     * Owns the complete bottom performance strip for one frame. Clears stale
     * pixels first, then draws waveform, feel and mute activity in a stable
     * back-to-front order.
     */
    void drawPerformanceHud(IGfx& gfx, MiniAcid& mini_acid, bool feelPulse);

    /**
     * Global toast (single line).
     */
    void showToast(const char* msg, int durationMs = 1500);
    void drawToast(IGfx& gfx);

    /**
     * Mini HUD for G/T/L in header area.
     */
    void drawFeelHeaderHud(IGfx& gfx, MiniAcid& mini_acid, int x, int y);

    /**
     * Captures the bounded status projection once from authoritative runtime
     * state. The returned snapshot is independent of renderer residency and is
     * small enough to live on the stack for one frame.
     */
    UiStatusSnapshot captureUiStatusSnapshot(MiniAcid& mini_acid,
                                             UiStatusContext context);

    /**
     * Draws the one-line global context/status chrome inside the existing
     * 16-pixel header from an already captured snapshot. Rendering is pure with
     * respect to MiniAcid: no second live runtime read is performed here.
     */
    void drawStatusChrome(IGfx& gfx, const UiStatusSnapshot& status);

    /**
     * Compatibility hook retained for external callers. MiniAcidDisplay uses
     * drawStatusChrome directly once U1F gives the shell sole header ownership.
     */
    void drawLiveMixLockBadge(IGfx& gfx, const UiStatusSnapshot& status);

}
