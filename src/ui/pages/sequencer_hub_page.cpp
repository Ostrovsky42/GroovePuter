#include "sequencer_hub_page.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../ui_colors.h"
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
    if (mode_ == Mode::OVERVIEW) {
        drawOverview(gfx);
    } else {
        drawDetail(gfx);
    }
}

void SequencerHubPage::drawOverview(IGfx& gfx) {
    UI::drawStandardHeader(gfx, mini_acid_, "SEQUENCER HUB [OVERVIEW]");
    LayoutManager::clearContent(gfx);

    const int startY = LayoutManager::lineY(0);
    const int rowH = 10;
    
    for (int i = 0; i < 10; i++) {
        drawTrackRow(gfx, i, startY + i * (rowH + 1), rowH, i == selectedTrack_);
    }

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
        
        IGfxColor color = hit ? (selected ? COLOR_WHITE : COLOR_GRAY) : COLOR_BLACK;
        if (s == currentStep && mini_acid_.isPlaying()) color = IGfxColor::Yellow();
        
        gfx.fillRect(maskX + s * cellW, y + 2, cellW - 1, h - 4, color);
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

    // Fast return for global nav
    if (UIInput::isGlobalNav(e)) return false;

    if (handleModeSwitch(e)) return true;
    if (handleQuickKeys(e)) return true;
    if (handleNavigation(e)) return true;
    if (handleGridEdit(e)) return true;

    return false;
}

bool SequencerHubPage::handleModeSwitch(UIEvent& e) {
    // ENTER: toggle mode or action depending on context
    if (e.key == '\n' || e.key == '\r') {
        if (mode_ == Mode::OVERVIEW) {
            mode_ = Mode::DETAIL;
            focus_ = FocusLane::GRID;
            return true;
        }
        // In DETAIL, ENTER does edit action -> handled elsewhere
        return false;
    }
    
    // ESC: back to overview (but don't steal if already in overview)
    if (e.key == 0x1B) {
        if (mode_ == Mode::DETAIL) {
            mode_ = Mode::OVERVIEW;
            return true;
        }
        // Don't consume - let parent handle page back
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

bool SequencerHubPage::handleNavigation(UIEvent& e) {
    int nav = UIInput::navCode(e);
    if (!nav) return false;

    // OVERVIEW: UP/DOWN selects track
    if (mode_ == Mode::OVERVIEW) {
        if (nav == MINIACID_UP) {
            selectedTrack_ = (selectedTrack_ - 1 + 10) % 10;
            return true;
        }
        if (nav == MINIACID_DOWN) {
            selectedTrack_ = (selectedTrack_ + 1) % 10;
            return true;
        }
    }
    
    // DETAIL: GRID focus navigation
    if (mode_ == Mode::DETAIL && focus_ == FocusLane::GRID) {
        if (nav == MINIACID_LEFT) {
            stepCursor_ = (stepCursor_ - 1 + SEQ_STEPS) % SEQ_STEPS;
            return true;
        }
        if (nav == MINIACID_RIGHT) {
            stepCursor_ = (stepCursor_ + 1) % SEQ_STEPS;
            return true;
        }
        
        switch (nav) {
            case MINIACID_UP:    
                if (isDrumTrack(selectedTrack_)) {
                    voiceCursor_ = (voiceCursor_ - 1 + NUM_DRUM_VOICES) % NUM_DRUM_VOICES;
                    return true;
                }
                return false;
            case MINIACID_DOWN:
                if (isDrumTrack(selectedTrack_)) {
                    voiceCursor_ = (voiceCursor_ + 1) % NUM_DRUM_VOICES;
                    return true;
                }
                return false;
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

    // A: Toggle Slide (Synth only)
    if (lower == 'a' && !isDrumTrack(selectedTrack_)) {
        withAudioGuard([&]() {
            mini_acid_.toggle303SlideStep(selectedTrack_, stepCursor_);
        });
        return true;
    }

    return false;
}
