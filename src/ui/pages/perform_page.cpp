#include "perform_page.h"

#include <cstdio>

PerformPage::PerformPage(IGfx& gfx,
                         MiniAcid& miniAcid,
                         PerformanceKeyboard& keyboard)
    : miniAcid_(miniAcid), keyboard_(keyboard) {
    (void)gfx;
}

const char* PerformPage::noteName(int midiNote) {
    static const char* names[] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"
    };
    if (midiNote < 0) return "--";
    return names[midiNote % 12];
}

bool PerformPage::handleEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN || event.ctrl ||
        event.alt || event.meta) {
        return false;
    }

    switch (event.key) {
        case 'n':
        case 'N':
            keyboard_.toggleNoteMode();
            UI::showToast(keyboard_.noteModeEnabled()
                              ? "NOTE MODE: ON"
                              : "NOTE MODE: OFF",
                          900);
            return true;
        case '\\': {
            keyboard_.cycleTarget(1);
            char toast[40];
            std::snprintf(toast, sizeof(toast), "%s -> MIDI CH %u",
                          keyboard_.targetName(),
                          static_cast<unsigned>(keyboard_.targetMidiChannel()));
            UI::showToast(toast, 1000);
            return true;
        }
        case ',':
        case '<':
            keyboard_.cycleScale(-1);
            return true;
        case '.':
        case '>':
            keyboard_.cycleScale(1);
            return true;
        case '-':
            keyboard_.shiftOctave(-1);
            return true;
        case '=':
        case '+':
            keyboard_.shiftOctave(1);
            return true;
        case 'x':
        case 'X': {
            keyboard_.panic();
            char toast[40];
            std::snprintf(toast, sizeof(toast), "PANIC: %s OFF",
                          keyboard_.targetName());
            UI::showToast(toast, 1000);
            return true;
        }
        default:
            return false;
    }
}

void PerformPage::drawHeader(IGfx& gfx) {
    UI::drawStandardHeader(gfx, miniAcid_, "PERFORM");
}

void PerformPage::drawContent(IGfx& gfx) {
    LayoutManager::clearContent(gfx);

    const int active = keyboard_.activeNote();
    char line[48];

    gfx.setTextColor(COLOR_ACCENT);
    std::snprintf(line, sizeof(line), "NOTE MODE: %s",
                  keyboard_.noteModeEnabled() ? "ON" : "OFF");
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

    gfx.setTextColor(COLOR_TEXT);
    std::snprintf(line, sizeof(line), "TARGET:%s  MIDI CH:%u",
                  keyboard_.targetName(),
                  static_cast<unsigned>(keyboard_.targetMidiChannel()));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    if (!keyboard_.noteModeEnabled()) {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2),
                     "LEGACY KEY COMMANDS ACTIVE");
    } else if (miniAcid_.isPlaying()) {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2),
                     "PATTERN PLAYER BLOCKS LIVE");
    } else if (keyboard_.target() == MusicalEventTarget::Drums) {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2),
                     "DRUM ROUTING PENDING");
    } else {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2),
                     "LIVE INTERNAL + USB MIDI");
    }

    std::snprintf(line, sizeof(line), "ROOT:C SCALE:%s OCT:%+d",
                  keyboard_.scaleName(),
                  static_cast<int>(keyboard_.octaveShift()));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), line);

    std::snprintf(line, sizeof(line), "HELD:%u",
                  static_cast<unsigned>(keyboard_.heldCount()));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), line);

    if (active >= 0) {
        const int octave = active / 12 - 1;
        std::snprintf(line, sizeof(line), "NOTE:%s%d MIDI:%d",
                      noteName(active), octave, active);
    } else {
        std::snprintf(line, sizeof(line), "NOTE:--");
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(5), line);

    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(6),
                 "QWERTY:+12  ASDF:BASE");
}

void PerformPage::drawFooter(IGfx& gfx) {
    UI::drawStandardFooter(gfx,
                           "\\:A/B N:Note ,/.:Scale",
                           "-/=:Oct X:Panic Fn+Tab:Mode");
}
