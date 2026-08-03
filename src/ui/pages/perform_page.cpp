#include "perform_page.h"

#include <cstdio>
#include <cstring>

#include "../components/music_visuals.h"
#include "src/midi/smf_player_service.h"

PerformPage::PerformPage(IGfx& gfx,
                         MiniAcid& miniAcid,
                         PerformanceKeyboard& keyboard)
    : miniAcid_(miniAcid), keyboard_(keyboard) {
    (void)gfx;
}

const char* PerformPage::noteName(int midiNote) {
    static const char* names[] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"};
    if (midiNote < 0) return "--";
    return names[midiNote % 12];
}

bool PerformPage::handleEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN || event.ctrl || event.alt) {
        return false;
    }

    if (event.meta) {
        char toast[56];
        switch (event.key) {
            case 'a':
            case 'A':
                keyboard_.toggleArpeggiator();
                std::snprintf(toast, sizeof(toast), "ARP: %s %s",
                              keyboard_.arpeggiatorEnabled() ? "ON" : "OFF",
                              keyboard_.arpDirectionName());
                UI::showToast(toast, 900);
                return true;
            case 'c':
            case 'C':
                keyboard_.cycleChordMode(event.shift ? -1 : 1);
                std::snprintf(toast, sizeof(toast), "CHORD: %s",
                              keyboard_.chordModeName());
                UI::showToast(toast, 900);
                return true;
            case 'k':
            case 'K':
                if (keyboard_.heldCount() >= 2 && keyboard_.captureChordMemory()) {
                    std::snprintf(toast, sizeof(toast), "CHORD MEMORY: %u NOTES",
                                  static_cast<unsigned>(keyboard_.chordMemorySize()));
                } else {
                    keyboard_.clearChordMemory();
                    std::snprintf(toast, sizeof(toast), "CHORD MEMORY: CLEARED");
                }
                UI::showToast(toast, 1000);
                return true;
            case 's':
            case 'S':
                keyboard_.cycleStrum(event.shift ? -1 : 1);
                std::snprintf(toast, sizeof(toast), "STRUM: %u MS",
                              static_cast<unsigned>(keyboard_.strumMs()));
                UI::showToast(toast, 900);
                return true;
            case 'r':
            case 'R':
                keyboard_.cycleRatchet(event.shift ? -1 : 1);
                std::snprintf(toast, sizeof(toast), "RATCHET: X%u",
                              static_cast<unsigned>(keyboard_.ratchetCount()));
                UI::showToast(toast, 900);
                return true;
            case 'e':
            case 'E':
                if (event.shift) {
                    keyboard_.rotateEuclidean(1);
                    std::snprintf(toast, sizeof(toast), "EUCLID ROT: %u",
                                  static_cast<unsigned>(keyboard_.euclideanRotation()));
                } else {
                    keyboard_.cycleEuclideanPulses(1);
                    std::snprintf(toast, sizeof(toast), "EUCLID: %u/16",
                                  static_cast<unsigned>(keyboard_.euclideanPulses()));
                }
                UI::showToast(toast, 900);
                return true;
            case 'v':
            case 'V':
                keyboard_.cycleArpDirection(event.shift ? -1 : 1);
                std::snprintf(toast, sizeof(toast), "ARP DIR: %s",
                              keyboard_.arpDirectionName());
                UI::showToast(toast, 900);
                return true;
            default:
                return false;
        }
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
    keyboard_.setTempoBpm(miniAcid_.bpm());

    const int active = keyboard_.activeNote();
    const int velocity = keyboard_.activeVelocity();
    const bool drums = keyboard_.target() == MusicalEventTarget::Drums;
    const bool noteMode = keyboard_.noteModeEnabled();
    char line[72];

    int x = Layout::COL_1;
    const int chipY = LayoutManager::lineY(0);
    x += MusicVisuals::drawChip(gfx, x, chipY,
                                noteMode ? "NOTE ON" : "NOTE OFF",
                                noteMode,
                                noteMode ? MusicVisuals::accentForStyle()
                                         : COLOR_DANGER) + 3;
    x += MusicVisuals::drawChip(gfx, x, chipY, keyboard_.targetName(), true) + 3;

    char channel[12];
    if (drums) {
        std::snprintf(channel, sizeof(channel), "CH1-7");
    } else {
        std::snprintf(channel, sizeof(channel), "CH%u",
                      static_cast<unsigned>(keyboard_.targetMidiChannel()));
    }
    MusicVisuals::drawChip(gfx, x, chipY, channel, false);

    gfx.setTextColor(COLOR_LABEL);
    if (drums) {
        std::snprintf(line, sizeof(line), "NATIVE 7-LANE  HELD:%u  VEL:%d",
                      static_cast<unsigned>(keyboard_.heldCount()), velocity);
    } else {
        std::snprintf(line, sizeof(line), "%s O%+d C:%s A:%s R%u E%u S%u",
                      keyboard_.scaleName(),
                      static_cast<int>(keyboard_.octaveShift()),
                      keyboard_.chordModeName(),
                      keyboard_.arpeggiatorEnabled()
                          ? keyboard_.arpDirectionName() : "OFF",
                      static_cast<unsigned>(keyboard_.ratchetCount()),
                      static_cast<unsigned>(keyboard_.euclideanPulses()),
                      static_cast<unsigned>(keyboard_.strumMs()));
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    const int visualY = LayoutManager::lineY(2);
    const int visualH = LayoutManager::lineY(7) - visualY - 2;
    const int visualW = Layout::CONTENT.w - 8;
    if (drums) {
        MusicVisuals::drawDrumPads(gfx, Layout::COL_1, visualY,
                                   visualW, visualH, keyboard_);
    } else {
        MusicVisuals::drawPiano(gfx, Layout::COL_1, visualY,
                                visualW, visualH, keyboard_);
    }

    const GroovePuterMidi::ISmfPlayerService* player =
        GroovePuterMidi::smfPlayerService();
    const GroovePuterMidi::SmfPlayerSnapshot playerState =
        player ? player->snapshot() : GroovePuterMidi::SmfPlayerSnapshot{};
    const bool usbBlocked = std::strncmp(playerState.message,
                                         "USB MIDI BLOCKED", 16) == 0;
    const bool usbReady = std::strncmp(playerState.message,
                                       "USB MIDI READY", 14) == 0;

    if (usbBlocked) {
        gfx.setTextColor(COLOR_DANGER);
        std::snprintf(line, sizeof(line), "USB MIDI BLOCKED - KEYS WAIT");
    } else if (usbReady) {
        gfx.setTextColor(COLOR_WARN);
        std::snprintf(line, sizeof(line), "USB MIDI READY - PRESS PLAY");
    } else if (!noteMode) {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "NOTE MODE OFF | N ENABLE");
    } else if (miniAcid_.isPlaying()) {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "INPUT LOCK | PATTERN PLAYER ACTIVE");
    } else if (!drums && active >= 0) {
        const int octave = active / 12 - 1;
        gfx.setTextColor(MusicVisuals::accentForStyle());
        std::snprintf(line, sizeof(line), "NOTE %s%d | MIDI:%d | VEL:%d | H:%u",
                      noteName(active), octave, active, velocity,
                      static_cast<unsigned>(keyboard_.heldCount()));
    } else if (drums && keyboard_.heldCount() > 0) {
        gfx.setTextColor(MusicVisuals::accentForStyle());
        std::snprintf(line, sizeof(line), "PAD ACTIVE | N60 | VEL:%d | LANES:7", velocity);
    } else if (drums) {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "READY | A/S/D/F/G/H/J | CH1..7");
    } else if (keyboard_.target() == MusicalEventTarget::Dx) {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "USB ONLY | FN+A/C/K/S/R/E/V TOOLS");
    } else {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "INT+USB | FN+A/C/K/S/R/E/V TOOLS");
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), line);
}

void PerformPage::drawFooter(IGfx& gfx) {
    UI::drawStandardFooter(gfx,
                           "\\ Target  N Note  ,/. Scale",
                           "Fn A/C/K/S/R/E/V  X Panic");
}
