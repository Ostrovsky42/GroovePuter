#include "sequencer_hub_page.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "../retro_ui_theme.h"
#include "../retro_widgets.h"
#include "../amber_ui_theme.h"
#include "../amber_widgets.h"
#include "../ui_clipboard.h"
#include "../../debug_log.h"
#include "../key_normalize.h"

namespace {
constexpr int kHubTrackCount = 10;    // 303A, 303B, D1..D8
constexpr int kHubVisibleTracks = 6;  // 303A, 303B, D1..D4 on one screen

const char* kDrumLaneShort[8] = {"BD", "SD", "CH", "OH", "MT", "HT", "RM", "CP"};

inline void buildHubTrackLabel(int trackIdx, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    int keyNum = (trackIdx + 1) % 10;
    if (trackIdx == 0) {
        std::snprintf(out, outSize, "%d|A", keyNum);
    } else if (trackIdx == 1) {
        std::snprintf(out, outSize, "%d|B", keyNum);
    } else {
        int drumVoice = trackIdx - 2;    // 0..7
        if (drumVoice < 0) drumVoice = 0;
        if (drumVoice > 7) drumVoice = 7;
        std::snprintf(out, outSize, "%d|%s", keyNum, kDrumLaneShort[drumVoice]);
    }
}

inline bool hubTrackHitAt(MiniAcid& mini_acid, int trackIdx, int step) {
    if (trackIdx < 2) return mini_acid.pattern303Steps(trackIdx)[step] >= 0;
    int voice = trackIdx - 2;
    switch (voice) {
        case 0: return mini_acid.patternKickSteps()[step] > 0;
        case 1: return mini_acid.patternSnareSteps()[step] > 0;
        case 2: return mini_acid.patternHatSteps()[step] > 0;
        case 3: return mini_acid.patternOpenHatSteps()[step] > 0;
        case 4: return mini_acid.patternMidTomSteps()[step] > 0;
        case 5: return mini_acid.patternHighTomSteps()[step] > 0;
        case 6: return mini_acid.patternRimSteps()[step];
        case 7: return mini_acid.patternClapSteps()[step];
        default: return false;
    }
}

inline void drawHubScrollbar(IGfx& gfx,
                             int x,
                             int y,
                             int h,
                             int total,
                             int visible,
                             int first,
                             IGfxColor trackColor,
                             IGfxColor thumbColor) {
    if (total <= visible || visible <= 0 || h <= 4) return;
    gfx.drawRect(x, y, 3, h, trackColor);
    int thumbH = (h * visible) / total;
    if (thumbH < 5) thumbH = 5;
    int travel = h - thumbH - 2;
    if (travel < 0) travel = 0;
    int maxFirst = total - visible;
    int thumbY = y + 1;
    if (maxFirst > 0) {
        thumbY += (travel * first) / maxFirst;
    }
    gfx.fillRect(x + 1, thumbY, 1, thumbH, thumbColor);
}
} // namespace

SequencerHubPage::SequencerHubPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
    (void)gfx;

    DrumSequencerGridComponent::Callbacks cb;
    cb.onToggle = [this](int voice, int step) {
        withAudioGuard([&]() { mini_acid_.toggleDrumStep(voice, step); });
    };
    cb.onToggleAccent = [this](int step) {
        withAudioGuard([&]() { mini_acid_.toggleDrumAccentStep(step); });
    };
    cb.cursorStep = [this]() { return stepCursor_; };
    cb.cursorVoice = [this]() { return voiceCursor_; };
    cb.gridFocused = [this]() { 
        return mode_ == Mode::DETAIL && isDrumTrack(selectedTrack_) && focus_ == FocusLane::GRID; 
    };
    cb.currentStep = [this]() { return mini_acid_.currentStep(); };

    drumGrid_ = std::make_unique<DrumSequencerGridComponent>(mini_acid_, cb);
    syncOverviewScroll();
}

void SequencerHubPage::draw(IGfx& gfx) {
    switch (hub_style_) {
        case VisualStyle::RETRO_CLASSIC:
            drawRetroClassicStyle(gfx);
            break;
        case VisualStyle::AMBER:
            drawAmberStyle(gfx);
            break;
        case VisualStyle::MINIMAL_DARK:
            drawTEGridStyle(gfx);
            break;
        case VisualStyle::MINIMAL:
        default:
            drawMinimalStyle(gfx);
            break;
    }
}

void SequencerHubPage::drawMinimalStyle(IGfx& gfx) {
    if (mode_ == Mode::OVERVIEW) {
        drawOverview(gfx);
    } else {
        drawDetail(gfx);
    }
}

void SequencerHubPage::drawTEGridStyle(IGfx& gfx) {
    const Rect& bounds = getBoundaries();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.w;
    int h = bounds.h;

    // TE color palette: strict monochrome
    const IGfxColor TE_BLACK = IGfxColor::Black();
    const IGfxColor TE_WHITE = IGfxColor::White();
    const IGfxColor TE_GRID = IGfxColor(0x404040);
    const IGfxColor TE_ACCENT = IGfxColor(0xC0C0C0);
    const IGfxColor TE_DIM = IGfxColor(0x808080);
    const IGfxColor TE_ACTIVE = IGfxColor(0xFFFFFF);

    // Clear background
    gfx.fillRect(x, y, w, h, TE_BLACK);

    // === HEADER BAR (TE brutalist style) ===
    int header_h = 11;
    gfx.fillRect(x, y, w, header_h, TE_WHITE);
    gfx.setTextColor(TE_BLACK);

    // Title
    const char* modeText = (mode_ == Mode::OVERVIEW) ? "SEQ" : "EDIT";
    char titleBuf[32];
    if (mode_ == Mode::OVERVIEW) {
        snprintf(titleBuf, sizeof(titleBuf), "SEQ OVERVIEW");
    } else {
        const char* trackName = "?";
        if (selectedTrack_ == 0) trackName = "A";
        else if (selectedTrack_ == 1) trackName = "B";
        else {
            static char drumName[8];
            snprintf(drumName, sizeof(drumName), "D%d", selectedTrack_ - 1);
            trackName = drumName;
        }
        snprintf(titleBuf, sizeof(titleBuf), "SEQ %s", trackName);
    }
    gfx.drawText(x + 2, y + 2, titleBuf);

    // Status
    char statusBuf[32];
    bool playing = mini_acid_.isPlaying();
    int bpm = (int)mini_acid_.bpm();
    snprintf(statusBuf, sizeof(statusBuf), "%s %03d", playing ? ">" : "||", bpm);
    int statusW = textWidth(gfx, statusBuf);
    gfx.drawText(x + w - statusW - 2, y + 2, statusBuf);

    // === MAIN CONTENT ===
    int content_y = y + header_h + 1;
    int content_h = h - header_h - 12; // Reserve footer

    if (mode_ == Mode::OVERVIEW) {
        syncOverviewScroll();
        // === OVERVIEW MODE: Track grid ===
        const int row_h = 14;
        const int track_count = kHubTrackCount;
        const int visible_tracks = kHubVisibleTracks;
        const int first_track = overviewScroll_;

        // Draw grid lines first
        for (int i = 0; i <= visible_tracks; i++) {
            int ly = content_y + i * row_h;
            gfx.drawLine(x, ly, x + w - 1, ly, TE_GRID);
        }

        // Track rows
        for (int row = 0; row < visible_tracks; row++) {
            const int track_idx = first_track + row;
            if (track_idx >= track_count) break;
            int ry = content_y + row * row_h;
            bool selected = (track_idx == selectedTrack_);

            // Selection highlight
            if (selected) {
                gfx.fillRect(x + 1, ry + 1, w - 2, row_h - 1, TE_ACCENT);
            }

            // Track label
            char label[12];
            buildHubTrackLabel(track_idx, label, sizeof(label));

            gfx.setTextColor(selected ? TE_BLACK : TE_WHITE);
            gfx.drawText(x + 4, ry + 2, label);

            // LED activity indicator
            bool active = mini_acid_.isTrackActive(track_idx);
            int led_x = x + 30;
            int led_y = ry + row_h / 2 - 2;
            if (active && playing) {
                gfx.fillRect(led_x, led_y, 4, 4, selected ? TE_BLACK : TE_ACTIVE);
            }
            gfx.drawRect(led_x, led_y, 4, 4, selected ? TE_BLACK : TE_GRID);

            // Step grid (16 cells) - enlarged for readability
            int grid_x = x + 42;
            int cell_w = 12;
            int cell_h = row_h - 4;
            int currentStep = mini_acid_.currentStep();

            for (int s = 0; s < 16; s++) {
                int cx = grid_x + s * cell_w;

                bool hit = hubTrackHitAt(mini_acid_, track_idx, s);

                // Cell background
                IGfxColor cellBg = TE_BLACK;
                if (s == currentStep && playing) cellBg = TE_GRID;
                if (hit) cellBg = selected ? TE_BLACK : TE_WHITE;
                if (selected && s == stepCursor_) cellBg = TE_BLACK;

                gfx.fillRect(cx, ry + 2, cell_w - 1, cell_h, cellBg);

                // Cell border
                IGfxColor borderColor = TE_GRID;
                if (s % 4 == 0) borderColor = TE_ACCENT; // Emphasize beats
                if (selected && s == stepCursor_) borderColor = TE_BLACK; // Cursor

                gfx.drawRect(cx, ry + 2, cell_w - 1, cell_h, borderColor);
            }
        }
        drawHubScrollbar(gfx,
                         x + w - 3,
                         content_y,
                         visible_tracks * row_h,
                         track_count,
                         visible_tracks,
                         first_track,
                         TE_GRID,
                         TE_ACTIVE);

    } else {
        // === DETAIL MODE ===
        if (isDrumTrack(selectedTrack_)) {
            // Use drum grid component
            drumGrid_->setBoundaries(Rect(x + 2, content_y, w - 4, content_h - 2));
            drumGrid_->draw(gfx);
        } else {
            // 303 detail view (compact TE style)
            int cell_w = 14;
            int cell_h = content_h - 20;
            int grid_x = x + (w - cell_w * 16) / 2;
            int grid_y = content_y + 10;

            const int8_t* notes = mini_acid_.pattern303Steps(selectedTrack_);
            const bool* accents = mini_acid_.pattern303AccentSteps(selectedTrack_);
            const bool* slides = mini_acid_.pattern303SlideSteps(selectedTrack_);
            int playingStep = mini_acid_.currentStep();

            for (int s = 0; s < 16; s++) {
                int cx = grid_x + s * cell_w;
                bool isCursor = (s == stepCursor_);
                bool isPlay = (s == playingStep && playing);

                IGfxColor bgColor = (s % 4 == 0) ? TE_GRID : TE_BLACK;
                if (isCursor) bgColor = TE_ACCENT;
                if (isPlay) bgColor = TE_WHITE;

                gfx.fillRect(cx, grid_y, cell_w - 1, cell_h, bgColor);
                gfx.drawRect(cx, grid_y, cell_w - 1, cell_h, TE_GRID);

                if (notes[s] >= 0) {
                    // Draw note
                    char n[8];
                    formatNoteName(notes[s], n, sizeof(n));
                    IGfxColor textColor = isCursor || isPlay ? TE_BLACK : TE_WHITE;
                    gfx.setTextColor(textColor);
                    int text_w = textWidth(gfx, n);
                    gfx.drawText(cx + (cell_w - text_w) / 2, grid_y + 5, n);

                    // Indicators
                    if (accents[s]) {
                        gfx.fillRect(cx + 2, grid_y + cell_h - 6, 3, 3, isCursor || isPlay ? TE_BLACK : TE_WHITE);
                    }
                    if (slides[s]) {
                        gfx.fillRect(cx + cell_w - 5, grid_y + cell_h - 6, 3, 3, isCursor || isPlay ? TE_BLACK : TE_WHITE);
                    }
                }
            }
        }
    }

    // === FOOTER BAR ===
    int footer_y = y + h - 11;
    gfx.drawLine(x, footer_y - 1, x + w - 1, footer_y - 1, TE_GRID);
    gfx.setTextColor(TE_DIM);

    const char* footer_text = (mode_ == Mode::OVERVIEW)
        ? "UP/DN:TRK L/R:STEP X:HIT A:ACC"
        : "ESC  A/Z:NOTE S/X:OCT";
    gfx.drawText(x + 2, footer_y + 2, footer_text);

    // Play indicator
    if (playing) {
        gfx.setTextColor(TE_WHITE);
        gfx.drawText(x + w - 10, footer_y + 2, ">");
    }
}

void SequencerHubPage::drawRetroClassicStyle(IGfx& gfx) {
#ifdef USE_RETRO_THEME
    const Rect& bounds = getBoundaries();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.w;
    int h = bounds.h;

    int playingStep = mini_acid_.currentStep();
    bool isPlaying = mini_acid_.isPlaying();
    int bpm = (int)(mini_acid_.bpm() + 0.5f);

    // 1. Header
    char subTitle[32];
    if (mode_ == Mode::OVERVIEW) {
        snprintf(subTitle, sizeof(subTitle), "OVERVIEW");
    } else {
        snprintf(subTitle, sizeof(subTitle), "SEQ:%s", 
            selectedTrack_ == 0 ? "303A" : (selectedTrack_ == 1 ? "303B" : "DRUM"));
    }
    
    drawHeaderBar(gfx, x, y, w, 14, "SEQ HUB", subTitle, isPlaying, bpm, playingStep);

    // 2. Content Area
    int contentY = y + 15;
    int contentH = h - 15 - 12;
    gfx.fillRect(x, contentY, w, contentH, IGfxColor(BG_DEEP_BLACK));

    if (mode_ == Mode::OVERVIEW) {
        syncOverviewScroll();
        // Simple List of 10 Tracks (Retro Style)
            int rowH = 13;
            int spacing = 1;
            int firstTrack = overviewScroll_;
            for (int row = 0; row < kHubVisibleTracks; row++) {
                int i = firstTrack + row;
                if (i >= kHubTrackCount) break;
                int ry = contentY + row * (rowH + spacing);
                if (ry + rowH > contentY + contentH) break;
                bool selected = (i == selectedTrack_);
            
            // Per-track colors based on instrument
            uint32_t trackColor = NEON_ORANGE; // Default for Drums
            if (i == 0) trackColor = NEON_CYAN;
            else if (i == 1) trackColor = NEON_MAGENTA;
            
            if (selected) {
                 gfx.fillRect(x + 2, ry, w - 4, rowH, IGfxColor(BG_PANEL));
                 drawGlowBorder(gfx, x + 2, ry, w - 4, rowH, IGfxColor(trackColor), 1);
            }

            // Name with Glow if selected
            char name[16];
            buildHubTrackLabel(i, name, sizeof(name));
            
            if (selected) {
                drawGlowText(gfx, x + 6, ry + 1, name, IGfxColor(FOCUS_GLOW), IGfxColor(TEXT_PRIMARY));
            } else {
                gfx.setTextColor(IGfxColor(TEXT_SECONDARY));
                gfx.drawText(x + 6, ry + 1, name);
            }

            // Tiny mask
            int maskX = x + 50;
            int cellW = 11;
            for (int s = 0; s < 16; s++) {
                bool hit = hubTrackHitAt(mini_acid_, i, s);
                
                IGfxColor color = hit ? (selected ? IGfxColor(trackColor) : IGfxColor(GRID_MEDIUM)) : IGfxColor(BG_INSET);
                if (s == playingStep && isPlaying) color = IGfxColor(NEON_YELLOW);
                gfx.fillRect(maskX + s * cellW, ry + 2, cellW - 1, rowH - 4, color);
                IGfxColor border = (s % 4 == 0) ? IGfxColor(GRID_MEDIUM) : IGfxColor(GRID_DIM);
                gfx.drawRect(maskX + s * cellW, ry + 2, cellW - 1, rowH - 4, border);
            }
            
            if (selected) {
                 drawOverviewCursor(gfx, i, stepCursor_, maskX, ry + 2, cellW, rowH - 4);
            }

            // Activity LED (Retro Hardware style)
            bool active = mini_acid_.isTrackActive(i);
            RetroWidgets::drawLED(gfx, x + 42, ry + (rowH/2), 2, active && isPlaying, IGfxColor(trackColor));
        }
            drawHubScrollbar(gfx,
                             x + w - 4,
                             contentY + 1,
                             kHubVisibleTracks * (rowH + spacing) - spacing,
                             kHubTrackCount,
                             kHubVisibleTracks,
                             firstTrack,
                             IGfxColor(GRID_DIM),
                             IGfxColor(SELECT_BRIGHT));

        // Removed redundant channel activity bar - LED indicators already show activity

        // Scanlines disabled: caused flicker on small TFT
        RetroWidgets::drawFooterBar(gfx, x, y + h - 12, w, 12, "[UP/DN]TRK [L/R]STEP [X]HIT [A]ACC", "ENT:Open  Q-I:Pat", "HUB");
    } else {
        // DETAIL MODE
        if (isDrumTrack(selectedTrack_)) {
            drumGrid_->setStyle(GrooveboxStyle::RETRO_CLASSIC);
            drumGrid_->setBoundaries(Rect(0, contentY + 2, 240, contentH - 4));
            drumGrid_->draw(gfx);
            drawFooterBar(gfx, x, y + h - 12, w, 12, "[ARROWS]Grid [A]Accent", "ESC", "DRUM");
        } else {
            // Enhanced 303 Detail (Retro Style with Teal & Orange)
            int cellW = (w - 20) / 16;
            int cellH = 40;
            int gridX = (w - cellW * 16) / 2;
            int gridY = contentY + (contentH - cellH) / 2;

            const int8_t* notes = mini_acid_.pattern303Steps(selectedTrack_);
            const bool* accents = mini_acid_.pattern303AccentSteps(selectedTrack_);
            const bool* slides = mini_acid_.pattern303SlideSteps(selectedTrack_);

            for (int s = 0; s < 16; s++) {
                int cx = gridX + s * cellW;
                bool isCursor = (s == stepCursor_ && focus_ == FocusLane::GRID);
                bool isPlay = (s == playingStep && isPlaying);

                IGfxColor bgColor = (s % 4 == 0) ? IGfxColor(BG_INSET) : IGfxColor(BG_PANEL);
                gfx.fillRect(cx, gridY, cellW - 1, cellH, bgColor);
                gfx.drawRect(cx, gridY, cellW - 1, cellH, IGfxColor(GRID_MEDIUM));

                if (isCursor) drawGlowBorder(gfx, cx, gridY, cellW - 1, cellH, IGfxColor(SELECT_BRIGHT), 1);
                if (isPlay) drawGlowBorder(gfx, cx, gridY, cellW - 1, cellH, IGfxColor(STATUS_PLAYING), 2);

                if (notes[s] >= 0) {
                    char n[8]; formatNoteName(notes[s], n, sizeof(n));
                    IGfxColor noteColor = accents[s] ? IGfxColor(NEON_ORANGE) : IGfxColor(NEON_CYAN);
                    gfx.setTextColor(noteColor);
                    gfx.drawText(cx + (cellW - textWidth(gfx, n)) / 2, gridY + 10, n);
                    
                    if (slides[s]) drawLED(gfx, cx + cellW / 2 - 1, gridY + cellH - 8, 1, true, IGfxColor(NEON_MAGENTA));
                    if (accents[s]) drawLED(gfx, cx + cellW - 5, gridY + cellH - 8, 1, true, IGfxColor(NEON_ORANGE));
                } else {
                    gfx.setTextColor(IGfxColor(TEXT_DIM));
                    gfx.drawText(cx + (cellW - 4) / 2, gridY + 10, ".");
                }
            }
            // Scanlines disabled: caused flicker on small TFT
            RetroWidgets::drawFooterBar(gfx, x, y + h - 12, w, 12, "[A/Z]±nt [S/X]±oct [Alt+S]Sld [Alt+A]Acc", "ESC", "303");
        }
    }
#else
    drawMinimalStyle(gfx);
#endif
}

void SequencerHubPage::drawAmberStyle(IGfx& gfx) {
#ifdef USE_AMBER_THEME
    const Rect& bounds = getBoundaries();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.w;
    int h = bounds.h;

    int playingStep = mini_acid_.currentStep();
    bool isPlaying = mini_acid_.isPlaying();
    int bpm = (int)(mini_acid_.bpm() + 0.5f);

    char subTitle[32];
    if (mode_ == Mode::OVERVIEW) {
        snprintf(subTitle, sizeof(subTitle), "OVERVIEW");
    } else {
        snprintf(subTitle, sizeof(subTitle), "SEQ:%s", 
            selectedTrack_ == 0 ? "303A" : (selectedTrack_ == 1 ? "303B" : "DRUM"));
    }
    
    AmberWidgets::drawHeaderBar(gfx, x, y, w, 14, "SEQ HUB", subTitle, isPlaying, bpm, playingStep);

    int contentY = y + 15;
    int contentH = h - 15 - 12;
    gfx.fillRect(x, contentY, w, contentH, IGfxColor(AmberTheme::BG_DEEP_BLACK));

    if (mode_ == Mode::OVERVIEW) {
        syncOverviewScroll();
        int rowH = 13;
        int spacing = 1;
        int firstTrack = overviewScroll_;
        for (int row = 0; row < kHubVisibleTracks; row++) {
            int i = firstTrack + row;
            if (i >= kHubTrackCount) break;
            int ry = contentY + row * (rowH + spacing);
            if (ry + rowH > contentY + contentH) break;
            bool selected = (i == selectedTrack_);
            
            // Amber is more monochromatic but can use shades
            uint32_t AmberTrackColor = AmberTheme::NEON_CYAN;
            if (i == 1) AmberTrackColor = AmberTheme::NEON_MAGENTA;
            else if (i >= 2) AmberTrackColor = AmberTheme::NEON_ORANGE;

            if (selected) {
                 gfx.fillRect(x + 2, ry, w - 4, rowH, IGfxColor(AmberTheme::BG_PANEL));
                 AmberWidgets::drawGlowBorder(gfx, x + 2, ry, w - 4, rowH, IGfxColor(AmberTheme::NEON_CYAN), 1);
            }

            char name[16];
            buildHubTrackLabel(i, name, sizeof(name));
            
            if (selected) {
                AmberWidgets::drawGlowText(gfx, x + 6, ry + 1, name, IGfxColor(AmberTheme::FOCUS_GLOW), IGfxColor(AmberTheme::TEXT_PRIMARY));
            } else {
                gfx.setTextColor(IGfxColor(AmberTheme::TEXT_SECONDARY));
                gfx.drawText(x + 6, ry + 1, name);
            }

            int maskX = x + 50;
            int cellW = 11;
            for (int s = 0; s < 16; s++) {
                bool hit = hubTrackHitAt(mini_acid_, i, s);
                
                IGfxColor color = hit ? (selected ? IGfxColor(AmberTheme::NEON_CYAN) : IGfxColor(AmberTheme::GRID_MEDIUM)) : IGfxColor(AmberTheme::BG_INSET);
                if (s == playingStep && isPlaying) color = IGfxColor(AmberTheme::NEON_YELLOW);
                gfx.fillRect(maskX + s * cellW, ry + 2, cellW - 1, rowH - 4, color);
                IGfxColor border = (s % 4 == 0) ? IGfxColor(AmberTheme::GRID_MEDIUM) : IGfxColor(AmberTheme::GRID_DIM);
                gfx.drawRect(maskX + s * cellW, ry + 2, cellW - 1, rowH - 4, border);
            }
            
            if (selected) {
                 drawOverviewCursor(gfx, i, stepCursor_, maskX, ry + 2, cellW, rowH - 4);
            }

            // Activity LED (Amber Hardware style)
            bool active = mini_acid_.isTrackActive(i);
            AmberWidgets::drawLED(gfx, x + 42, ry + (rowH/2), 2, active && isPlaying, IGfxColor(AmberTrackColor));
        }
        drawHubScrollbar(gfx,
                         x + w - 4,
                         contentY + 1,
                         kHubVisibleTracks * (rowH + spacing) - spacing,
                         kHubTrackCount,
                         kHubVisibleTracks,
                         firstTrack,
                         IGfxColor(AmberTheme::GRID_DIM),
                         IGfxColor(AmberTheme::SELECT_BRIGHT));

        // Removed redundant channel activity bar - LED indicators already show activity

        // Scanlines disabled: caused flicker on small TFT
        AmberWidgets::drawFooterBar(gfx, x, y + h - 12, w, 12, "[UP/DN]TRK [L/R]STEP [X]HIT [A]ACC", "ENT:Open  Q-I:Pat", "HUB");
    } else {
        if (isDrumTrack(selectedTrack_)) {
            drumGrid_->setStyle(GrooveboxStyle::AMBER);
            drumGrid_->setBoundaries(Rect(0, contentY + 2, 240, contentH - 4));
            drumGrid_->draw(gfx);
            AmberWidgets::drawFooterBar(gfx, x, y + h - 12, w, 12, "[ARROWS]Grid [A]Accent", "ESC:Back", "DRUM");
        } else {
            int cellW = (w - 20) / 16;
            int cellH = 40;
            int gridX = (w - cellW * 16) / 2;
            int gridY = contentY + (contentH - cellH) / 2;

            const int8_t* notes = mini_acid_.pattern303Steps(selectedTrack_);
            const bool* accents = mini_acid_.pattern303AccentSteps(selectedTrack_);
            const bool* slides = mini_acid_.pattern303SlideSteps(selectedTrack_);

            for (int s = 0; s < 16; s++) {
                int cx = gridX + s * cellW;
                bool isCursor = (s == stepCursor_ && focus_ == FocusLane::GRID);
                bool isPlay = (s == playingStep && isPlaying);

                IGfxColor bgColor = (s % 4 == 0) ? IGfxColor(AmberTheme::BG_INSET) : IGfxColor(AmberTheme::BG_PANEL);
                gfx.fillRect(cx, gridY, cellW - 1, cellH, bgColor);
                gfx.drawRect(cx, gridY, cellW - 1, cellH, IGfxColor(AmberTheme::GRID_MEDIUM));

                if (isCursor) AmberWidgets::drawGlowBorder(gfx, cx, gridY, cellW - 1, cellH, IGfxColor(AmberTheme::SELECT_BRIGHT), 1);
                if (isPlay) AmberWidgets::drawGlowBorder(gfx, cx, gridY, cellW - 1, cellH, IGfxColor(AmberTheme::STATUS_PLAYING), 2);

                if (notes[s] >= 0) {
                    char n[8]; formatNoteName(notes[s], n, sizeof(n));
                    IGfxColor noteColor = accents[s] ? IGfxColor(AmberTheme::NEON_ORANGE) : IGfxColor(AmberTheme::NEON_CYAN);
                    gfx.setTextColor(noteColor);
                    gfx.drawText(cx + (cellW - textWidth(gfx, n)) / 2, gridY + 10, n);
                    
                    if (slides[s]) AmberWidgets::drawLED(gfx, cx + cellW / 2 - 1, gridY + cellH - 8, 1, true, IGfxColor(AmberTheme::NEON_MAGENTA));
                    if (accents[s]) AmberWidgets::drawLED(gfx, cx + cellW - 5, gridY + cellH - 8, 1, true, IGfxColor(AmberTheme::NEON_ORANGE));
                } else {
                    gfx.setTextColor(IGfxColor(AmberTheme::TEXT_DIM));
                    gfx.drawText(cx + (cellW - 4) / 2, gridY + 10, ".");
                }
            }
            AmberWidgets::drawFooterBar(gfx, x, y + h - 12, w, 12, "[A/Z]±nt [S/X]±oct [Alt+S]Sld [Alt+A]Acc", "ESC:Back", "303");
        }
    }
#else
    drawMinimalStyle(gfx);
#endif
}

void SequencerHubPage::drawOverview(IGfx& gfx) {
    // Enhanced header with Swing % and Bank
    char headerBuf[64];
    int swingPct = (int)(mini_acid_.swing() * 100.0f + 0.5f);
    char bankChar = 'A' + (mini_acid_.currentScene() / 16);
    snprintf(headerBuf, sizeof(headerBuf), "SEQUENCER [BANK:%c SW:%d%%]", bankChar, swingPct);
    
    UI::drawStandardHeader(gfx, mini_acid_, headerBuf);
    UI::drawFeelHeaderHud(gfx, mini_acid_, 166, 9);
    LayoutManager::clearContent(gfx);

    const int startY = LayoutManager::lineY(0);
    const int rowH = 13;
    syncOverviewScroll();

    for (int row = 0; row < kHubVisibleTracks; row++) {
        int trackIdx = overviewScroll_ + row;
        if (trackIdx >= kHubTrackCount) break;
        drawTrackRow(gfx, trackIdx, startY + row * (rowH + 1), rowH, trackIdx == selectedTrack_);
    }
    drawHubScrollbar(gfx,
                     236,
                     startY,
                     kHubVisibleTracks * (rowH + 1) - 1,
                     kHubTrackCount,
                     kHubVisibleTracks,
                     overviewScroll_,
                     COLOR_GRAY_DARKER,
                     COLOR_ACCENT);

    // Removed redundant channel activity bar - LED indicators already show activity

    UI::drawStandardFooter(gfx,
        "[UP/DN]TRK [L/R]STEP [X]HIT [A]ACC",
        "[ENT]OPEN [Q-I]PAT [SPACE]PLAY");
}

void SequencerHubPage::drawTrackRow(IGfx& gfx, int trackIdx, int y, int h, bool selected) {
    const int ledX = 50;
    const int maskX = 60;
    const int cellW = 11;
    
    // Background highlight
    if (selected) {
        gfx.fillRect(2, y, 236, h, IGfxColor(0x282850));
    }
    
    // Name
    char name[16];
    buildHubTrackLabel(trackIdx, name, sizeof(name));
    
    gfx.setTextColor(selected ? COLOR_WHITE : COLOR_GRAY);
    gfx.drawText(4, y + 1, name);
    
    // Activity LED
    bool active = mini_acid_.isTrackActive(trackIdx);
    gfx.fillRect(ledX, y + 2, 6, 6, active ? COLOR_ACCENT : COLOR_BLACK);
    gfx.drawRect(ledX, y + 2, 6, 6, COLOR_GRAY);
    
    // Mini step mask
    bool isSynth = !isDrumTrack(trackIdx);
    int currentStep = mini_acid_.currentStep();
    
    for (int s = 0; s < SEQ_STEPS; s++) {
        bool hit = hubTrackHitAt(mini_acid_, trackIdx, s);
        
        IGfxColor baseColor = isSynth ? (trackIdx == 0 ? COLOR_SYNTH_A : COLOR_SYNTH_B) : COLOR_TEXT;
        IGfxColor color = hit ? (selected ? baseColor : COLOR_GRAY) : COLOR_DARKER;
        if (s == currentStep && mini_acid_.isPlaying()) color = COLOR_WARN;
        
        gfx.fillRect(maskX + s * cellW, y + 2, cellW - 1, h - 4, color);
        IGfxColor border = (s % 4 == 0) ? COLOR_ACCENT : COLOR_GRAY_DARKER;
        gfx.drawRect(maskX + s * cellW, y + 2, cellW - 1, h - 4, border);
    }

    if (selected) {
        int cx = maskX + stepCursor_ * cellW;
        gfx.drawRect(cx, y + 2, cellW - 1, h - 4, COLOR_STEP_SELECTED);
    }
}

void SequencerHubPage::drawOverviewCursor(IGfx& gfx, int trackIdx, int stepIdx, int x, int y, int cellW, int cellH) {
     int cx = x + stepIdx * cellW;
     // Draw a prominent cursor around the step
     if (UI::currentStyle == ::VisualStyle::AMBER) {
         AmberWidgets::drawGlowBorder(gfx, cx, y, cellW - 1, cellH, IGfxColor(AmberTheme::SELECT_BRIGHT), 2);
     } else if (UI::currentStyle == ::VisualStyle::RETRO_CLASSIC) {
         RetroWidgets::drawGlowBorder(gfx, cx, y, cellW - 1, cellH, IGfxColor(RetroTheme::SELECT_BRIGHT), 2);
     } else {
         gfx.drawRect(cx, y, cellW - 1, cellH, COLOR_STEP_SELECTED);
     }
}

void SequencerHubPage::drawDetail(IGfx& gfx) {
    char title[32];
    std::snprintf(title, sizeof(title), "SEQ DETAIL: %s", 
        selectedTrack_ == 0 ? "303 A" : (selectedTrack_ == 1 ? "303 B" : "DRUMS"));
    
    UI::drawStandardHeader(gfx, mini_acid_, title);
    UI::drawFeelHeaderHud(gfx, mini_acid_, 166, 9);
    LayoutManager::clearContent(gfx);

    if (isDrumTrack(selectedTrack_)) {
        // Reuse DrumSequencerGridComponent for drum tracks
        int contentY = LayoutManager::lineY(0);
        drumGrid_->setBoundaries(Rect(0, contentY, 240, 100));
        drumGrid_->draw(gfx);
    } else {
        // Custom detail for synthesis tracks
        const int gridY = LayoutManager::lineY(1);
        const int cellW = 14;
        const int gridX = (240 - cellW * SEQ_STEPS) / 2;
        
        const int8_t* steps = mini_acid_.pattern303Steps(selectedTrack_);
        const bool* accents = mini_acid_.pattern303AccentSteps(selectedTrack_);
        const bool* slides = mini_acid_.pattern303SlideSteps(selectedTrack_);
        int playingStep = mini_acid_.currentStep();
        
        for (int s = 0; s < SEQ_STEPS; s++) {
            int x = gridX + s * cellW;
            bool isCurrent = (s == playingStep && mini_acid_.isPlaying());
            bool isCursor = (s == stepCursor_ && focus_ == FocusLane::GRID);
            
            // Step background
            IGfxColor bgColor = isCurrent ? IGfxColor(0x303000) : (isCursor ? IGfxColor(0x3C3C64) : COLOR_BLACK);
            gfx.fillRect(x, gridY, cellW - 1, 40, bgColor);
            gfx.drawRect(x, gridY, cellW - 1, 40, COLOR_GRAY);
            
            // Note indicator
            if (steps[s] >= 0) {
                IGfxColor noteColor = selectedTrack_ == 0 ? COLOR_SYNTH_A : COLOR_SYNTH_B;
                gfx.fillRect(x + 2, gridY + 5, cellW - 5, 10, noteColor);
                // Mini note value (simplified)
                char n[4]; std::snprintf(n, sizeof(n), "%d", steps[s] % 12);
                gfx.setTextColor(COLOR_BLACK);
                gfx.drawText(x + 3, gridY + 6, n);
            }
            
            // Accent/Slide
            if (accents[s]) gfx.fillRect(x + 2, gridY + 20, 4, 4, COLOR_ACCENT);
            if (slides[s]) gfx.fillRect(x + 8, gridY + 20, 4, 4, IGfxColor::Cyan());
        }
    }

    // Contextual Footer
    const char* left = "[ESC]  [SPACE] PLAY";
    const char* right = isDrumTrack(selectedTrack_) ? "[A] ACCENT" : "[A] ACC  [S] SLIDE";
    UI::drawStandardFooter(gfx, left, right);
}

bool SequencerHubPage::handleEvent(UIEvent& e) {
    if (e.event_type == GROOVEPUTER_MOUSE_DOWN) {
        if (mode_ == Mode::DETAIL && isDrumTrack(selectedTrack_)) {
            return drumGrid_->handleEvent(e);
        }
        return false;
    }

    if (e.event_type != GROOVEPUTER_KEY_DOWN) return false;

    // LOCAL NAV FIRST: Ensure Esc/Back work within the hub to exit Detail mode
    if (mode_ == Mode::DETAIL && UIInput::isBack(e)) {
        mode_ = Mode::OVERVIEW;
        return true;
    }

    // Fast return for global nav (others like help, voice toggle)
    if (UIInput::isGlobalNav(e)) return false;

    if (handleModeSwitch(e)) return true;
    if (handleQuickKeys(e)) return true;
    if (handleVolumeInput(e)) return true; // Check volume before navigation
    if (handleNavigation(e)) return true;
    if (handleGridEdit(e)) return true;

    return false;
}

bool SequencerHubPage::handleModeSwitch(UIEvent& e) {
    // ENTER: perform action or enter detail
    if (e.key == '\n' || e.key == '\r') {
        if (mode_ == Mode::OVERVIEW) {
            // "Imba" Feature: Jump to full editor page for ALL tracks
            if (selectedTrack_ == 0) { // 303 A
                requestPageTransition(1, stepCursor_); // Page 1 = Pattern Edit 303A
                return true;
            } else if (selectedTrack_ == 1) { // 303 B
                requestPageTransition(2, stepCursor_); // Page 2 = Pattern Edit 303B
                return true;
            } else {
                // Jump to Drum Sequencer for tracks 2-9
                int voiceIndex = getDrumVoiceIndex(selectedTrack_);
                int context = (voiceIndex << 8) | stepCursor_;
                requestPageTransition(5, context); // Page 5 = Drum Sequencer Page
                return true;
            }
        }
        // In DETAIL (legacy fallback), ENTER does nothing special for now
        return false;
    }
    
    // ESC/BACK: return to overview
    if (e.key == 0x1B || e.key == 0x08) {
        if (mode_ == Mode::DETAIL) {
            mode_ = Mode::OVERVIEW;
            return true;
        }
        // Let MiniAcidDisplay handle global back if in OVERVIEW
        return false;
    }
    
    return false;
}

bool SequencerHubPage::handleQuickKeys(UIEvent& e) {
    char lower = std::tolower(e.key);
    
    // Direct drum step editing from the HUB overview grid.
    if (mode_ == Mode::OVERVIEW && !e.alt && !e.ctrl && !e.meta && isDrumTrack(selectedTrack_)) {
        if (lower == 'x') {
            int voice = getDrumVoiceIndex(selectedTrack_);
            withAudioGuard([&]() { mini_acid_.toggleDrumStep(voice, stepCursor_); });
            return true;
        }
        if (lower == 'a') {
            withAudioGuard([&]() { mini_acid_.toggleDrumAccentStep(stepCursor_); });
            return true;
        }
    }
    
    // Clear (Backspace / Alt+Backspace)
    if (e.key == '\b' || e.key == 0x7F) {
        if (e.alt) {
            // Clear entire pattern for current track
            withAudioGuard([&]() {
                if (isDrumTrack(selectedTrack_)) {
                    int voice = getDrumVoiceIndex(selectedTrack_);
                    for (int i = 0; i < SEQ_STEPS; ++i) {
                        mini_acid_.sceneManager().setDrumStep(voice, i, false, false);
                    }
                } else {
                    for (int i = 0; i < SEQ_STEPS; ++i) {
                        mini_acid_.clear303Step(i, selectedTrack_);
                    }
                }
            });
            UI::showToast("Track Cleared");
            return true;
        } else {
            // Clear current step
            withAudioGuard([&]() {
                if (isDrumTrack(selectedTrack_)) {
                    int voice = getDrumVoiceIndex(selectedTrack_);
                    mini_acid_.sceneManager().setDrumStep(voice, stepCursor_, false, false);
                } else {
                    mini_acid_.clear303Step(stepCursor_, selectedTrack_);
                }
            });
            return true;
        }
    }
    
    // Transport
    if (e.key == ' ') {
        withAudioGuard([&]() {
            if (mini_acid_.isPlaying()) mini_acid_.stop();
            else mini_acid_.start();
        });
        return true;
    }

    // Pattern quick select (Q-I)
    int patIdx = qwertyToPatternIndex(lower);
    
    if (patIdx >= 0) {
        patternCursor_ = patIdx;
        withAudioGuard([&]() {
            if (isDrumTrack(selectedTrack_)) {
                mini_acid_.setDrumPatternIndex(patIdx);
            } else {
                mini_acid_.set303PatternIndex(selectedTrack_, patIdx);
            }
        });
        return true;
    }
    
    // Copy/Paste (Ctrl+C / Ctrl+V)
    if (lower == 'c' && e.ctrl) {
        UIEvent app_evt{};
        app_evt.event_type = GROOVEPUTER_APPLICATION_EVENT;
        app_evt.app_event_type = GROOVEPUTER_APP_EVENT_COPY;
        // Forward as application event
        return handleAppEvent(app_evt);
    }
    if (lower == 'v' && e.ctrl) {
        UIEvent app_evt{};
        app_evt.event_type = GROOVEPUTER_APPLICATION_EVENT;
        app_evt.app_event_type = GROOVEPUTER_APP_EVENT_PASTE;
        // Forward as application event
        return handleAppEvent(app_evt);
    }
    
    return false;
}

bool SequencerHubPage::handleAppEvent(const UIEvent& e) {
    if (e.event_type != GROOVEPUTER_APPLICATION_EVENT) return false;
    
    if (e.app_event_type == GROOVEPUTER_APP_EVENT_COPY) {
        if (isDrumTrack(selectedTrack_)) {
            // Copy Drums
            const bool* hits[NUM_DRUM_VOICES] = {
                mini_acid_.patternKickSteps(), mini_acid_.patternSnareSteps(),
                mini_acid_.patternHatSteps(), mini_acid_.patternOpenHatSteps(),
                mini_acid_.patternMidTomSteps(), mini_acid_.patternHighTomSteps(),
                mini_acid_.patternRimSteps(), mini_acid_.patternClapSteps()
            };
            const bool* accents[NUM_DRUM_VOICES] = {
                mini_acid_.patternKickAccentSteps(), mini_acid_.patternSnareAccentSteps(),
                mini_acid_.patternHatAccentSteps(), mini_acid_.patternOpenHatAccentSteps(),
                mini_acid_.patternMidTomAccentSteps(), mini_acid_.patternHighTomAccentSteps(),
                mini_acid_.patternRimAccentSteps(), mini_acid_.patternClapAccentSteps()
            };
            for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
                for (int i = 0; i < SEQ_STEPS; ++i) {
                    g_drum_pattern_clipboard.pattern.voices[v].steps[i].hit = hits[v][i];
                    g_drum_pattern_clipboard.pattern.voices[v].steps[i].accent = accents[v][i];
                }
            }
            g_drum_pattern_clipboard.has_pattern = true;
        } else {
            // Copy 303
            int patIdx = mini_acid_.current303PatternIndex(selectedTrack_);
            const SynthPattern& source = mini_acid_.sceneManager().getSynthPattern(selectedTrack_, patIdx);
            g_pattern_clipboard.pattern = source;
            g_pattern_clipboard.has_pattern = true;
        }
        return true;
    }
    
    if (e.app_event_type == GROOVEPUTER_APP_EVENT_PASTE) {
        if (isDrumTrack(selectedTrack_)) {
            if (!g_drum_pattern_clipboard.has_pattern) return false;
            const DrumPatternSet& src = g_drum_pattern_clipboard.pattern;
            withAudioGuard([&]() {
                for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
                    for (int i = 0; i < SEQ_STEPS; ++i) {
                         mini_acid_.sceneManager().setDrumStep(v, i, src.voices[v].steps[i].hit, src.voices[v].steps[i].accent);
                    }
                }
            });
        } else {
            if (!g_pattern_clipboard.has_pattern) return false;
            const SynthPattern& src = g_pattern_clipboard.pattern;
            withAudioGuard([&]() {
                mini_acid_.sceneManager().editCurrentSynthPattern(selectedTrack_) = src;
            });
        }
        return true;
    }
    
    return false;
}

bool SequencerHubPage::handleVolumeInput(UIEvent& e) {
    if (mode_ != Mode::OVERVIEW) return false;

    // Volume control: Per-track fader logic
    // - Use Ctrl + Minus/Plus (overrides global master volume on this page)
    // - Use Alt + Left/Right (alternative)
    // Note: Ctrl + '=' is often '+' without shift, checking both covers bases
    bool isVolUp = (e.ctrl && (e.key == '=' || e.key == '+')) || (e.alt && UIInput::isRight(e));
    bool isVolDn = (e.ctrl && (e.key == '-' || e.key == '_')) || (e.alt && UIInput::isLeft(e));

    if (isVolUp || isVolDn) {
        float vol = mini_acid_.getTrackVolume((VoiceId)selectedTrack_);
        if (isVolDn) {
            vol -= 0.05f;
            if (vol < 0.0f) vol = 0.0f;
            mini_acid_.setTrackVolume((VoiceId)selectedTrack_, vol);
            return true;
        }
        if (isVolUp) {
            vol += 0.05f;
            if (vol > 1.2f) vol = 1.2f; // Slight boost allowed
            mini_acid_.setTrackVolume((VoiceId)selectedTrack_, vol);
            return true;
        }
    }
    return false;
}

bool SequencerHubPage::handleNavigation(UIEvent& e) {
    auto isNavKey = [](const UIEvent& ev) {
        return UIInput::isUp(ev) || UIInput::isDown(ev) || UIInput::isLeft(ev) || UIInput::isRight(ev);
    };

    if (!isNavKey(e)) return false;

    if (mode_ == Mode::OVERVIEW) {
        if (UIInput::isUp(e)) {
            selectedTrack_ = (selectedTrack_ - 1 + kHubTrackCount) % kHubTrackCount;
            syncOverviewScroll();
            return true;
        }
        if (UIInput::isDown(e)) {
            selectedTrack_ = (selectedTrack_ + 1) % kHubTrackCount;
            syncOverviewScroll();
            return true;
        }



        // Horizontal navigation for "the square"
        if (UIInput::isLeft(e)) {
            stepCursor_ = (stepCursor_ - 1 + SEQ_STEPS) % SEQ_STEPS;
            return true;
        }
        if (UIInput::isRight(e)) {
            stepCursor_ = (stepCursor_ + 1) % SEQ_STEPS;
            return true;
        }
    } else {
        // Detail Mode (Drum Grid)
        if (isDrumTrack(selectedTrack_)) {
            return drumGrid_->handleEvent(e);
        }
        // 303 Detail Logic (Local) - now reachable only if transition fails? 
        // Or if we decide to keep it as fallback.
        if (UIInput::isLeft(e)) {
            stepCursor_ = (stepCursor_ - 1 + SEQ_STEPS) % SEQ_STEPS;
            return true;
        }
        if (UIInput::isRight(e)) {
            stepCursor_ = (stepCursor_ + 1) % SEQ_STEPS;
            return true;
        }
    }
    return false;
}

void SequencerHubPage::syncOverviewScroll() {
    if (selectedTrack_ < 0) selectedTrack_ = 0;
    if (selectedTrack_ >= kHubTrackCount) selectedTrack_ = kHubTrackCount - 1;

    int maxScroll = kHubTrackCount - kHubVisibleTracks;
    if (maxScroll < 0) maxScroll = 0;
    if (overviewScroll_ < 0) overviewScroll_ = 0;
    if (overviewScroll_ > maxScroll) overviewScroll_ = maxScroll;

    if (selectedTrack_ < overviewScroll_) {
        overviewScroll_ = selectedTrack_;
    } else if (selectedTrack_ >= overviewScroll_ + kHubVisibleTracks) {
        overviewScroll_ = selectedTrack_ - kHubVisibleTracks + 1;
    }

    if (overviewScroll_ < 0) overviewScroll_ = 0;
    if (overviewScroll_ > maxScroll) overviewScroll_ = maxScroll;
}

bool SequencerHubPage::handleGridEdit(UIEvent& e) {
    if (mode_ != Mode::DETAIL || focus_ != FocusLane::GRID) return false;

    char key = e.key;
    char lower = std::tolower(key);

    // ENTER or X: Toggle step
    if (key == '\n' || key == '\r' || lower == 'x') {
        withAudioGuard([&]() {
            if (isDrumTrack(selectedTrack_)) {
                mini_acid_.toggleDrumStep(voiceCursor_, stepCursor_);
            } else {
                // Synth: toggle by clearing or setting a note
                const int8_t* steps = mini_acid_.pattern303Steps(selectedTrack_);
                if (steps[stepCursor_] >= 0) {
                    mini_acid_.clear303StepNote(selectedTrack_, stepCursor_);
                } else {
                    // Add note: adjust303StepNote with positive delta triggers default note
                    mini_acid_.adjust303StepNote(selectedTrack_, stepCursor_, 1);
                }
            }
        });
        return true;
    }

    // A: Toggle Accent (Drums only, for 303 it's handled below as Note Up / Alt+Accent)
    if (lower == 'a' && isDrumTrack(selectedTrack_)) {
        withAudioGuard([&]() {
            mini_acid_.toggleDrumAccentStep(stepCursor_);
        });
        return true;
    }

    // === Note editing for 303 tracks only ===
    if (!isDrumTrack(selectedTrack_)) {
        // A/Z: Note +/- semitone
        if (lower == 'a') {
            if (e.alt) {
                 withAudioGuard([&]() { mini_acid_.toggle303AccentStep(selectedTrack_, stepCursor_); });
            } else {
                 withAudioGuard([&]() { mini_acid_.adjust303StepNote(selectedTrack_, stepCursor_, 1); });
            }
            return true;
        }
        if (lower == 'z') {
            withAudioGuard([&]() {
                mini_acid_.adjust303StepNote(selectedTrack_, stepCursor_, -1);
            });
            return true;
        }
        
        // S/X: Octave +/- 1
        if (lower == 's') {
            if (e.alt) {
                 withAudioGuard([&]() { mini_acid_.toggle303SlideStep(selectedTrack_, stepCursor_); });
            } else {
                 withAudioGuard([&]() { mini_acid_.adjust303StepOctave(selectedTrack_, stepCursor_, 1); });
            }
            return true;
        }
        if (lower == 'x') {
            focus_ = FocusLane::GRID; 
             withAudioGuard([&]() { mini_acid_.adjust303StepOctave(selectedTrack_, stepCursor_, -1); });
            return true;
        }
    }

    return false;
}
