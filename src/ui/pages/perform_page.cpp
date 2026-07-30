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
        case '1':
            requestPageTransition(WorkflowPages::kPerform);
            return true;
        case '2':
            requestPageTransition(WorkflowPages::kPattern);
            return true;
        case '3':
            requestPageTransition(WorkflowPages::kArrange);
            return true;
        case '[':
            keyboard_.cycleScale(-1);
            return true;
        case ']':
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
        case 'X':
            keyboard_.panic();
            UI::showToast("PANIC: ALL NOTES OFF", 1000);
            return true;
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
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0),
                 miniAcid_.isPlaying() ? "PATTERN PLAYER OWNS SYNTH A"
                                       : "LIVE KEYBOARD -> SYNTH A");

    gfx.setTextColor(COLOR_TEXT);
    std::snprintf(line, sizeof(line), "ROOT:C  SCALE:%s", keyboard_.scaleName());
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    std::snprintf(line, sizeof(line), "OCT:%+d  HELD:%u",
                  static_cast<int>(keyboard_.octaveShift()),
                  static_cast<unsigned>(keyboard_.heldCount()));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), line);

    if (active >= 0) {
        const int octave = active / 12 - 1;
        std::snprintf(line, sizeof(line), "NOTE:%s%d  MIDI:%d",
                      noteName(active), octave, active);
    } else {
        std::snprintf(line, sizeof(line), "NOTE:--");
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), line);

    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), "QWERTYUIOP");
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(5), "ASDFGHJKL");
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(6),
                 miniAcid_.isPlaying() ? "STOP TRANSPORT TO PLAY"
                                       : "LAST NOTE PRIORITY");
}

void PerformPage::drawFooter(IGfx& gfx) {
    UI::drawStandardFooter(gfx,
                           "[ ]:Scale  -/=:Oct  X:Panic",
                           "1:PERF 2:PAT 3:ARR");
}
