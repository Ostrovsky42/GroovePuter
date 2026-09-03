#include "perform_page.h"

#include <cstdio>
#include <cstring>

#include "../components/music_visuals.h"
#include "../ui_input.h"
#include "src/midi/smf_player_service.h"

namespace {
constexpr const char* kToolContextNames[] = {
    "KEY", "CHORD", "ARP", "RHYTHM",
};
constexpr const char* kToolContextActiveNames[] = {
    "[KEY]", "[CHORD]", "[ARP]", "[RHYTHM]",
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
        case PerformanceToolContext::Key: return 6;
        case PerformanceToolContext::Chord: return 5;
        case PerformanceToolContext::Arp: return 4;
        case PerformanceToolContext::Rhythm: return 6;
        case PerformanceToolContext::Count: break;
    }
    return 1;
}

uint8_t PerformPage::currentRow() const {
    return selectedRow_[static_cast<int>(selectedContext_)];
}

void PerformPage::moveContext(int direction) {
    int next = static_cast<int>(selectedContext_) + (direction >= 0 ? 1 : -1);
    const int count = static_cast<int>(PerformanceToolContext::Count);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    selectedContext_ = static_cast<PerformanceToolContext>(next);
    // Each context remembers its own row so switching back lands where the
    // musician left off instead of jumping to the top.
}

void PerformPage::moveRow(int direction) {
    const int count = static_cast<int>(rowCountForContext());
    int next = static_cast<int>(currentRow()) + (direction >= 0 ? 1 : -1);
    while (next < 0) next += count;
    while (next >= count) next -= count;
    selectedRow_[static_cast<int>(selectedContext_)] = static_cast<uint8_t>(next);
}

void PerformPage::adjustSelectedValue(int direction) {
    if (direction == 0) return;
    const uint8_t row = currentRow();
    const bool chordActive = keyboard_.chordMode() != PerformanceChordMode::Off;
    const bool arpEnabled = keyboard_.arpeggiatorEnabled();

    switch (selectedContext_) {
        case PerformanceToolContext::Key:
            switch (row) {
                case 0: keyboard_.cycleRoot(direction); break;
                case 1: keyboard_.cycleScale(direction); break;
                case 2: keyboard_.shiftOctave(direction); break;
                case 3: keyboard_.adjustVelocity(direction); break;
                case 4: keyboard_.cycleTarget(direction); break;
                case 5:
                    if (keyboard_.target() != MusicalEventTarget::Drums) {
                        keyboard_.setVoiceMode(direction > 0
                            ? PerformanceVoiceMode::Poly
                            : PerformanceVoiceMode::Mono);
                    }
                    break;
                default: break;
            }
            return;

        case PerformanceToolContext::Chord:
            switch (row) {
                case 0:
                    keyboard_.cycleChordMode(direction);
                    break;
                case 1:
                    if (chordActive) keyboard_.cycleChordInversion(direction);
                    break;
                case 2:
                    if (chordActive) {
                        keyboard_.setChordSpread(direction > 0
                            ? PerformanceSpread::Wide : PerformanceSpread::Close);
                    }
                    break;
                case 3:
                    if (chordActive && !arpEnabled) {
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
            switch (row) {
                case 0: keyboard_.setArpeggiatorEnabled(direction > 0); break;
                case 1: if (arpEnabled) keyboard_.cycleArpDirection(direction); break;
                case 2: if (arpEnabled) keyboard_.cycleArpOctaves(direction); break;
                case 3: if (arpEnabled) keyboard_.setLatchEnabled(direction > 0); break;
                default: break;
            }
            return;

        case PerformanceToolContext::Rhythm:
            switch (row) {
                case 0: keyboard_.cycleArpRate(direction); break;
                case 1: keyboard_.cycleGate(direction); break;
                case 2: keyboard_.cycleRatchet(direction); break;
                case 3: keyboard_.cycleEuclideanPulses(direction); break;
                case 4:
                    if (rotationIsAudible(keyboard_)) keyboard_.rotateEuclidean(direction);
                    break;
                case 5:
                    if (strumIsAudible(keyboard_)) keyboard_.cycleStrum(direction);
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
    const uint8_t row = currentRow();
    const bool chordActive = keyboard_.chordMode() != PerformanceChordMode::Off;
    const bool arpEnabled = keyboard_.arpeggiatorEnabled();

    switch (selectedContext_) {
        case PerformanceToolContext::Key:
            if (row == 4) {
                keyboard_.cycleTarget(1);
            } else if (row == 5 && keyboard_.target() != MusicalEventTarget::Drums) {
                keyboard_.toggleVoiceMode();
            }
            return;

        case PerformanceToolContext::Chord:
            if (row == 2 && chordActive) {
                keyboard_.toggleChordSpread();
                return;
            }
            if (row == 3 && chordActive && !arpEnabled) {
                keyboard_.toggleVoiceLeading();
                return;
            }
            if (row == 4) {
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
            if (row == 0) {
                keyboard_.toggleArpeggiator();
            } else if (row == 3 && arpEnabled) {
                keyboard_.toggleLatch();
            }
            return;

        case PerformanceToolContext::Rhythm:
            if (row == 3) {
                keyboard_.cycleEuclideanLength(1);
            } else if (row == 5 && strumIsAudible(keyboard_)) {
                keyboard_.cycleStrumDirection(1);
            }
            return;

        case PerformanceToolContext::Count:
            return;
    }
}

const char* PerformPage::selectedRowHint() const {
    const bool chordActive = keyboard_.chordMode() != PerformanceChordMode::Off;
    const bool arpEnabled = keyboard_.arpeggiatorEnabled();
    const bool drums = keyboard_.target() == MusicalEventTarget::Drums;
    const uint8_t row = currentRow();

    switch (selectedContext_) {
        case PerformanceToolContext::Key:
            switch (row) {
                case 0: return "</> ROOT NOTE";
                case 1: return "</> SCALE";
                case 2: return "</> OCTAVE SHIFT -2..+2";
                case 3: return "</> VELOCITY 10..120";
                case 4: return "</> OUTPUT (STOPS SOUNDING NOTES)";
                case 5:
                    return drums ? "N/A: DRUMS ARE ALWAYS 7 LANES"
                                 : "</> OR ENTER: MONO/POLY RECEIVER";
                default: return "";
            }

        case PerformanceToolContext::Chord:
            switch (row) {
                case 0: return "</> CHORD MODE";
                case 1: return chordActive ? "</> INVERSION" : "N/A: SET CHORD MODE";
                case 2: return chordActive ? "</> OR ENTER: CLOSE/WIDE" : "N/A: SET CHORD MODE";
                case 3:
                    if (!chordActive) return "N/A: SET CHORD MODE";
                    if (arpEnabled) return "N/A: ARP PLAYS SINGLE NOTES";
                    return "</> OR ENTER: OFF/NEAREST";
                case 4:
                    if (keyboard_.heldCount() >= 2) return "ENTER CAPTURE HELD NOTES";
                    if (keyboard_.chordMemorySize() > 0) return "ENTER CLEAR MEMORY";
                    return "HOLD 2+ NOTES, THEN ENTER";
                default: return "";
            }

        case PerformanceToolContext::Arp:
            if (row == 0) return "</> OR ENTER: ON/OFF (CLEARS STRUM)";
            if (!arpEnabled) return "N/A: ENABLE ARP FIRST";
            switch (row) {
                case 1: return "</> NOTE ORDER";
                case 2: return "</> OCTAVE RANGE 1..4";
                case 3: return "</> OR ENTER: HOLD AFTER RELEASE";
                default: return "";
            }

        case PerformanceToolContext::Rhythm:
            switch (row) {
                case 0: return "</> STEP CLOCK, APPLIES NEXT STEP";
                case 1: return "</> NOTE LENGTH PER STEP";
                case 2: return "</> REPEATS PER STEP (CLEARS STRUM)";
                case 3: return "</> PULSES  ENTER LENGTH";
                case 4:
                    return rotationIsAudible(keyboard_)
                        ? "</> PATTERN ROTATION"
                        : "N/A: SET PULSES BETWEEN 1 AND LEN-1";
                case 5:
                    if (arpEnabled) return "N/A: ARP PLAYS SINGLE NOTES";
                    if (!chordActive) return "N/A: SET CHORD MODE";
                    return "</> STRUM MS  ENTER DIRECTION";
                default: return "";
            }

        case PerformanceToolContext::Count:
            break;
    }
    return "";
}

bool PerformPage::handleToolKey(const UIEvent& event) {
    switch (UIInput::navCode(event)) {
        case GROOVEPUTER_UP:
            moveRow(-1);
            return true;
        case GROOVEPUTER_DOWN:
            moveRow(1);
            return true;
        case GROOVEPUTER_LEFT:
            adjustSelectedValue(-1);
            return true;
        case GROOVEPUTER_RIGHT:
            adjustSelectedValue(1);
            return true;
        default:
            break;
    }

    switch (event.key) {
        case '\r':
        case '\n':
            toggleSelectedValue();
            return true;
        // The Cardputer arrow keycaps are the ; , . / keys. The firmware sends
        // the arrow scancode and the printed character as two events, so the
        // characters must be swallowed here or every arrow press would also
        // change the scale through the live PERFORM bindings.
        case ';': case ':':
        case ',': case '<':
        case '.': case '>':
        case '/': case '?':
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

void PerformPage::drawToolTabs(IGfx& gfx, int y) {
    // Fixed slots: "[KEY] [CHORD] [ARP] [RHYTHM]". The bracket cell to the
    // left of each label is always reserved, so labels never shift when the
    // active context changes.
    const UI::ThemePalette palette = UI::themePalette();
    const int charW = gfx.measureText("M");
    const int cellH = Layout::LINE_HEIGHT - 1;
    int x = Layout::COL_1;
    for (int i = 0; i < static_cast<int>(PerformanceToolContext::Count); ++i) {
        const char* name = kToolContextNames[i];
        const int len = static_cast<int>(std::strlen(name));
        const bool active = i == static_cast<int>(selectedContext_);
        if (active) {
            gfx.fillRect(x - 1, y - 1, (len + 2) * charW + 2, cellH,
                         MusicVisuals::accentForStyle());
            gfx.setTextColor(palette.invert);
            gfx.drawText(x, y, kToolContextActiveNames[i]);
        } else {
            gfx.setTextColor(palette.secondary);
            gfx.drawText(x + charW, y, name);
        }
        x += (len + 3) * charW;
    }

    char target[16];
    if (keyboard_.target() == MusicalEventTarget::Drums) {
        std::snprintf(target, sizeof(target), "%s",
                      compactTargetName(keyboard_.target()));
    } else {
        std::snprintf(target, sizeof(target), "%s %s",
                      compactTargetName(keyboard_.target()),
                      keyboard_.voiceModeName());
    }
    const int rightEdge = Layout::CONTENT.x + Layout::CONTENT.w - Layout::CONTENT_PAD_X;
    int targetX = rightEdge - gfx.measureText(target);
    if (targetX < x) targetX = x;
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(targetX, y, target);
}

void PerformPage::drawToolsLayer(IGfx& gfx) {
    // The display repaints the whole frame every update, so this surface is
    // always drawn in full. Partial repaint would leave labels blank after the
    // first frame.
    LayoutManager::clearContent(gfx);

    const UI::ThemePalette palette = UI::themePalette();
    const int labelX = Layout::COL_1 + 6;
    const int valueX = Layout::COL_2;
    const int rowX = Layout::CONTENT.x + 2;
    const int rowW = Layout::CONTENT.w - 4;
    const int rowH = Layout::LINE_HEIGHT - 2;
    const int valueW = Layout::CONTENT.x + Layout::CONTENT.w - Layout::CONTENT_PAD_X - valueX;
    const uint8_t selected = currentRow();
    const bool drums = keyboard_.target() == MusicalEventTarget::Drums;
    const bool chordActive = keyboard_.chordMode() != PerformanceChordMode::Off;
    const bool arpEnabled = keyboard_.arpeggiatorEnabled();
    char value[48];

    drawToolTabs(gfx, LayoutManager::lineY(0));

    auto drawRow = [&](uint8_t row, const char* label, const char* rowValue,
                       bool available) {
        const int y = LayoutManager::lineY(1 + row);
        const bool isSelected = row == selected;
        if (isSelected) {
            gfx.fillRect(rowX, y - 1, rowW, rowH, palette.focus);
            gfx.setTextColor(palette.invert);
            gfx.drawText(Layout::COL_1, y, ">");
            gfx.drawText(labelX, y, label);
            Widgets::drawClippedText(gfx, valueX, y, valueW, rowValue);
            return;
        }
        gfx.setTextColor(palette.secondary);
        gfx.drawText(labelX, y, label);
        gfx.setTextColor(available ? palette.text : palette.dim);
        Widgets::drawClippedText(gfx, valueX, y, valueW, rowValue);
    };

    switch (selectedContext_) {
        case PerformanceToolContext::Key:
            drawRow(0, "ROOT", keyboard_.rootName(), true);
            drawRow(1, "SCALE", keyboard_.scaleName(), true);
            std::snprintf(value, sizeof(value), "%+d",
                          static_cast<int>(keyboard_.octaveShift()));
            drawRow(2, "OCTAVE", value, true);
            std::snprintf(value, sizeof(value), "%u",
                          static_cast<unsigned>(keyboard_.velocity()));
            drawRow(3, "VELOCITY", value, true);
            if (drums) {
                std::snprintf(value, sizeof(value), "%s  CH1-7", keyboard_.targetName());
            } else {
                std::snprintf(value, sizeof(value), "%s  CH%u", keyboard_.targetName(),
                              static_cast<unsigned>(keyboard_.targetMidiChannel()));
            }
            drawRow(4, "OUTPUT", value, true);
            drawRow(5, "VOICE", drums ? "N/A" : keyboard_.voiceModeName(), !drums);
            break;

        case PerformanceToolContext::Chord: {
            drawRow(0, "MODE", keyboard_.chordModeName(), true);
            if (chordActive) {
                std::snprintf(value, sizeof(value), "%u",
                              static_cast<unsigned>(keyboard_.chordInversion()));
                drawRow(1, "INVERSION", value, true);
                drawRow(2, "SPREAD", keyboard_.chordSpreadName(), true);
            } else {
                drawRow(1, "INVERSION", "N/A", false);
                drawRow(2, "SPREAD", "N/A", false);
            }
            if (chordActive && !arpEnabled) {
                drawRow(3, "LEADING",
                        keyboard_.voiceLeading() == PerformanceVoiceLeading::Nearest
                            ? "NEAREST" : "OFF", true);
            } else {
                drawRow(3, "LEADING", "N/A", false);
            }
            if (keyboard_.chordMemorySize() == 0) {
                drawRow(4, "MEMORY", "EMPTY", true);
            } else {
                std::snprintf(value, sizeof(value), "%u NOTES",
                              static_cast<unsigned>(keyboard_.chordMemorySize()));
                drawRow(4, "MEMORY", value, true);
            }
            break;
        }

        case PerformanceToolContext::Arp: {
            const bool latched = arpEnabled && keyboard_.latchEnabled();
            drawRow(0, "ARP", !arpEnabled ? "OFF" : (latched ? "LATCHED" : "ON"), true);
            if (!arpEnabled) {
                drawRow(1, "ORDER", "N/A", false);
                drawRow(2, "OCTAVES", "N/A", false);
                drawRow(3, "LATCH", "N/A", false);
                break;
            }
            drawRow(1, "ORDER", arpOrderLabel(keyboard_.arpDirection()), true);
            std::snprintf(value, sizeof(value), "%u",
                          static_cast<unsigned>(keyboard_.arpOctaves()));
            drawRow(2, "OCTAVES", value, true);
            drawRow(3, "LATCH", keyboard_.latchEnabled() ? "ON" : "OFF", true);
            break;
        }

        case PerformanceToolContext::Rhythm:
            // RATE and GATE drive the shared step clock used by ARP, RATCHET and
            // EUCLID alike, so they live here and stay editable with ARP off.
            if (keyboard_.activeRate() != keyboard_.arpRate()) {
                std::snprintf(value, sizeof(value), "%s NEXT",
                              keyboard_.arpRateName());
                drawRow(0, "RATE", value, true);
            } else {
                drawRow(0, "RATE", keyboard_.arpRateName(), true);
            }
            std::snprintf(value, sizeof(value), "%u%%",
                          static_cast<unsigned>(keyboard_.gatePercent()));
            drawRow(1, "GATE", value, true);
            if (keyboard_.ratchetCount() <= 1) {
                drawRow(2, "RATCHET", "OFF", true);
            } else {
                std::snprintf(value, sizeof(value), "x%u",
                              static_cast<unsigned>(keyboard_.ratchetCount()));
                drawRow(2, "RATCHET", value, true);
            }
            if (keyboard_.euclideanPulses() == 0) {
                std::snprintf(value, sizeof(value), "OFF  LEN %u",
                              static_cast<unsigned>(keyboard_.euclideanLength()));
            } else {
                std::snprintf(value, sizeof(value), "%u/%u",
                              static_cast<unsigned>(keyboard_.euclideanPulses()),
                              static_cast<unsigned>(keyboard_.euclideanLength()));
            }
            drawRow(3, "EUCLID", value, true);
            if (rotationIsAudible(keyboard_)) {
                std::snprintf(value, sizeof(value), "%u",
                              static_cast<unsigned>(keyboard_.euclideanRotation()));
                drawRow(4, "ROTATE", value, true);
            } else {
                drawRow(4, "ROTATE", "N/A", false);
            }
            if (!strumIsAudible(keyboard_)) {
                drawRow(5, "STRUM", "N/A", false);
            } else if (keyboard_.strumMs() == 0) {
                drawRow(5, "STRUM", "OFF", true);
            } else {
                std::snprintf(value, sizeof(value), "%s %ums",
                              strumDirectionLabel(keyboard_.strumDirection()),
                              static_cast<unsigned>(keyboard_.strumMs()));
                drawRow(5, "STRUM", value, true);
            }
            break;

        case PerformanceToolContext::Count:
            break;
    }

    // The global performance HUD strip is composited over the bottom of the
    // content zone, so the hint sits two pixels above the nominal line 7.
    gfx.setTextColor(COLOR_LABEL);
    Widgets::drawClippedText(gfx, Layout::COL_1, LayoutManager::lineY(7) - 2,
                             Layout::CONTENT.w - 2 * Layout::CONTENT_PAD_X,
                             selectedRowHint());
}

bool PerformPage::handleEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
    // The Cardputer keycaps label the arrows as Fn alternates, so arrows are
    // accepted with Fn (meta) held while the tools layer is open. Every other
    // modifier combination passes through to the global shortcuts.
    const bool toolsArrow = toolsLayerVisible_ && !event.ctrl && !event.alt &&
                            UIInput::navCode(event) != 0;
    if ((event.ctrl || event.alt || event.meta) && !toolsArrow) return false;

    const bool tabPressed =
        event.key == '\t' || event.scancode == GROOVEPUTER_TAB;
    if (tabPressed) {
        if (!toolsLayerVisible_) {
            // Reopen where the musician left off; the tab bar itself explains
            // the contexts, so no toast is needed over the fresh surface.
            toolsLayerVisible_ = true;
        } else {
            // The Cardputer has no Shift key: Tab cycles forward, Backspace
            // steps back. Shift+Tab stays as a desktop-emulator alias only.
            moveContext(event.shift ? -1 : 1);
        }
        return true;
    }

    if (toolsLayerVisible_ && event.key == '\b') {
        moveContext(-1);
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
    keyboard_.setTempoBpm(miniAcid_.bpm());

    if (toolsLayerVisible_) {
        drawToolsLayer(gfx);
        return;
    }

    LayoutManager::clearContent(gfx);

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
                               "Tab/Bksp Context  ^/v Row  </> Value",
                               "Enter Action  Esc Live");
        return;
    }
    UI::drawStandardFooter(gfx,
                           "\\ Target  N Note  ,/. Scale",
                           "Tab Tools  -/= Oct  X Panic");
}
