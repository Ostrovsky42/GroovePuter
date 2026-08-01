#include "perform_page.h"

#include <cstdio>

#include "../components/music_visuals.h"

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
    const int velocity = keyboard_.activeVelocity();
    const bool drums = keyboard_.target() == MusicalEventTarget::Drums;
    const bool noteMode = keyboard_.noteModeEnabled();
    char line[64];

    // Primary stage-readable layer: three stable badges that remain legible in
    // photos/video without turning the whole screen into large typography.
    int x = Layout::COL_1;
    const int chipY = LayoutManager::lineY(0);
    x += MusicVisuals::drawChip(gfx, x, chipY,
                                noteMode ? "NOTE ON" : "NOTE OFF",
                                noteMode,
                                noteMode ? MusicVisuals::accentForStyle() : COLOR_DANGER) + 3;

    x += MusicVisuals::drawChip(gfx, x, chipY, keyboard_.targetName(), true) + 3;

    char channel[12];
    if (drums) {
        std::snprintf(channel, sizeof(channel), "CH1-7");
    } else {
        std::snprintf(channel, sizeof(channel), "CH%u",
                      static_cast<unsigned>(keyboard_.targetMidiChannel()));
    }
    MusicVisuals::drawChip(gfx, x, chipY, channel, false);

    // Secondary information density stays compact/dim: useful at arm's length,
    // but it does not compete with target/piano/pads when filmed from farther away.
    gfx.setTextColor(COLOR_LABEL);
    if (drums) {
        std::snprintf(line, sizeof(line), "NATIVE 7-LANE  HELD:%u  VEL:%d",
                      static_cast<unsigned>(keyboard_.heldCount()), velocity);
    } else {
        std::snprintf(line, sizeof(line), "ROOT:C  %-8s OCT:%+d  HELD:%u",
                      keyboard_.scaleName(),
                      static_cast<int>(keyboard_.octaveShift()),
                      static_cast<unsigned>(keyboard_.heldCount()));
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    const int visualY = LayoutManager::lineY(2);
    const int visualH = LayoutManager::lineY(6) - visualY - 2;
    const int visualW = Layout::CONTENT.w - 8;
    if (drums) {
        MusicVisuals::drawDrumPads(gfx, Layout::COL_1, visualY,
                                   visualW, visualH, keyboard_);
    } else {
        MusicVisuals::drawPiano(gfx, Layout::COL_1, visualY,
                                visualW, visualH, keyboard_);
    }

    // Micro-HUD: keep operational detail available without shrinking the main
    // instrument geometry. These rows intentionally use normal 5x7 UI text.
    gfx.setTextColor(COLOR_LABEL);
    if (!noteMode) {
        std::snprintf(line, sizeof(line), "LEGACY KEYS | N ENABLE NOTE MODE");
    } else if (miniAcid_.isPlaying()) {
        std::snprintf(line, sizeof(line), "INPUT LOCK | PATTERN PLAYER OWNS LIVE");
    } else if (drums) {
        std::snprintf(line, sizeof(line), "USB CH1-7 | A/S/D/F/G/H/J NATIVE PADS");
    } else if (keyboard_.target() == MusicalEventTarget::Dx) {
        std::snprintf(line, sizeof(line), "USB ONLY | ASDF BASE | QWERTY +12");
    } else {
        std::snprintf(line, sizeof(line), "INT+USB | ASDF BASE | QWERTY +12");
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(6), line);

    gfx.setTextColor(active >= 0 ? MusicVisuals::accentForStyle() : COLOR_LABEL);
    if (!drums && active >= 0) {
        const int octave = active / 12 - 1;
        std::snprintf(line, sizeof(line), "NOTE %s%d | MIDI:%d | VEL:%d | H:%u",
                      noteName(active), octave, active, velocity,
                      static_cast<unsigned>(keyboard_.heldCount()));
    } else if (drums && keyboard_.heldCount() > 0) {
        std::snprintf(line, sizeof(line), "PAD ACTIVE | N60 | VEL:%d | LANES:7", velocity);
    } else if (drums) {
        std::snprintf(line, sizeof(line), "READY | NOTE60 | CH1..7 | LANES:7");
    } else {
        std::snprintf(line, sizeof(line), "READY | RANGE:C0-B6 | OCT:-2..+2");
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), line);
}

void PerformPage::drawFooter(IGfx& gfx) {
    UI::drawStandardFooter(gfx,
                           "\\ Target  N Note  ,/. Scale",
                           "-/+ Oct  X Panic");
}
