#include "sequencer_hub_page.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#ifndef USE_RETRO_THEME
#define USE_RETRO_THEME
#endif
#ifndef USE_AMBER_THEME
#define USE_AMBER_THEME
#endif
#include "../retro_ui_theme.h"
#include "../retro_widgets.h"
#include "../amber_ui_theme.h"
#include "../amber_widgets.h"
#include <cstdio>

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
}

void SequencerHubPage::draw(IGfx& gfx) {
    switch (UI::currentStyle) {
        case VisualStyle::RETRO_CLASSIC:
            drawRetroClassicStyle(gfx);
            break;
        case VisualStyle::AMBER:
            drawAmberStyle(gfx);
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
        // Simple List of 10 Tracks (Retro Style)
        int rowH = 10;
        int spacing = 1;
        for (int i = 0; i < 10; i++) {
            int ry = contentY + i * (rowH + spacing);
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
            if (i == 0) std::strcpy(name, "303 A");
            else if (i == 1) std::strcpy(name, "303 B");
            else std::snprintf(name, sizeof(name), "DRM %d", i - 1);
            
            if (selected) {
                drawGlowText(gfx, x + 6, ry + 1, name, IGfxColor(FOCUS_GLOW), IGfxColor(TEXT_PRIMARY));
            } else {
                gfx.setTextColor(IGfxColor(TEXT_SECONDARY));
                gfx.drawText(x + 6, ry + 1, name);
            }

            // Tiny mask
            int maskX = x + 65;
            int cellW = (w - maskX - 10) / 16;
            for (int s = 0; s < 16; s++) {
                bool hit = false;
                if (i < 2) hit = mini_acid_.pattern303Steps(i)[s] >= 0;
                else {
                    int v = i - 2;
                    if (v == 0) hit = mini_acid_.patternKickSteps()[s] > 0;
                    else if (v == 1) hit = mini_acid_.patternSnareSteps()[s] > 0;
                    else if (v == 2) hit = mini_acid_.patternHatSteps()[s] > 0;
                }
                
                IGfxColor color = hit ? (selected ? IGfxColor(trackColor) : IGfxColor(GRID_MEDIUM)) : IGfxColor(BG_INSET);
                if (s == playingStep && isPlaying) color = IGfxColor(NEON_YELLOW);
                gfx.fillRect(maskX + s * cellW, ry + 2, cellW - 1, rowH - 4, color);
            }
            
            if (selected) {
                 drawOverviewCursor(gfx, i, stepCursor_, maskX, ry + 2, cellW, rowH - 4);
            }

            // Volume indicator (Retro)
            float vol = mini_acid_.getTrackVolume((VoiceId)i);
            int volBarW = 20;
            int volBarX = x + w - volBarW - 12;
            int volW = (int)(vol * volBarW);
            gfx.fillRect(volBarX, ry + 3, volBarW, rowH - 6, IGfxColor(BG_INSET));
            gfx.fillRect(volBarX, ry + 4, volW, rowH - 8, IGfxColor(trackColor));
            
            // Activity LED (Retro Hardware style)
            bool active = mini_acid_.isTrackActive(i);
            RetroWidgets::drawLED(gfx, volBarX + volBarW + 5, ry + (rowH/2), 2, active && isPlaying, IGfxColor(trackColor));
        }
        
        // Channel activity bar (from Play Page merge)
        bool active[10];
        for (int i = 0; i < 10; ++i) active[i] = mini_acid_.isTrackActive(i);
        UI::drawChannelActivityBar(gfx, 8, Layout::FOOTER.y - 12, w - 16, 4, active, 10);

        // Scanlines disabled: caused flicker on small TFT
        RetroWidgets::drawFooterBar(gfx, x, y + h - 12, w, 12, "[UP/DN]Track [ENT]Detail", "SPACE:Play", "HUB");
    } else {
        // DETAIL MODE
        if (isDrumTrack(selectedTrack_)) {
            drumGrid_->setBoundaries(Rect(0, contentY + 2, 240, contentH - 4));
            drumGrid_->draw(gfx);
            drawFooterBar(gfx, x, y + h - 12, w, 12, "[ARROWS]Grid [W]Accent", "ESC:Back", "DRUM");
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
            RetroWidgets::drawFooterBar(gfx, x, y + h - 12, w, 12, "[A/Z]±nt [S/X]±oct [Alt+S]Sld [Alt+A]Acc", "ESC:Back", "303");
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
        int rowH = 10;
        int spacing = 1;
        for (int i = 0; i < 10; i++) {
            int ry = contentY + i * (rowH + spacing);
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
            if (i == 0) std::strcpy(name, "303 A");
            else if (i == 1) std::strcpy(name, "303 B");
            else std::snprintf(name, sizeof(name), "DRM %d", i - 1);
            
            if (selected) {
                AmberWidgets::drawGlowText(gfx, x + 6, ry + 1, name, IGfxColor(AmberTheme::FOCUS_GLOW), IGfxColor(AmberTheme::TEXT_PRIMARY));
            } else {
                gfx.setTextColor(IGfxColor(AmberTheme::TEXT_SECONDARY));
                gfx.drawText(x + 6, ry + 1, name);
            }

            int maskX = x + 65;
            int cellW = (w - maskX - 10) / 16;
            for (int s = 0; s < 16; s++) {
                bool hit = false;
                if (i < 2) hit = mini_acid_.pattern303Steps(i)[s] >= 0;
                else {
                    int v = i - 2;
                    if (v == 0) hit = mini_acid_.patternKickSteps()[s] > 0;
                    else if (v == 1) hit = mini_acid_.patternSnareSteps()[s] > 0;
                    else if (v == 2) hit = mini_acid_.patternHatSteps()[s] > 0;
                }
                
                IGfxColor color = hit ? (selected ? IGfxColor(AmberTheme::NEON_CYAN) : IGfxColor(AmberTheme::GRID_MEDIUM)) : IGfxColor(AmberTheme::BG_INSET);
                if (s == playingStep && isPlaying) color = IGfxColor(AmberTheme::NEON_YELLOW);
                gfx.fillRect(maskX + s * cellW, ry + 2, cellW - 1, rowH - 4, color);
            }
            
            if (selected) {
                 drawOverviewCursor(gfx, i, stepCursor_, maskX, ry + 2, cellW, rowH - 4);
            }

            // Volume indicator (Amber)
            float vol = mini_acid_.getTrackVolume((VoiceId)i);
            int volBarW = 20;
            int volBarX = x + w - volBarW - 12;
            int volW = (int)(vol * volBarW);
            gfx.fillRect(volBarX, ry + 3, volBarW, rowH - 6, IGfxColor(AmberTheme::BG_INSET));
            gfx.fillRect(volBarX, ry + 4, volW, rowH - 8, IGfxColor(AmberTheme::NEON_CYAN));

            // Activity LED (Amber Hardware style)
            bool active = mini_acid_.isTrackActive(i);
            AmberWidgets::drawLED(gfx, volBarX + volBarW + 5, ry + (rowH/2), 2, active && isPlaying, IGfxColor(AmberTrackColor));
        }
        
        bool active[10];
        for (int i = 0; i < 10; ++i) active[i] = mini_acid_.isTrackActive(i);
        UI::drawChannelActivityBar(gfx, 8, Layout::FOOTER.y - 12, w - 16, 4, active, 10);

        // Scanlines disabled: caused flicker on small TFT
        AmberWidgets::drawFooterBar(gfx, x, y + h - 12, w, 12, "[UP/DN]Track [ENT]Detail", "SPACE:Play", "HUB");
    } else {
        if (isDrumTrack(selectedTrack_)) {
            drumGrid_->setBoundaries(Rect(0, contentY + 2, 240, contentH - 4));
            drumGrid_->draw(gfx);
            AmberWidgets::drawFooterBar(gfx, x, y + h - 12, w, 12, "[ARROWS]Grid [W]Accent", "ESC:Back", "DRUM");
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
    LayoutManager::clearContent(gfx);

    const int startY = LayoutManager::lineY(0);
    const int rowH = 10;
    
    for (int i = 0; i < 10; i++) {
        drawTrackRow(gfx, i, startY + i * (rowH + 1), rowH, i == selectedTrack_);
    }
    
    // Channel activity bar (10 tracks: 2 synths + 8 drum voices)
    bool active[10];
    for (int i = 0; i < 10; ++i) {
        active[i] = mini_acid_.isTrackActive(i);
    }
    UI::drawChannelActivityBar(gfx, 8, Layout::FOOTER.y - 10, Layout::FOOTER.w - 16, 4, active, 10);

    UI::drawStandardFooter(gfx, 
        "[UP/DN] TRACK  [ENT] DETAIL", 
        "[SPACE] PLAY/STOP");
}

void SequencerHubPage::drawTrackRow(IGfx& gfx, int trackIdx, int y, int h, bool selected) {
    const int nameW = 60;
    const int ledX = 65;
    const int maskX = 80;
    const int maskW = 150;
    
    // Background highlight
    if (selected) {
        gfx.fillRect(2, y, 236, h, IGfxColor(0x282850));
    }
    
    // Name
    char name[16];
    if (trackIdx == 0) std::strcpy(name, "303 A");
    else if (trackIdx == 1) std::strcpy(name, "303 B");
    else std::snprintf(name, sizeof(name), "DRUM %d", trackIdx - 1);
    
    gfx.setTextColor(selected ? COLOR_WHITE : COLOR_GRAY);
    gfx.drawText(4, y + 1, name);
    
    // Activity LED
    bool active = mini_acid_.isTrackActive(trackIdx);
    gfx.fillRect(ledX, y + 2, 6, 6, active ? COLOR_ACCENT : COLOR_BLACK);
    gfx.drawRect(ledX, y + 2, 6, 6, COLOR_GRAY);
    
    // Mini step mask
    bool isSynth = !isDrumTrack(trackIdx);
    int currentStep = mini_acid_.currentStep();
    
    int cellW = maskW / SEQ_STEPS;
    for (int s = 0; s < SEQ_STEPS; s++) {
        bool hit = false;
        if (isSynth) {
            hit = mini_acid_.pattern303Steps(trackIdx)[s] >= 0;
        } else {
            int voice = getDrumVoiceIndex(trackIdx);
            // Different drums return different types, handle generically
            if (voice == 0) hit = mini_acid_.patternKickSteps()[s] > 0;
            else if (voice == 1) hit = mini_acid_.patternSnareSteps()[s] > 0;
            else if (voice == 2) hit = mini_acid_.patternHatSteps()[s] > 0;
            else if (voice == 3) hit = mini_acid_.patternOpenHatSteps()[s] > 0;
            else if (voice == 4) hit = mini_acid_.patternMidTomSteps()[s] > 0;
            else if (voice == 5) hit = mini_acid_.patternHighTomSteps()[s] > 0;
            else if (voice == 6) hit = mini_acid_.patternRimSteps()[s];
            else if (voice == 7) hit = mini_acid_.patternClapSteps()[s];
        }
        
        IGfxColor color = hit ? (selected ? (isSynth ? COLOR_ACCENT : COLOR_TEXT) : COLOR_GRAY) : COLOR_DARKER;
        if (s == currentStep && mini_acid_.isPlaying()) color = COLOR_WARN;
        
        gfx.fillRect(maskX + s * cellW, y + 2, cellW - 1, h - 4, color);
    }

    // Volume indicator
    float vol = mini_acid_.getTrackVolume((VoiceId)trackIdx);
    int volBarW = 20;
    int volBarX = 236 - volBarW - 4;
    int volW = (int)(vol * volBarW);
    gfx.drawRect(volBarX, y + 3, volBarW, h - 6, COLOR_GRAY);
    gfx.fillRect(volBarX + 1, y + 4, volW - 2, h - 8, COLOR_ACCENT);
    
    // Activity bar indicator (the "bar" from play page)
    if (active && mini_acid_.isPlaying()) {
        gfx.fillRect(volBarX + volBarW + 2, y + 2, 2, h - 4, COLOR_WARN);
    }
}

void SequencerHubPage::drawOverviewCursor(IGfx& gfx, int trackIdx, int stepIdx, int x, int y, int cellW, int cellH) {
     int cx = x + stepIdx * cellW;
     // Draw a prominent cursor around the step
     if (UI::currentStyle == VisualStyle::AMBER) {
         AmberWidgets::drawGlowBorder(gfx, cx, y, cellW - 1, cellH, IGfxColor(AmberTheme::SELECT_BRIGHT), 2);
     } else if (UI::currentStyle == VisualStyle::RETRO_CLASSIC) {
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
                gfx.fillRect(x + 2, gridY + 5, cellW - 5, 10, COLOR_WHITE);
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
    const char* left = "[ESC] BACK  [SPACE] PLAY";
    const char* right = isDrumTrack(selectedTrack_) ? "[W] ACCENT" : "[W] ACC  [A] SLIDE";
    UI::drawStandardFooter(gfx, left, right);
}

bool SequencerHubPage::handleEvent(UIEvent& e) {
    if (e.event_type == MINIACID_MOUSE_DOWN) {
        if (mode_ == Mode::DETAIL && isDrumTrack(selectedTrack_)) {
            return drumGrid_->handleEvent(e);
        }
        return false;
    }

    if (e.event_type != MINIACID_KEY_DOWN) return false;

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
    
    // Transport
    if (e.key == ' ') {
        withAudioGuard([&]() {
            if (mini_acid_.isPlaying()) mini_acid_.stop();
            else mini_acid_.start();
        });
        return true;
    }

    // Pattern quick select (Q-I)
    int patIdx = -1;
    if (lower == 'q') patIdx = 0;
    else if (lower == 'w' && mode_ == Mode::OVERVIEW) patIdx = 1; // 'w' has other meaning in DETAIL
    else if (lower == 'e') patIdx = 2;
    else if (lower == 'r') patIdx = 3;
    else if (lower == 't') patIdx = 4;
    else if (lower == 'y') patIdx = 5;
    else if (lower == 'u') patIdx = 6;
    else if (lower == 'i') patIdx = 7;
    
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
            selectedTrack_ = (selectedTrack_ - 1 + 10) % 10;
            return true;
        }
        if (UIInput::isDown(e)) {
            selectedTrack_ = (selectedTrack_ + 1) % 10;
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

    // W: Toggle Accent
    if (lower == 'w') {
        withAudioGuard([&]() {
            if (isDrumTrack(selectedTrack_)) {
                mini_acid_.toggleDrumAccentStep(stepCursor_);
            } else {
                mini_acid_.toggle303AccentStep(selectedTrack_, stepCursor_);
            }
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
