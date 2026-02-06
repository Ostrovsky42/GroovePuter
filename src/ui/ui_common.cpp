#include "ui_common.h"
#include "ui_utils.h"
#include "ui_widgets.h"
#include "../dsp/grooveputer_engine.h"
#ifndef USE_RETRO_THEME
#define USE_RETRO_THEME
#endif
#ifndef USE_AMBER_THEME
#define USE_AMBER_THEME
#endif
#include "retro_ui_theme.h"
#include "amber_ui_theme.h"
#include <cstdio>

namespace UI {

    // Global overlay state
    WaveformOverlayState waveformOverlay;
    VisualStyle currentStyle = VisualStyle::RETRO_CLASSIC;

    // Internal state for wave history (compact overlay version)
    namespace {
        constexpr int kOverlayMaxPoints = 256;
        constexpr int kOverlayHistoryLayers = 2; // Reduced from 4 for performance
        int16_t overlayHistory[kOverlayHistoryLayers][kOverlayMaxPoints];
        int overlayLengths[kOverlayHistoryLayers] = {0};
        
        constexpr IGfxColor kOverlayFadeColors[] = {
            IGfxColor(0x808080),  // Brightest fade
            IGfxColor(0x404040),
            IGfxColor(0x202020),
        };
        constexpr int kFadeColorCount = 3;

        char gToastMsg[64] = {0};
        unsigned long gToastEndMs = 0;
    }

    void drawStandardHeader(IGfx& gfx, GroovePuter& mini_acid, const char* title) {
        char sceneStr[16];
        snprintf(sceneStr, sizeof(sceneStr), "%02d", mini_acid.currentScene() + 1);
        
        LayoutManager::drawHeader(gfx, sceneStr, (int)mini_acid.bpm(), 
                                 title, mini_acid.isRecording());
    }

    void drawStandardFooter(IGfx& gfx, const char* left, const char* right) {
        LayoutManager::drawFooter(gfx, left, right);
    }

    void drawVerticalList(IGfx& gfx, int x, int y, int width,
                          const char* const* items, int itemCount,
                          int selectedIndex, bool hasFocus,
                          int iconIndex) {
        for (int i = 0; i < itemCount; i++) {
            int rowY = y + i * Layout::LINE_HEIGHT;
            bool selected = (i == selectedIndex) && hasFocus;
            bool hasIcon = (i == iconIndex);
            Widgets::drawListRow(gfx, x, rowY, width, items[i], selected, hasIcon);
        }
    }

    void drawChannelActivityBar(IGfx& gfx, int x, int y, int width, int height,
                                const bool* activeFlags, int channelCount) {
        if (channelCount <= 0) return;

        const int gap = 2;
        const int segmentW = (width - gap * (channelCount - 1)) / channelCount;

        for (int i = 0; i < channelCount; ++i) {
            int sx = x + i * (segmentW + gap);

            if (activeFlags[i]) {
                gfx.fillRect(sx, y, segmentW, height, COLOR_KNOB_1);
            } else {
                gfx.drawRect(sx, y, segmentW, height, COLOR_DARKER);
            }
        }
    }

    void drawButtonGridHelper(IGfx& gfx, int x, int y,
                              const char* const* labels, int labelsCount,
                              int selectedIndex, bool hasFocus) {
        Widgets::drawButtonGrid(
            gfx,
            x, y,
            58, 10,
            2, 4,
            labels,
            labelsCount,
            hasFocus ? selectedIndex : -1
        );
    }

    void drawWaveformOverlay(IGfx& gfx, GroovePuter& mini_acid) {
        if (!waveformOverlay.enabled) return;
        
        // Compact dimensions at bottom of screen - increased height for better visibility
        const int h = 24; 
        const int y = Layout::FOOTER.y - h - 2;
        const int x = 8;
        const int w = Layout::FOOTER.w - 12;

        if (w < 10 || h < 4) return;

        // Get waveform buffer (thread-safe)
        const auto& waveBuffer = mini_acid.getWaveformBuffer();
        
        const int midY = y + h / 2;
        const int amplitude = h / 2 - 2; 
        int points = w < kOverlayMaxPoints ? w : kOverlayMaxPoints;

        // 1) Reference center line (matches page)
        gfx.drawLine(x, midY, x + w - 1, midY, COLOR_WAVE);

        // 2) Update wave history
        if (waveBuffer.count > 1 && points > 1) {
            for (int layer = kOverlayHistoryLayers - 1; layer > 0; --layer) {
                overlayLengths[layer] = overlayLengths[layer - 1];
                for (int px = 0; px < overlayLengths[layer]; ++px) {
                    overlayHistory[layer][px] = overlayHistory[layer - 1][px];
                }
            }

            overlayLengths[0] = points;
            for (int px = 0; px < points; ++px) {
                // Shared sampling math with Page
                size_t idx = static_cast<size_t>((uint64_t)px * (waveBuffer.count - 1) / (points - 1));
                overlayHistory[0][px] = waveBuffer.data[idx];
            }
        }

        // 3) Draw helper
        auto drawWave = [&](const int16_t* wave, int len, IGfxColor color) {
            if (len < 2) return;
            int drawLen = len < w ? len : w;
            for (int px = 0; px < drawLen - 1; ++px) {
                // High visual gain (3.5x) for compact overlay to ensure "dance"
                float s0 = (wave[px] * 3.5f) / 32768.0f;
                float s1 = (wave[px + 1] * 3.5f) / 32768.0f;
                if (s0 > 1.0f) s0 = 1.0f; else if (s0 < -1.0f) s0 = -1.0f;
                if (s1 > 1.0f) s1 = 1.0f; else if (s1 < -1.0f) s1 = -1.0f;
                
                int y0 = midY - static_cast<int>(s0 * amplitude);
                int y1 = midY - static_cast<int>(s1 * amplitude);
                drawLineColored(gfx, x + px, y0, x + px + 1, y1, color);
            }
        };

        // 4) Draw history (reduced layers)
        if (kOverlayHistoryLayers > 1) {
            drawWave(overlayHistory[1], overlayLengths[1], kOverlayFadeColors[0]);
        }

        // 5) Draw current (synchronized color)
        IGfxColor waveColor = WAVE_COLORS[waveformOverlay.colorIndex % NUM_WAVE_COLORS];
        drawWave(overlayHistory[0], overlayLengths[0], waveColor);
    }

    void drawMutesOverlay(IGfx& gfx, GroovePuter& mini_acid) {
        // Mutes Overlay v3: Compact, Numbered, Themed
        
        // 1. Setup Theme Colors
        IGfxColor kActive, kMuted, kIdle;
        
        if (currentStyle == VisualStyle::MINIMAL) {
            kActive = COLOR_WHITE;
            kMuted = COLOR_RED;
            kIdle = IGfxColor(0x404040); // Dark Gray
        } else if (currentStyle == VisualStyle::RETRO_CLASSIC) {
            kActive = IGfxColor(RetroTheme::NEON_CYAN);
            kMuted = IGfxColor(RetroTheme::STATUS_ACCENT); // Red/Pink
            kIdle = IGfxColor(RetroTheme::TEXT_DIM);
        } else { // AMBER
            kActive = IGfxColor(AmberTheme::NEON_CYAN);
            kMuted = IGfxColor(AmberTheme::NEON_ORANGE);
            kIdle = IGfxColor(AmberTheme::TEXT_DIM);
        }

        // 2. Position (Bottom Right)
        const int itemW = 8;     // Reduced width for numbers
        const int spacing = 2;
        const int totalW = (10 * itemW) + (9 * spacing);
        const int x = gfx.width() - totalW - 4; // Right aligned
        const int y = Layout::FOOTER.y - 10;
        
        // 3. Draw Loop
        for (int i = 0; i < 10; ++i) {
            int cx = x + i * (itemW + spacing);
            
            // Determine Mute Status
            bool muted = false;
            switch(i) {
                case 0: muted = mini_acid.is303Muted(0); break;
                case 1: muted = mini_acid.is303Muted(1); break;
                case 2: muted = mini_acid.isKickMuted(); break;
                case 3: muted = mini_acid.isSnareMuted(); break;
                case 4: muted = mini_acid.isHatMuted(); break;
                case 5: muted = mini_acid.isOpenHatMuted(); break;
                case 6: muted = mini_acid.isMidTomMuted(); break;
                case 7: muted = mini_acid.isHighTomMuted(); break;
                case 8: muted = mini_acid.isRimMuted(); break;
                case 9: muted = mini_acid.isClapMuted(); break;
            }
            
            // Determine Color & Style
            bool active = mini_acid.isTrackActive(i);
            IGfxColor color = kIdle;
            
            if (muted) {
                color = kMuted;
            } else if (active) {
                color = kActive;
            }
            
            // Draw Number
            char num[2]; 
            snprintf(num, sizeof(num), "%d", (i + 1) % 10);
            
            gfx.setTextColor(color);
            gfx.drawText(cx, y, num);
            
            // TODO: Optional: Subtle underline for playing tracks to indicate activity even if not muted
            /*
            if (!muted && active && mini_acid.isPlaying()) {
                 gfx.drawPixel(cx + 2, y + 9, color);
            }
            */
        }
    }

    void drawFeelOverlay(IGfx& gfx, GroovePuter& mini_acid, bool pulse) {
        const auto& feel = mini_acid.sceneManager().currentScene().feel;
        int grid = feel.gridSteps;
        if (grid != 8 && grid != 16 && grid != 32) grid = 16;
        int bars = feel.patternBars;
        if (bars != 1 && bars != 2 && bars != 4 && bars != 8) bars = 1;
        int tb = feel.timebase;
        if (tb < 0) tb = 0;
        if (tb > 2) tb = 2;

        const char* gridStr = (grid == 8) ? "1/8" : (grid == 32) ? "1/32" : "1/16";
        const char* tbStr = (tb == 0) ? "H" : (tb == 2) ? "D" : "N";
        char buf[20];
        snprintf(buf, sizeof(buf), "G%s T%s L%dB", gridStr, tbStr, bars);

        const int x = Layout::CONTENT_PAD_X;
        const int y = Layout::FOOTER.y - 10;

        IGfxColor textColor = COLOR_LABEL;
        if (currentStyle == VisualStyle::RETRO_CLASSIC) {
            textColor = IGfxColor(RetroTheme::TEXT_SECONDARY);
        } else if (currentStyle == VisualStyle::AMBER) {
            textColor = IGfxColor(AmberTheme::TEXT_SECONDARY);
        }

        if (pulse) {
            int w = gfx.textWidth(buf);
            gfx.fillRect(x - 2, y - 1, w + 4, 10, COLOR_ACCENT);
            gfx.setTextColor(COLOR_BLACK);
        } else {
            gfx.setTextColor(textColor);
        }

        gfx.drawText(x, y, buf);
    }

    void drawFeelHeaderHud(IGfx& gfx, GroovePuter& mini_acid, int x, int y) {
        (void)x;
        (void)y;
        const auto& feel = mini_acid.sceneManager().currentScene().feel;
        int grid = feel.gridSteps;
        if (grid != 8 && grid != 16 && grid != 32) grid = 16;
        int bars = feel.patternBars;
        if (bars != 1 && bars != 2 && bars != 4 && bars != 8) bars = 1;
        int tb = feel.timebase;
        if (tb < 0) tb = 0;
        if (tb > 2) tb = 2;

        char tbChar = (tb == 0) ? 'H' : (tb == 2) ? 'D' : 'N';
        char buf[20];
        snprintf(buf, sizeof(buf), "G%d T%c L%d", grid, tbChar, bars);

        // Draw as right-aligned chip inside header, clipped and isolated from title text.
        const int chipW = 72;
        const int chipH = 9;
        const int chipX = Layout::HEADER.x + Layout::HEADER.w - chipW - 14; // keep REC area free
        const int chipY = 3;

        gfx.fillRect(chipX, chipY, chipW, chipH, COLOR_BLACK);
        gfx.setTextColor(COLOR_LABEL);
        Widgets::drawClippedText(gfx, chipX, chipY, chipW, buf);
    }

    void showToast(const char* msg, int durationMs) {
        if (!msg) return;
        snprintf(gToastMsg, sizeof(gToastMsg), "%s", msg);
        gToastEndMs = millis() + durationMs;
    }

    void drawToast(IGfx& gfx) {
        if (millis() < gToastEndMs) {
            int w = gfx.width();
            int tw = gfx.textWidth(gToastMsg);
            int x = (w - tw) / 2;
            int y = gfx.height() - 25;
            gfx.fillRect(x - 4, y - 2, tw + 8, 11, COLOR_BLACK);
            gfx.drawRect(x - 4, y - 2, tw + 8, 11, COLOR_KNOB_2);
            gfx.setTextColor(COLOR_WHITE);
            gfx.drawText(x, y, gToastMsg);
        }
    }

}
