#include "perform_page.h"

#include <cstdio>
#include <cstring>

#include "../components/music_visuals.h"
#include "src/midi/smf_player_service.h"

namespace {
constexpr const char* kToolContextNames[] = {
    "KEY", "CHORD", "ARP", "RHYTHM",
};

bool strumIsAudible(const PerformanceKeyboard& keyboard) {
    return !keyboard.arpeggiatorEnabled() &&
           keyboard.chordMode() != PerformanceChordMode::Off;
}

bool rotationIsAudible(const PerformanceKeyboard& keyboard) {
    const uint8_t pulses = keyboard.euclideanPulses();
    const uint8_t length = keyboard.euclideanLength();
    return pulses > 0 && pulses < length;
}

const char* compactTargetName(MusicalEventTarget target) {
    switch (target) {
        case MusicalEventTarget::SynthA: return "SYN A";
        case MusicalEventTarget::SynthB: return "SYN B";
        case MusicalEventTarget::Dx: return "DX";
        case MusicalEventTarget::Drums: return "DRUMS";
    }
    return "?";
}

const char* arpOrderLabel(PerformanceArpDirection direction) {
    switch (direction) {
        case PerformanceArpDirection::Up: return "UP";
        case PerformanceArpDirection::Down: return "DOWN";
        case PerformanceArpDirection::UpDown: return "UPDOWN";
        case PerformanceArpDirection::DownUp: return "DOWNUP";
        case PerformanceArpDirection::AsPlayed: return "AS PLAYED";
        case PerformanceArpDirection::Random: return "RANDOM";
        case PerformanceArpDirection::Count: break;
    }
    return "UP";
}

const char* strumDirectionLabel(PerformanceStrumDirection direction) {
    switch (direction) {
        case PerformanceStrumDirection::LowToHigh: return "UP";
        case PerformanceStrumDirection::HighToLow: return "DOWN";
        case PerformanceStrumDirection::AsPlayed: return "AS PLAYED";
        case PerformanceStrumDirection::Count: break;
    }
    return "UP";
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

uint8_t PerformPage::rowCountForContext() const {
    switch (selectedContext_) {
        case PerformanceToolContext::Key: return 4;
        case PerformanceToolContext::Chord: return 5;
        case PerformanceToolContext::Arp: return 6;
        case PerformanceToolContext::Rhythm: return 4;
        case PerformanceToolContext::Count: break;
    }
    return 1;
}

void PerformPage::moveContext(int direction) {
    int next = static_cast<int>(selectedContext_) + (direction >= 0 ? 1 : -1);
    const int count = static_cast<int>(PerformanceToolContext::Count);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    selectedContext_ = static_cast<PerformanceToolContext>(next);
    selectedRow_ = 0;
}

void PerformPage::moveRow(int direction) {
    const int count = static_cast<int>(rowCountForContext());
    int next = static_cast<int>(selectedRow_) + (direction >= 0 ? 1 : -1);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    selectedRow_ = static_cast<uint8_t>(next);
}

void PerformPage::adjustSelectedValue(int direction) {
    if (direction == 0) return;

    switch (selectedContext_) {
        case PerformanceToolContext::Key:
            switch (selectedRow_) {
                case 0: keyboard_.cycleRoot(direction); break;
                case 1: keyboard_.cycleScale(direction); break;
                case 2: keyboard_.shiftOctave(direction); break;
                case 3: keyboard_.adjustVelocity(direction); break;
                default: break;
            }
            return;

        case PerformanceToolContext::Chord:
            switch (selectedRow_) {
                case 0:
                    keyboard_.cycleChordMode(direction);
                    break;
                case 1:
                    if (keyboard_.chordMode() != PerformanceChordMode::Off) {
                        keyboard_.cycleChordInversion(direction);
                    }
                    break;
                case 2:
                    if (keyboard_.chordMode() != PerformanceChordMode::Off) {
                        keyboard_.setChordSpread(direction > 0
                            ? PerformanceSpread::Wide : PerformanceSpread::Close);
                    }
                    break;
                case 3:
                    if (keyboard_.chordMode() != PerformanceChordMode::Off &&
                        !keyboard_.arpeggiatorEnabled()) {
                        keyboard_.setVoiceLeading(direction > 0
                            ? PerformanceVoiceLeading::Nearest
                            : PerformanceVoiceLeading::Off);
                    }
                    break;
                default:
                    break;
            }
            return;

        case PerformanceToolContext::Arp:
            switch (selectedRow_) {
                case 0:
                    keyboard_.setArpeggiatorEnabled(direction > 0);
                    break;
                case 1:
                    if (keyboard_.arpeggiatorEnabled()) {
                        keyboard_.cycleArpDirection(direction);
                    }
                    break;
                case 2:
                    if (keyboard_.arpeggiatorEnabled()) {
                        keyboard_.cycleArpRate(direction);
                    }
                    break;
                case 3:
                    if (keyboard_.arpeggiatorEnabled()) {
                        keyboard_.cycleArpOctaves(direction);
                    }
                    break;
                case 4:
                    if (keyboard_.arpeggiatorEnabled()) {
                        keyboard_.cycleGate(direction);
                    }
                    break;
                case 5:
                    if (keyboard_.arpeggiatorEnabled()) {
                        keyboard_.setLatchEnabled(direction > 0);
                    }
                    break;
                default:
                    break;
            }
            return;

        case PerformanceToolContext::Rhythm:
            switch (selectedRow_) {
                case 0:
                    keyboard_.cycleRatchet(direction);
                    break;
                case 1:
                    keyboard_.cycleEuclideanPulses(direction);
                    break;
                case 2:
                    if (rotationIsAudible(keyboard_)) {
                        keyboard_.rotateEuclidean(direction);
                    }
                    break;
                case 3:
                    if (strumIsAudible(keyboard_)) {
                        keyboard_.cycleStrum(direction);
                    }
                    break;
                default:
                    break;
            }
            return;

        case PerformanceToolContext::Count:
            return;
    }
}

void PerformPage::toggleSelectedValue() {
    char toast[64];

    switch (selectedContext_) {
        case PerformanceToolContext::Key:
            return;

        case PerformanceToolContext::Chord:
            if (selectedRow_ == 2 &&
                keyboard_.chordMode() != PerformanceChordMode::Off) {
                keyboard_.toggleChordSpread();
                return;
            }
            if (selectedRow_ == 3 &&
                keyboard_.chordMode() != PerformanceChordMode::Off &&
                !keyboard_.arpeggiatorEnabled()) {
                keyboard_.toggleVoiceLeading();
                return;
            }
            if (selectedRow_ == 4) {
                if (keyboard_.heldCount() >= 2 && keyboard_.captureChordMemory()) {
                    std::snprintf(toast, sizeof(toast), "MEMORY: %u NOTES",
                                  static_cast<unsigned>(keyboard_.chordMemorySize()));
                } else if (keyboard_.chordMemorySize() > 0) {
                    keyboard_.clearChordMemory();
                    std::snprintf(toast, sizeof(toast), "MEMORY: CLEARED");
                } else {
                    std::snprintf(toast, sizeof(toast), "MEMORY: HOLD 2+ NOTES");
                }
                UI::showToast(toast, 900);
            }
            return;

        case PerformanceToolContext::Arp:
            if (selectedRow_ == 0) {
                keyboard_.toggleArpeggiator();
            } else if (selectedRow_ == 5 && keyboard_.arpeggiatorEnabled()) {
                keyboard_.toggleLatch();
            }
            return;

        case PerformanceToolContext::Rhythm:
            if (selectedRow_ == 1) {
                keyboard_.cycleEuclideanLength(1);
            } else if (selectedRow_ == 3 && strumIsAudible(keyboard_)) {
                keyboard_.cycleStrumDirection(1);
            }
            return;

        case PerformanceToolContext::Count:
            return;
    }
}

bool PerformPage::handleToolKey(const UIEvent& event) {
    switch (event.scancode) {
        case GROOVEPUTER_LEFT:
            moveContext(-1);
            return true;
        case GROOVEPUTER_RIGHT:
            moveContext(1);
            return true;
        case GROOVEPUTER_UP:
            moveRow(-1);
            return true;
        case GROOVEPUTER_DOWN:
            moveRow(1);
            return true;
        default:
            break;
    }

    switch (event.key) {
        case '-':
        case '_':
            adjustSelectedValue(-1);
            return true;
        case '=':
        case '+':
            adjustSelectedValue(1);
            return true;
        case '\r':
        case '\n':
            toggleSelectedValue();
            return true;
        case '9': {
            // Compatibility shortcut. Voice-mode transition semantics, including
            // cleanup, remain entirely owned by PerformanceKeyboard.
            keyboard_.toggleVoiceMode();
            char toast[48];
            std::snprintf(toast, sizeof(toast), "VOICE: %s / RECEIVER",
                          keyboard_.voiceModeName());
            UI::showToast(toast, 900);
            return true;
        }
        default:
            return false;
    }
}

void PerformPage::drawToolsLayer(IGfx& gfx) {
    const int labelX = Layout::COL_1 + 8;
    const int valueX = Layout::COL_2;
    char line[72];
    char value[48];

    if (keyboard_.target() == MusicalEventTarget::Drums) {
        std::snprintf(line, sizeof(line), "%s  CH1-7",
                      compactTargetName(keyboard_.target()));
    } else {
        std::snprintf(line, sizeof(line), "%s  %s  CH%u",
                      compactTargetName(keyboard_.target()),
                      keyboard_.voiceModeName(),
                      static_cast<unsigned>(keyboard_.targetMidiChannel()));
    }
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

    const uint8_t contextIndex = static_cast<uint8_t>(selectedContext_);
    switch (selectedContext_) {
        case PerformanceToolContext::Key:
            std::snprintf(line, sizeof(line), "[KEY] CHORD ARP RHYTHM");
            break;
        case PerformanceToolContext::Chord:
            std::snprintf(line, sizeof(line), "KEY [CHORD] ARP RHYTHM");
            break;
        case PerformanceToolContext::Arp:
            std::snprintf(line, sizeof(line), "KEY CHORD [ARP] RHYTHM");
            break;
        case PerformanceToolContext::Rhythm:
            std::snprintf(line, sizeof(line), "KEY CHORD ARP [RHYTHM]");
            break;
        case PerformanceToolContext::Count:
            std::snprintf(line, sizeof(line), "KEY CHORD ARP RHYTHM");
            break;
    }
    (void)contextIndex;
    (void)kToolContextNames;
    gfx.setTextColor(COLOR_ACCENT);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    auto drawRow = [&](uint8_t row, const char* label, const char* rowValue) {
        const int y = LayoutManager::lineY(2 + row);
        const bool selected = row == selectedRow_;
        gfx.setTextColor(selected ? COLOR_ACCENT : COLOR_LABEL);
        gfx.drawText(Layout::COL_1, y, selected ? ">" : " ");
        gfx.drawText(labelX, y, label);
        gfx.setTextColor(selected ? COLOR_WHITE : COLOR_LABEL);
        gfx.drawText(valueX, y, rowValue);
    };

    switch (selectedContext_) {
        case PerformanceToolContext::Key:
            drawRow(0, "ROOT", keyboard_.rootName());
            drawRow(1, "SCALE", keyboard_.scaleName());
            std::snprintf(value, sizeof(value), "%+d",
                          static_cast<int>(keyboard_.octaveShift()));
            drawRow(2, "OCTAVE", value);
            std::snprintf(value, sizeof(value), "%u",
                          static_cast<unsigned>(keyboard_.velocity()));
            drawRow(3, "VELOCITY", value);
            break;

        case PerformanceToolContext::Chord: {
            const bool chordActive =
                keyboard_.chordMode() != PerformanceChordMode::Off;
            drawRow(0, "MODE", keyboard_.chordModeName());
            if (chordActive) {
                std::snprintf(value, sizeof(value), "%u",
                              static_cast<unsigned>(keyboard_.chordInversion()));
                drawRow(1, "INVERSION", value);
                drawRow(2, "SPREAD", keyboard_.chordSpreadName());
            } else {
                drawRow(1, "INVERSION", "N/A");
                drawRow(2, "SPREAD", "N/A");
            }
            if (chordActive && !keyboard_.arpeggiatorEnabled()) {
                drawRow(3, "LEADING",
                        keyboard_.voiceLeading() == PerformanceVoiceLeading::Nearest
                            ? "NEAREST" : "OFF");
            } else {
                drawRow(3, "LEADING", "N/A");
            }
            if (keyboard_.chordMemorySize() == 0) {
                drawRow(4, "MEMORY", "EMPTY");
            } else {
                std::snprintf(value, sizeof(value), "%u NOTES",
                              static_cast<unsigned>(keyboard_.chordMemorySize()));
                drawRow(4, "MEMORY", value);
            }
            break;
        }

        case PerformanceToolContext::Arp: {
            const bool arpEnabled = keyboard_.arpeggiatorEnabled();
            const bool latched = arpEnabled && keyboard_.latchEnabled();
            drawRow(0, "ARP", !arpEnabled ? "OFF" : (latched ? "LATCHED" : "ON"));
            if (!arpEnabled) {
                drawRow(1, "ORDER", "N/A");
                drawRow(2, "RATE", "N/A");
                drawRow(3, "OCTAVES", "N/A");
                drawRow(4, "GATE", "N/A");
                drawRow(5, "LATCH", "N/A");
                break;
            }
            drawRow(1, "ORDER", arpOrderLabel(keyboard_.arpDirection()));
            if (keyboard_.activeRate() != keyboard_.arpRate()) {
                std::snprintf(value, sizeof(value), "%s NEXT",
                              keyboard_.arpRateName());
                drawRow(2, "RATE", value);
            } else {
                drawRow(2, "RATE", keyboard_.arpRateName());
            }
            std::snprintf(value, sizeof(value), "%u",
                          static_cast<unsigned>(keyboard_.arpOctaves()));
            drawRow(3, "OCTAVES", value);
            std::snprintf(value, sizeof(value), "%u%%",
                          static_cast<unsigned>(keyboard_.gatePercent()));
            drawRow(4, "GATE", value);
            drawRow(5, "LATCH", keyboard_.latchEnabled() ? "ON" : "OFF");
            break;
        }

        case PerformanceToolContext::Rhythm:
            std::snprintf(value, sizeof(value), "x%u",
                          static_cast<unsigned>(keyboard_.ratchetCount()));
            drawRow(0, "RATCHET", value);
            std::snprintf(value, sizeof(value), "%u/%u",
                          static_cast<unsigned>(keyboard_.euclideanPulses()),
                          static_cast<unsigned>(keyboard_.euclideanLength()));
            drawRow(1, "EUCLID", value);
            if (rotationIsAudible(keyboard_)) {
                std::snprintf(value, sizeof(value), "%u",
                              static_cast<unsigned>(keyboard_.euclideanRotation()));
                drawRow(2, "ROTATE", value);
            } else {
                drawRow(2, "ROTATE", "N/A");
            }
            if (strumIsAudible(keyboard_)) {
                std::snprintf(value, sizeof(value), "%s %ums",
                              strumDirectionLabel(keyboard_.strumDirection()),
                              static_cast<unsigned>(keyboard_.strumMs()));
                drawRow(3, "STRUM", value);
            } else {
                drawRow(3, "STRUM", "N/A");
            }
            break;

        case PerformanceToolContext::Count:
            break;
    }
}

bool PerformPage::handleEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN ||
        event.ctrl || event.alt || event.meta) {
        return false;
    }

    const bool tabPressed =
        event.key == '\t' || event.scancode == GROOVEPUTER_TAB;
    if (tabPressed) {
        if (!toolsLayerVisible_) {
            toolsLayerVisible_ = true;
            selectedContext_ = PerformanceToolContext::Key;
            selectedRow_ = 0;
            UI::showToast("PERFORM: KEY / CHORD / ARP / RHYTHM", 800);
        } else {
            moveContext(1);
        }
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

    if (toolsLayerVisible_) {
        drawToolsLayer(gfx);
        return;
    }

    const int active = keyboard_.activeNote();
    const int activeVelocity = keyboard_.activeVelocity();
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
        std::snprintf(line, sizeof(line), "NATIVE 7-LANE HELD:%u VEL:%u",
                      static_cast<unsigned>(keyboard_.heldCount()),
                      static_cast<unsigned>(keyboard_.velocity()));
    } else {
        std::snprintf(line, sizeof(line), "%s O%+d C:%s A:%s R%u E%u S%u V%u",
                      keyboard_.scaleName(),
                      static_cast<int>(keyboard_.octaveShift()),
                      keyboard_.chordModeName(),
                      keyboard_.arpeggiatorEnabled()
                          ? keyboard_.arpDirectionName() : "OFF",
                      static_cast<unsigned>(keyboard_.ratchetCount()),
                      static_cast<unsigned>(keyboard_.euclideanPulses()),
                      static_cast<unsigned>(keyboard_.strumMs()),
                      static_cast<unsigned>(keyboard_.velocity()));
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
    } else if (miniAcid_.isPlaying() && !drums && active >= 0) {
        const int octave = active / 12 - 1;
        gfx.setTextColor(MusicVisuals::accentForStyle());
        std::snprintf(line, sizeof(line), "%s | NOTE %s%d | MIDI:%d | H:%u V:%d",
                      directPoly ? "POLY EXT" : "MONO EXT",
                      noteName(active), octave, active,
                      static_cast<unsigned>(keyboard_.heldCount()), activeVelocity);
    } else if (miniAcid_.isPlaying() && drums && keyboard_.heldCount() > 0) {
        gfx.setTextColor(MusicVisuals::accentForStyle());
        std::snprintf(line, sizeof(line), "PLAYING | PAD ACTIVE | N60 | VEL:%d",
                      activeVelocity);
    } else if (miniAcid_.isPlaying()) {
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(line, sizeof(line), "PLAYING | %s | %s",
                      keyboard_.targetName(),
                      stepTools ? "LIVE SYNC" : (directPoly ? "POLY EXT" : "MONO EXT"));
    } else if (!drums && active >= 0) {
        const int octave = active / 12 - 1;
        gfx.setTextColor(MusicVisuals::accentForStyle());
        std::snprintf(line, sizeof(line), "%s NOTE %s%d | MIDI:%d | H:%u V:%d",
                      directPoly ? "POLY" : "MONO",
                      noteName(active), octave, active,
                      static_cast<unsigned>(keyboard_.heldCount()), activeVelocity);
    } else if (drums && keyboard_.heldCount() > 0) {
        gfx.setTextColor(MusicVisuals::accentForStyle());
        std::snprintf(line, sizeof(line), "PAD ACTIVE | N60 | VEL:%d | LANES:7",
                      activeVelocity);
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
                               "TAB/LR CONTEXT  UD ROW",
                               "-/+ VALUE  ENTER ALT  9 VOICE");
        return;
    }
    UI::drawStandardFooter(gfx,
                           "\\ Target  N Note  ,/. Scale",
                           "Tab Tools  -/= Oct  X Panic");
}
