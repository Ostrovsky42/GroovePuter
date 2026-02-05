#include "ui_common.h"
#include "ui_utils.h"
#include "../dsp/miniacid_engine.h"
#include <cstdio>

namespace UI {

    // Global overlay state
    WaveformOverlayState waveformOverlay;
    VisualStyle currentStyle = VisualStyle::RETRO_CLASSIC;

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

    void drawMutesOverlay(IGfx& gfx, MiniAcid& mini_acid) {
        if (currentStyle != VisualStyle::RETRO_CLASSIC) {
            // Restore Original Minimal Style (Block-based)
            const int h = 10;
            const int y = Layout::FOOTER.y - 12;
            const int x = 8;
            const int w = Layout::FOOTER.w - 16;
            const int numTracks = 10;
            const int gap = 2;
            const int segmentW = (w - (numTracks - 1) * gap) / numTracks;

            for (int i = 0; i < numTracks; ++i) {
                int sx = x + i * (segmentW + gap);
                bool muted = false;
                bool active = mini_acid.isTrackActive(i);
                IGfxColor color = COLOR_GRAY;

                if (i == 0) {
                    muted = mini_acid.is303Muted(0);
                    color = COLOR_KNOB_1; // Orange
                } else if (i == 1) {
                    muted = mini_acid.is303Muted(1);
                    color = COLOR_KNOB_2; // Cyan
                } else {
                    int drumIdx = i - 2;
                    switch (drumIdx) {
                        case 0: muted = mini_acid.isKickMuted(); color = COLOR_DRUM_KICK; break;
                        case 1: muted = mini_acid.isSnareMuted(); color = COLOR_DRUM_SNARE; break;
                        case 2: muted = mini_acid.isHatMuted(); color = COLOR_DRUM_HAT; break;
                        case 3: muted = mini_acid.isOpenHatMuted(); color = COLOR_DRUM_OPEN_HAT; break;
                        case 4: muted = mini_acid.isMidTomMuted(); color = COLOR_DRUM_MID_TOM; break;
                        case 5: muted = mini_acid.isHighTomMuted(); color = COLOR_DRUM_HIGH_TOM; break;
                        case 6: muted = mini_acid.isRimMuted(); color = COLOR_DRUM_RIM; break;
                        default: muted = mini_acid.isClapMuted(); color = COLOR_DRUM_CLAP; break;
                    }
                }

                if (muted) {
                    gfx.drawRect(sx, y, segmentW, h, COLOR_DARK_GRAY);
                    gfx.drawLine(sx, y, sx + segmentW - 1, y + h - 1, COLOR_DARK_GRAY);
                    gfx.drawLine(sx + segmentW - 1, y, sx, y + h - 1, COLOR_DARK_GRAY);
                } else {
                    if (active) {
                        gfx.fillRect(sx, y, segmentW, h, color);
                    } else {
                        gfx.drawRect(sx, y, segmentW, h, COLOR_DARK_GRAY);
                    }
                    char idxStr[2];
                    snprintf(idxStr, sizeof(idxStr), "%d", (i + 1) % 10);
                    gfx.setTextColor(active ? COLOR_BLACK : COLOR_GRAY);
                    gfx.drawText(sx + (segmentW - 5) / 2, y + 2, idxStr);
                }
            }
            return;
        }

        // RETRO MODE: Airy LED Bar v3 (Cyan / Orange / Magenta palette)
        const int barH = 11;
        const int barW = 160; 
        const int x = (gfx.width() - barW) / 2;
        const int y = Layout::FOOTER.y - barH - 2;
        
        // Semi-transparent look plate
        gfx.fillRect(x - 4, y, barW + 8, barH, IGfxColor(0x0841)); // BG_INSET
        gfx.drawRect(x - 4, y, barW + 8, barH, IGfxColor(0x1082)); // Subtle border

        const int numTracks = 10;
        const int trackStep = barW / numTracks;

        for (int i = 0; i < numTracks; ++i) {
            int ledCX = x + i * trackStep + trackStep / 2;
            int ledCY = y + barH / 2;

            bool muted = false;
            bool active = mini_acid.isTrackActive(i);
            uint16_t color = 0x7BEF;

            if (i == 0) {
                muted = mini_acid.is303Muted(0);
                color = 0x07FF; // Cyan (Acid 1)
            } else if (i == 1) {
                muted = mini_acid.is303Muted(1);
                color = 0xFD20; // Orange (Acid 2)
            } else {
                int drumIdx = i - 2;
                switch (drumIdx) {
                    case 0: muted = mini_acid.isKickMuted(); color = 0xF81F; break; // Magenta (Kick)
                    case 1: muted = mini_acid.isSnareMuted(); color = 0x07FF; break; // Cyan (Snare)
                    case 2: muted = mini_acid.isHatMuted(); color = 0xFD20; break; // Orange (Hats)
                    case 3: muted = mini_acid.isOpenHatMuted(); color = 0xFD20; break; 
                    default: muted = (i < 10) ? mini_acid.isTrackActive(i) == false : false; // Placeholder for safety
                             color = (i % 2 == 0) ? 0xF81F : 0x07FF; break;
                }
                // Specific overrides for missing mutes
                if (drumIdx == 4) muted = mini_acid.isMidTomMuted();
                if (drumIdx == 5) muted = mini_acid.isHighTomMuted();
                if (drumIdx == 6) muted = mini_acid.isRimMuted();
                if (drumIdx == 7) muted = mini_acid.isClapMuted();
            }

            if (muted) {
                 gfx.drawCircle(ledCX, ledCY, 1, IGfxColor(0xF800)); // Distinct Red ring for muted
            } else {
                if (active) {
                    gfx.fillCircle(ledCX, ledCY, 2, IGfxColor(color));
                    gfx.drawPixel(ledCX, ledCY, IGfxColor(0xFFFF)); // Reflection
                } else {
                    gfx.fillCircle(ledCX, ledCY, 1, IGfxColor(0x18E3)); // Dim hole
                }
            }
        }
    }

}
