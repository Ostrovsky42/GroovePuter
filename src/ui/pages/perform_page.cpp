#include "perform_page.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../components/music_visuals.h"
#include "src/midi/smf_player_service.h"

namespace {
struct HeldPerformanceSnapshot {
    char keys[PerformanceKeyboard::kMaxHeldNotes]{};
    std::size_t count{0};
    char activeKey{0};
    uint8_t velocity{100};
};

HeldPerformanceSnapshot captureHeldPerformanceKeys(
    const PerformanceKeyboard& keyboard) {
    HeldPerformanceSnapshot snapshot{};
    const int activeNote = keyboard.activeNote();
    const int activeVelocity = keyboard.activeVelocity();
    if (activeVelocity > 0) {
        snapshot.velocity = static_cast<uint8_t>(activeVelocity);
    }

    for (char key = 'a'; key <= 'z'; ++key) {
        if (!PerformanceKeyboard::isPerformanceKey(key) ||
            !keyboard.isPhysicalKeyHeld(key)) {
            continue;
        }
        if (snapshot.count >= PerformanceKeyboard::kMaxHeldNotes) break;
        snapshot.keys[snapshot.count++] = key;

        uint8_t note = 0;
        if (activeNote >= 0 && keyboard.noteForKey(key, note) &&
            note == static_cast<uint8_t>(activeNote)) {
            snapshot.activeKey = key;
        }
    }
    return snapshot;
}

void restoreHeldPerformanceKeys(PerformanceKeyboard& keyboard,
                                const HeldPerformanceSnapshot& snapshot) {
    // Current tool setters call panic(), which clears the logical held-key
    // table even though the physical Cardputer keys remain down. Rehydrate the
    // snapshot once. The heldCount guard makes this safe when the core keyboard
    // implementation later preserves held keys itself.
    if (snapshot.count == 0 || keyboard.heldCount() != 0) return;

    for (std::size_t i = 0; i < snapshot.count; ++i) {
        const char key = snapshot.keys[i];
        if (key == snapshot.activeKey) continue;
        keyboard.keyDown(key, snapshot.velocity);
    }
    if (snapshot.activeKey != 0) {
        keyboard.keyDown(snapshot.activeKey, snapshot.velocity);
    }
}

bool strumIsAudible(const PerformanceKeyboard& keyboard) {
    return !keyboard.arpeggiatorEnabled() &&
           keyboard.chordMode() != PerformanceChordMode::Off;
}

bool rotationIsAudible(const PerformanceKeyboard& keyboard) {
    const uint8_t pulses = keyboard.euclideanPulses();
    return pulses > 0 && pulses < PerformanceKeyboard::kEuclideanSteps;
}
}  // namespace

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

bool PerformPage::handleToolKey(const UIEvent& event) {
    const int direction = event.shift ? -1 : 1;
    const HeldPerformanceSnapshot heldSnapshot =
        captureHeldPerformanceKeys(keyboard_);
    char toast[64];

    switch (event.key) {
        case '1':
            keyboard_.toggleArpeggiator();
            std::snprintf(toast, sizeof(toast), "ARP: %s %s",
                          keyboard_.arpeggiatorEnabled() ? "ON" : "OFF",
                          keyboard_.arpDirectionName());
            break;
        case '2':
            keyboard_.cycleArpDirection(direction);
            std::snprintf(toast, sizeof(toast), "ARP DIR: %s",
                          keyboard_.arpDirectionName());
            break;
        case '3':
            keyboard_.cycleChordMode(direction);
            std::snprintf(toast, sizeof(toast), "CHORD: %s",
                          keyboard_.chordModeName());
            break;
        case '4':
            if (keyboard_.heldCount() >= 2 && keyboard_.captureChordMemory()) {
                std::snprintf(toast, sizeof(toast), "MEMORY: %u NOTES",
                              static_cast<unsigned>(keyboard_.chordMemorySize()));
            } else if (keyboard_.chordMemorySize() > 0) {
                keyboard_.clearChordMemory();
                std::snprintf(toast, sizeof(toast), "MEMORY: CLEARED");
            } else {
                std::snprintf(toast, sizeof(toast), "MEMORY: HOLD 2+ NOTES");
            }
            break;
        case '5':
            keyboard_.cycleStrum(direction);
            if (keyboard_.arpeggiatorEnabled()) {
                std::snprintf(toast, sizeof(toast),
                              "STRUM: N/A / ARP IS SINGLE NOTE");
            } else if (keyboard_.chordMode() == PerformanceChordMode::Off) {
                std::snprintf(toast, sizeof(toast),
                              "STRUM: N/A / ENABLE CHORD");
            } else {
                std::snprintf(toast, sizeof(toast), "STRUM: %u MS",
                              static_cast<unsigned>(keyboard_.strumMs()));
            }
            break;
        case '6':
            keyboard_.cycleRatchet(direction);
            std::snprintf(toast, sizeof(toast), "RATCHET: X%u",
                          static_cast<unsigned>(keyboard_.ratchetCount()));
            break;
        case '7':
            keyboard_.cycleEuclideanPulses(direction);
            std::snprintf(toast, sizeof(toast), "EUCLID: %u/16",
                          static_cast<unsigned>(keyboard_.euclideanPulses()));
            break;
        case '8':
            keyboard_.rotateEuclidean(direction);
            if (keyboard_.euclideanPulses() == 0) {
                std::snprintf(toast, sizeof(toast),
                              "ROTATE: N/A / EUCLID OFF");
            } else if (keyboard_.euclideanPulses() >=
                       PerformanceKeyboard::kEuclideanSteps) {
                std::snprintf(toast, sizeof(toast),
                              "ROTATE: N/A / ALL 16 ACTIVE");
            } else {
                std::snprintf(toast, sizeof(toast), "EUCLID ROT: %u",
                              static_cast<unsigned>(keyboard_.euclideanRotation()));
            }
            break;
        case '9':
            keyboard_.toggleVoiceMode();
            std::snprintf(toast, sizeof(toast),
                          keyboard_.voiceMode() == PerformanceVoiceMode::Poly
                              ? "VOICE: POLY / EXT MIDI"
                              : "VOICE: MONO / EXT MIDI");
            break;
        default:
            return false;
    }

    restoreHeldPerformanceKeys(keyboard_, heldSnapshot);
    UI::showToast(toast, 900);
    return true;
}

void PerformPage::drawToolsLayer(IGfx& gfx) {
    const int leftX = Layout::COL_1;
    const int rightX = Layout::COL_2;
    char value[48];

    gfx.setTextColor(COLOR_ACCENT);
    gfx.drawText(leftX, LayoutManager::lineY(2), "PERFORMANCE TOOLS");

    gfx.setTextColor(COLOR_WHITE);
    std::snprintf(value, sizeof(value), "1 ARPEGGIATOR %s",
                  keyboard_.arpeggiatorEnabled() ? "ON" : "OFF");
    gfx.drawText(leftX, LayoutManager::lineY(3), value);
    if (strumIsAudible(keyboard_)) {
        std::snprintf(value, sizeof(value), "5 STRUM %ums",
                      static_cast<unsigned>(keyboard_.strumMs()));
    } else {
        std::snprintf(value, sizeof(value), "5 STRUM N/A");
    }
    gfx.drawText(rightX, LayoutManager::lineY(3), value);

    std::snprintf(value, sizeof(value), "2 DIRECTION %s", keyboard_.arpDirectionName());
    gfx.drawText(leftX, LayoutManager::lineY(4), value);
    std::snprintf(value, sizeof(value), "6 RATCHET x%u",
                  static_cast<unsigned>(keyboard_.ratchetCount()));
    gfx.drawText(rightX, LayoutManager::lineY(4), value);

    std::snprintf(value, sizeof(value), "3 CHORD %s", keyboard_.chordModeName());
    gfx.drawText(leftX, LayoutManager::lineY(5), value);
    std::snprintf(value, sizeof(value), "7 EUCLIDEAN %u/16",
                  static_cast<unsigned>(keyboard_.euclideanPulses()));
    gfx.drawText(rightX, LayoutManager::lineY(5), value);

    std::snprintf(value, sizeof(value), "4 MEMORY %u",
                  static_cast<unsigned>(keyboard_.chordMemorySize()));
    gfx.drawText(leftX, LayoutManager::lineY(6), value);
    if (rotationIsAudible(keyboard_)) {
        std::snprintf(value, sizeof(value), "8 ROTATE %u",
                      static_cast<unsigned>(keyboard_.euclideanRotation()));
    } else {
        std::snprintf(value, sizeof(value), "8 ROTATE N/A");
    }
    gfx.drawText(rightX, LayoutManager::lineY(6), value);

    gfx.setTextColor(COLOR_LABEL);
    std::snprintf(value, sizeof(value), "9 VOICE %s", keyboard_.voiceModeName());
    gfx.drawText(leftX, LayoutManager::lineY(7), value);
    gfx.drawText(rightX, LayoutManager::lineY(7), "EXT MIDI ONLY");
}

bool PerformPage::handleEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN ||
        event.ctrl || event.alt || event.meta) {
        return false;
    }

    const bool tabPressed =
        event.key == '\t' || event.scancode == GROOVEPUTER_TAB;
    if (tabPressed) {
        toolsLayerVisible_ = !toolsLayerVisible_;
        UI::showToast(toolsLayerVisible_
                          ? "PERFORMANCE TOOLS: 1-9"
                          : "PERFORMANCE TOOLS: CLOSED",
                      700);
        return true;
    }

    if (toolsLayerVisible_ &&
        (event.key == 0x1B || event.key == '`' ||
         event.scancode == GROOVEPUTER_ESCAPE)) {
        toolsLayerVisible_ = false;
        return true;
    }

    if (toolsLayerVisible_ && handleToolKey(event)) return true;

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
    const bool directPoly = keyboard_.directPolyphonyEnabled();
    const bool stepTools = keyboard_.arpeggiatorEnabled() ||
                           keyboard_.ratchetCount() > 1 ||
                           keyboard_.euclideanPulses() > 0;
    char line[72];

    int x = Layout::COL_1;
    const int chipY = LayoutManager::lineY(0);
    x += MusicVisuals::drawChip(gfx, x, chipY,
                                noteMode ? "NOTE ON" : "NOTE OFF",
                                noteMode,
                                noteMode ? MusicVisuals::accentForStyle()
                                         : COLOR_DANGER) + 3;
    x += MusicVisuals::drawChip(gfx, x, chipY, keyboard_.targetName(), true) + 3;
    if (!drums) {
        x += MusicVisuals::drawChip(gfx, x, chipY,
                                    keyboard_.voiceModeName(),
                                    directPoly) + 3;
    }

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

    if (toolsLayerVisible_) {
        drawToolsLayer(gfx);
        return;
    }

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
    } else if (miniAcid_.isPlaying() && !drums && active >= 0) {
        const int octave = active / 12 - 1;
        gfx.setTextColor(MusicVisuals::accentForStyle());
        std::snprintf(line, sizeof(line), "%s | NOTE %s%d | MIDI:%d | H:%u",
                      directPoly ? "POLY EXT" : "MONO EXT",
                      noteName(active), octave, active,
                      static_cast<unsigned>(keyboard_.heldCount()));
    } else if (miniAcid_.isPlaying() && drums && keyboard_.heldCount() > 0) {
        gfx.setTextColor(MusicVisuals::accentForStyle());
        std::snprintf(line, sizeof(line), "PLAYING | PAD ACTIVE | N60 | VEL:%d",
                      velocity);
    } else if (miniAcid_.isPlaying()) {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "PLAYING | %s | %s",
                      keyboard_.targetName(),
                      stepTools ? "LIVE SYNC" : (directPoly ? "POLY EXT" : "MONO EXT"));
    } else if (!drums && active >= 0) {
        const int octave = active / 12 - 1;
        gfx.setTextColor(MusicVisuals::accentForStyle());
        std::snprintf(line, sizeof(line), "%s NOTE %s%d | MIDI:%d | H:%u",
                      directPoly ? "POLY" : "MONO",
                      noteName(active), octave, active,
                      static_cast<unsigned>(keyboard_.heldCount()));
    } else if (drums && keyboard_.heldCount() > 0) {
        gfx.setTextColor(MusicVisuals::accentForStyle());
        std::snprintf(line, sizeof(line), "PAD ACTIVE | N60 | VEL:%d | LANES:7", velocity);
    } else if (drums) {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "READY | A/S/D/F/G/H/J | CH1..7");
    } else if (keyboard_.target() == MusicalEventTarget::Dx) {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "%s | TAB PERFORMANCE TOOLS",
                      directPoly ? "USB POLY" : "USB MONO");
    } else if (directPoly) {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "EXT POLY | TAB PERFORMANCE TOOLS");
    } else {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "EXT MONO | TAB PERFORMANCE TOOLS");
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), line);
}

void PerformPage::drawFooter(IGfx& gfx) {
    if (toolsLayerVisible_) {
        UI::drawStandardFooter(gfx,
                               "1-4 LEFT  5-8 RIGHT",
                               "9 VOICE | SHIFT REVERSE");
        return;
    }
    UI::drawStandardFooter(gfx,
                           "\\ Target  N Note  ,/. Scale",
                           "Tab Tools  -/= Oct  X Panic");
}
