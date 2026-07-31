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
            if (keyboard_.target() == MusicalEventTarget::Drums) {
                std::snprintf(toast, sizeof(toast), "DRUMS -> MIDI CH 1-7");
            } else {
                std::snprintf(toast, sizeof(toast), "%s -> MIDI CH %u",
                              keyboard_.targetName(),
                              static_cast<unsigned>(keyboard_.targetMidiChannel()));
            }
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
    if (keyboard_.target() == MusicalEventTarget::Drums) {
        std::snprintf(line, sizeof(line), "TARGET:DRUMS  MIDI CH:1-7");
    } else {
        std::snprintf(line, sizeof(line), "TARGET:%s  MIDI CH:%u",
                      keyboard_.targetName(),
                      static_cast<unsigned>(keyboard_.targetMidiChannel()));
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    if (!keyboard_.noteModeEnabled()) {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2),
                     "LEGACY KEY COMMANDS ACTIVE");
    } else if (miniAcid_.isPlaying()) {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2),
                     "PATTERN PLAYER BLOCKS LIVE");
    } else if (keyboard_.target() == MusicalEventTarget::Drums) {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2),
                     "A-J:K/S/C/H1/H2/P1/P2");
    } else if (keyboard_.target() == MusicalEventTarget::Dx) {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2),
                     "SEQTRAK DX - USB MIDI ONLY");
    } else {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2),
                     "LIVE INTERNAL + USB MIDI");
    }

    if (keyboard_.target() == MusicalEventTarget::Drums) {
        std::snprintf(line, sizeof(line), "PAD NOTE:60  7 NATIVE LANES");
    } else {
        std::snprintf(line, sizeof(line), "ROOT:C SCALE:%s OCT:%+d",
                      keyboard_.scaleName(),
                      static_cast<int>(keyboard_.octaveShift()));
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), line);

    std::snprintf(line, sizeof(line), "HELD:%u",
                  static_cast<unsigned>(keyboard_.heldCount()));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), line);

    if (keyboard_.target() == MusicalEventTarget::Drums) {
        std::snprintf(line, sizeof(line), "A1 S2 D3 F4 G5 H6 J7");
    } else if (active >= 0) {
        const int octave = active / 12 - 1;
        std::snprintf(line, sizeof(line), "NOTE:%s%d MIDI:%d",
                      noteName(active), octave, active);
    } else {
        std::snprintf(line, sizeof(line), "NOTE:--");
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(5), line);

    gfx.setTextColor(COLOR_LABEL);
    if (keyboard_.target() == MusicalEventTarget::Drums) {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(6),
                     "CH1 KICK ... CH7 PERC2");
    } else {
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(6),
                     "QWERTY:+12  ASDF:BASE");
    }
}

void PerformPage::drawFooter(IGfx& gfx) {
    UI::drawStandardFooter(gfx,
                           "\\:A/B/DX/DR N:Note",
                           "-/=:Oct X:Panic Fn+Tab:Mode");
}
