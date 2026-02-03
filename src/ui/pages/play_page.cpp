#include "play_page.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include <cstdio>

PlayPage::PlayPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {}

void PlayPage::draw(IGfx& gfx) {
    char modeStr[32];
    snprintf(modeStr, sizeof(modeStr), "%s", 
             mode_ == Mode::OVERVIEW ? "OVERVIEW" : "DETAIL");
    
    UI::drawStandardHeader(gfx, mini_acid_, modeStr);
    
    // Content
    LayoutManager::clearContent(gfx);
    
    if (mode_ == Mode::OVERVIEW) {
        drawOverview(gfx);
    } else {
        drawDetail(gfx);
    }
    
    // QWERTY-friendly footer
    const char* footerLeft;
    const char* footerRight;
    if (mode_ == Mode::OVERVIEW) {
        footerLeft = "[SPC]PLAY [ARROWS]TRK [ENT]DETAIL";
        footerRight = "[G]GENRE";
    } else {
        footerLeft = "[SPC]PLAY [L/R]STEP [x]TOGGLE";
        footerRight = "[ESC]BACK";
    }

    // Channel activity bar (10 tracks: 2 synths + 8 drum voices)
    bool active[10];
    for (int i = 0; i < 10; ++i) {
        active[i] = mini_acid_.isTrackActive(i);
    }
    UI::drawChannelActivityBar(gfx, 8, Layout::FOOTER.y - 10, Layout::FOOTER.w - 16, 4, active, 10);

    UI::drawStandardFooter(gfx, footerLeft, footerRight);
}


void PlayPage::drawOverview(IGfx& gfx) {
    // Линия 0: информация
    int y0 = LayoutManager::lineY(0);
    
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(Layout::COL_1, y0, "STEP:");
    gfx.setTextColor(COLOR_KNOB_2);
    
    char stepStr[16];
    snprintf(stepStr, sizeof(stepStr), "%02d/16", getCurrentStep() + 1);
    gfx.drawText(Layout::COL_1 + 30, y0, stepStr);
    
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(Layout::COL_2, y0, "BANK:");
    gfx.setTextColor(COLOR_KNOB_2);
    char bankStr[4] = {getBankChar(), '\0'};
    gfx.drawText(Layout::COL_2 + 30, y0, bankStr);
    
    // SW: using COL_2 + offset to fit in 240px
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(Layout::COL_2 + 50, y0, "SW:");
    gfx.setTextColor(COLOR_KNOB_2);
    char swingStr[8];
    snprintf(swingStr, sizeof(swingStr), "%d%%", getSwingPercent());
    gfx.drawText(Layout::COL_2 + 70, y0, swingStr);
    
    // Линии 1-5: треки (компактные)
    for (int i = 0; i < 5; i++) {
        int y = LayoutManager::lineY(1 + i);
        uint16_t mask = getStepMask(i);
        
        // Выделение текущего трека
        bool selected = (i == selectedTrack_);
        if (selected) {
            gfx.fillRect(Layout::COL_1, y - 1, 232, 11, COLOR_KNOB_1);
            gfx.setTextColor(COLOR_BLACK);
        } else {
            gfx.setTextColor(COLOR_WHITE);
        }
        
        // Рисуем имя трека
        const char* name = getTrackName(i);
        Widgets::drawClippedText(gfx, Layout::COL_1 + 2, y, 34, name);
        
        // Рисуем шаги (компактный режим)
        Widgets::drawStepRow(gfx, Layout::COL_1 + 36, y, 
                            232 - 38, 
                            "", mask, getCurrentStep(), true);
    }
}

void PlayPage::drawDetail(IGfx& gfx) {
    // Детальный вид одного трека
    int y0 = LayoutManager::lineY(0);
    
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(Layout::COL_1, y0, "TRACK:");
    gfx.setTextColor(COLOR_KNOB_2);
    Widgets::drawClippedText(gfx, Layout::COL_1 + 40, y0, 80, getTrackName(selectedTrack_));
    
    // Полноразмерный степ-ряд
    int y1 = LayoutManager::lineY(2);
    uint16_t mask = getStepMask(selectedTrack_);
    Widgets::drawStepRow(gfx, Layout::COL_1, y1, 232,
                        "", mask, getCurrentStep(), false);
    
    // Информация о текущем шаге
    int y3 = LayoutManager::lineY(5);
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(Layout::COL_1, y3, "STEP:");
    gfx.setTextColor(COLOR_KNOB_2);
    gfx.drawText(Layout::COL_1 + 40, y3, 
                mask & (1 << (getCurrentStep() % 16)) ? "ON" : "OFF");
    
    // Параметры (Только для 303 голосов)
    if (selectedTrack_ < 2) {
        int accentCount = 0;
        int slideCount = 0;
        const bool* accents = mini_acid_.pattern303AccentSteps(selectedTrack_);
        const bool* slides = mini_acid_.pattern303SlideSteps(selectedTrack_);
        for (int i = 0; i < 16; i++) {
            if (accents[i]) accentCount++;
            if (slides[i]) slideCount++;
        }

        int y4 = LayoutManager::lineY(6);
        gfx.setTextColor(COLOR_WHITE);
        gfx.drawText(Layout::COL_1, y4, "ACCENT:");
        gfx.setTextColor(COLOR_KNOB_2);
        char accBuf[8]; snprintf(accBuf, sizeof(accBuf), "%d", accentCount);
        gfx.drawText(Layout::COL_1 + 50, y4, accBuf);
        
        int y5 = LayoutManager::lineY(7);
        gfx.setTextColor(COLOR_WHITE);
        gfx.drawText(Layout::COL_2, y5, "SLIDE:");
        gfx.setTextColor(COLOR_KNOB_2);
        char slideBuf[8]; snprintf(slideBuf, sizeof(slideBuf), "%d", slideCount);
        gfx.drawText(Layout::COL_2 + 40, y5, slideBuf);
    }
}

bool PlayPage::handleEvent(UIEvent& e) {
    if (e.event_type != MINIACID_KEY_DOWN) return false;
    
    // 1) Check SCANCODE for arrow keys (primary navigation)
    switch (UIInput::navCode(e)) {
        case MINIACID_UP:   prevTrack(); return true;
        case MINIACID_DOWN: nextTrack(); return true;
        case MINIACID_LEFT:  return true; // Could move step cursor
        case MINIACID_RIGHT: return true; // Could move step cursor
        default: break;
    }
    
    // 2) Check KEY for character keys
    switch (e.key) {
        case ' ': // SPACE - Play/Stop
            togglePlayback();
            return true;
        
        case '\n': case '\r': // ENTER - Toggle mode
            mode_ = (mode_ == Mode::OVERVIEW) ? Mode::DETAIL : Mode::OVERVIEW;
            return true;
        
        case 0x1B: // ESC
        case 0x08: // Backspace
            if (mode_ == Mode::DETAIL) {
                mode_ = Mode::OVERVIEW;
                return true;
            }
            return false; // Let parent handle page navigation
        
        // vim-style aliases
        case 'i': prevTrack(); return true;
        case 'k': nextTrack(); return true;
        case 'j': return true; // left
        case 'l': return true; // right
            
        case 'x': // Toggle step in Detail
            if (mode_ == Mode::DETAIL) {
                withAudioGuard([&](){ /* Logic to toggle step */ });
            }
            return true;
            
        // Global keys - return false to let parent handle
        case 'g': case 't': case 'm': case 's': case 'p':
        case '[': case ']': case 'h': case 'b': case 'B':
            return false;
    }
    
    return false;
}




void PlayPage::togglePlayback() {
    withAudioGuard([&](){
        if (mini_acid_.isPlaying()) {
            mini_acid_.stop();
        } else {
            mini_acid_.start();
        }
    });
}

void PlayPage::nextTrack() {
    selectedTrack_ = (selectedTrack_ + 1) % 5;
}

void PlayPage::prevTrack() {
    selectedTrack_ = (selectedTrack_ - 1 + 5) % 5;
}

uint16_t PlayPage::getStepMask(int track) const {
    if (track < 2) {
        const int8_t* steps = mini_acid_.pattern303Steps(track);
        uint16_t mask = 0;
        for (int i = 0; i < 16; i++) {
            if (steps[i] >= 0) mask |= (1 << i);
        }
        return mask;
    } else {
        const bool* steps = nullptr;
        if (track == 2) steps = mini_acid_.patternKickSteps();
        else if (track == 3) steps = mini_acid_.patternSnareSteps();
        else if (track == 4) steps = mini_acid_.patternHatSteps();
        
        uint16_t mask = 0;
        if (steps) {
            for (int i = 0; i < 16; i++) {
                if (steps[i]) mask |= (1 << i);
            }
        }
        return mask;
    }
}

const char* PlayPage::getTrackName(int track) const {
    static const char* names[] = {
        "BASS", "LEAD", "KICK", "SNARE", "HAT"
    };
    return (track >= 0 && track < 5) ? names[track] : "???";
}

int PlayPage::getCurrentStep() const {
    return mini_acid_.currentStep();
}

int PlayPage::getSwingPercent() const {
    return (int)(mini_acid_.swing() * 100.0f + 0.5f);
}

char PlayPage::getBankChar() const {
    return 'A' + (mini_acid_.currentScene() / 16);
}
