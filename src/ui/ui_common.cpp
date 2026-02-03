#include "ui_common.h"
#include "ui_utils.h"
#include "../dsp/miniacid_engine.h"
#include <cstdio>

namespace UI {

    // Global overlay state
    WaveformOverlayState waveformOverlay;
    VisualStyle currentStyle = VisualStyle::MINIMAL;

    // Internal state for wave history (compact overlay version)
    namespace {
        constexpr int kOverlayMaxPoints = 256;
        constexpr int kOverlayHistoryLayers = 4;
        int16_t overlayHistory[kOverlayHistoryLayers][kOverlayMaxPoints];
        int overlayLengths[kOverlayHistoryLayers] = {0};
        
        constexpr IGfxColor kOverlayFadeColors[] = {
            IGfxColor(0x808080),  // Brightest fade
            IGfxColor(0x404040),
            IGfxColor(0x202020),
        };
        constexpr int kFadeColorCount = 3;
    }

    void drawStandardHeader(IGfx& gfx, MiniAcid& mini_acid, const char* title) {
        char sceneStr[8];
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

    void drawWaveformOverlay(IGfx& gfx, MiniAcid& mini_acid) {
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

        // 4) Draw history
        for (int layer = kOverlayHistoryLayers - 1; layer >= 1; --layer) {
            int colorIdx = layer - 1;
            if (colorIdx >= kFadeColorCount) colorIdx = kFadeColorCount - 1;
            drawWave(overlayHistory[layer], overlayLengths[layer], kOverlayFadeColors[colorIdx]);
        }

        // 5) Draw current (synchronized color)
        IGfxColor waveColor = WAVE_COLORS[waveformOverlay.colorIndex % NUM_WAVE_COLORS];
        drawWave(overlayHistory[0], overlayLengths[0], waveColor);
    }

}
