#include "perform_page.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#include "../components/music_visuals.h"
#include "../ui_input.h"
#include "../perform_key_header.h"
#include "src/midi/smf_player_service.h"
#include "src/output/output_mode_runtime.h"
#include "src/output/output_ownership.h"
#include "src/state/scene_revision.h"
#include "src/platform/cardputer_midi_settings_session.h"
#include "src/platform/cardputer_usb_role_runtime.h"
#include "../midi_input_ui.h"
#include "src/input/call_response_listener.h"
#include "src/input/looper.h"
#include "src/input/looper_melody_export.h"
#include "src/state/generation_shape_state.h"
#include "synth_sequencer_page.h"
#include "../undo_ux.h"

namespace {
constexpr const char* kToolContextNames[] = {
    "KEY", "CHORD", "ARP", "RHYTHM", "INPUT", "LOOP",
};
constexpr const char* kToolContextActiveNames[] = {
    "[KEY]", "[CHORD]", "[ARP]", "[RHYTHM]", "[INPUT]", "[LOOP]",
};
// Same order as the keyboard's chord cycle; labels follow it one to one.
constexpr const PerformanceChordMode* kChordPopupModes = PerformanceKeyboard::kChordDisplayOrder;
constexpr std::size_t kChordPopupCount = PerformanceKeyboard::kChordDisplayCount;
constexpr const char* kChordPopupLabels[] = {
    "OFF", "SCALE3", "SCALE7", "5TH", "MIN", "MAJ",
    "MIN7", "MEM", "SUS2", "SUS4", "7", "MAJ7",
};
static_assert(sizeof(kChordPopupLabels) / sizeof(kChordPopupLabels[0]) == kChordPopupCount,
              "chord popup labels follow the display order");

const char* roleName(GroovePuterPerformance::PerformanceRole role) {
    switch (role) {
        case GroovePuterPerformance::PerformanceRole::Raw: return "RAW";
        case GroovePuterPerformance::PerformanceRole::Bass: return "BASS";
        case GroovePuterPerformance::PerformanceRole::Lead: return "LEAD";
        case GroovePuterPerformance::PerformanceRole::Chord: return "CHORD";
        case GroovePuterPerformance::PerformanceRole::Arp: return "ARP";
        case GroovePuterPerformance::PerformanceRole::Count: break;
    }
    return "RAW";
}

const char* leadModeName(GroovePuterPerformance::LeadMode mode) {
    switch (mode) {
        case GroovePuterPerformance::LeadMode::Raw: return "RAW";
        case GroovePuterPerformance::LeadMode::Scale: return "SCALE";
        case GroovePuterPerformance::LeadMode::Chord: return "CHORD";
        case GroovePuterPerformance::LeadMode::VoiceLead: return "VOICE";
        case GroovePuterPerformance::LeadMode::Count: break;
    }
    return "RAW";
}

const char* arpSourceName(GroovePuterPerformance::ArpPitchSource source) {
    switch (source) {
        case GroovePuterPerformance::ArpPitchSource::Held: return "HELD";
        case GroovePuterPerformance::ArpPitchSource::Memory: return "MEMORY";
        case GroovePuterPerformance::ArpPitchSource::Harmony: return "HARMONY";
        case GroovePuterPerformance::ArpPitchSource::Role: return "ROLE";
        case GroovePuterPerformance::ArpPitchSource::Count: break;
    }
    return "HELD";
}

const char* harmonyPolicyName(GroovePuterPerformance::HarmonyChangePolicy policy) {
    switch (policy) {
        case GroovePuterPerformance::HarmonyChangePolicy::Follow: return "FOLLOW";
        case GroovePuterPerformance::HarmonyChangePolicy::Hold: return "HOLD";
        case GroovePuterPerformance::HarmonyChangePolicy::Restart: return "RESTART";
        case GroovePuterPerformance::HarmonyChangePolicy::Count: break;
    }
    return "FOLLOW";
}

bool strumIsAudible(const PerformanceKeyboard& keyboard) {
    return !keyboard.arpeggiatorEnabled() &&
           keyboard.chordMode() != PerformanceChordMode::Off;
}

// The live scale binding (bare LEFT/RIGHT, and , .) already changes the scale;
// the toast names the scale just selected. Keys are not changed here.
void cycleScaleWithToast(PerformanceKeyboard& keyboard, int direction) {
    keyboard.cycleScale(direction);
    char toast[32];
    std::snprintf(toast, sizeof(toast), "SCALE: %s", keyboard.scaleName());
    UI::showToast(toast, 800);
}

bool rotationIsAudible(const PerformanceKeyboard& keyboard) {
    const uint8_t pulses = keyboard.euclideanPulses();
    const uint8_t length = keyboard.euclideanLength();
    return pulses > 0 && pulses < length;
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

using GroovePuterLooper::Looper;
using InRow = PerformInputRow;
constexpr uint8_t ir(InRow row) { return performInputRow(row); }

using LoopRow = PerformLoopRow;
constexpr uint8_t lr(LoopRow row) { return performLoopRow(row); }

// INPUT screen line of a focusable row: one heading line above each group.
constexpr uint8_t inputLine(uint8_t row) {
    return static_cast<uint8_t>(row + 1u + (row >= PerformPage::kLoopSetupRowFirst ? 1u : 0u) +
                                (row >= PerformPage::kPartnerRowFirst ? 1u : 0u));
}
constexpr uint8_t kInputLineCount = static_cast<uint8_t>(inputLine(PerformPage::kInputRowCount - 1u) + 1u);

using PartnerMode = GroovePuterCallResponse::CallResponseListener::Mode;
using PartnerReplace = GroovePuterCallResponse::CallResponseListener::LoopReplace;

// The PARTNER's repeating mode is Mode::Loop inside; the musician sees RIFF
// (LOOP is the separate LOOPER below it).
const char* helperModeName(const GroovePuterCallResponse::CallResponseListener& partner) {
    return partner.mode() == PartnerMode::Loop ? "RIFF" : partner.modeName();
}

const char* riffVariationName(uint8_t evolve) {
    switch (evolve) {
        case 0: return "EXACT";
        case 1: return "ENDING";
        case 2: return "DEVELOP";
        default: return "FREE";
    }
}

const char* newRiffIfName(PartnerReplace rule) {
    switch (rule) {
        case PartnerReplace::Any: return "1 NOTE";
        case PartnerReplace::Half: return "HALF";
        default: return "3 NOTES";
    }
}

// Toast for a refused or finished LOOPER action; done = text for Result::Done.
void showLooperToast(Looper::Result result, const char* done) {
    const char* text = done;
    switch (result) {
        case Looper::Result::Done: break;
        case Looper::Result::HoldLocked: text = "LOOP: HOLD, B TO RELEASE"; break;
        case Looper::Result::PartnerActive: text = "LOOP: HELPER OFF FIRST"; break;
        case Looper::Result::Empty: text = "LOOP: EMPTY, Z TO REC"; break;
        case Looper::Result::NothingToUndo: text = "LOOP: NOTHING TO UNDO"; break;
        case Looper::Result::NothingToHold: text = "LOOP: NOTHING TO HOLD"; break;
        case Looper::Result::Recording: text = "LOOP: AFTER THE REC PASS"; break;
        case Looper::Result::NoRoom: text = "LOOP: NO ROOM TO REPEAT"; break;
        case Looper::Result::Ignored: text = nullptr; break;
    }
    if (text) UI::showToast(text, 1000);
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

void PerformPage::openChordModePopup() {
    uint8_t selected = 0;
    for (std::size_t i = 0; i < kChordPopupCount; ++i) {
        if (kChordPopupModes[i] == keyboard_.chordMode()) { selected = static_cast<uint8_t>(i); break; }
    }
    chordModePopup_.open("CHORD MODE", static_cast<uint8_t>(kChordPopupCount), selected);
}

void PerformPage::applyChordModePopup() {
    const uint8_t selected = chordModePopup_.selectedIndex();
    if (selected >= kChordPopupCount) return;
    keyboard_.setChordMode(kChordPopupModes[selected]);
    char toast[32];
    std::snprintf(toast, sizeof(toast), "CHORD: %s", kChordPopupLabels[selected]);
    UI::showToast(toast, 900);
}

PerformPage::PerformPage(IGfx& gfx,
                         MiniAcid& miniAcid,
                         PerformanceKeyboard& keyboard,
                         AudioGuard audioGuard)
    : miniAcid_(miniAcid), keyboard_(keyboard), audioGuard_(audioGuard) {
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
        case PerformanceToolContext::Chord: return 6;
        case PerformanceToolContext::Arp: return 6;
        case PerformanceToolContext::Rhythm: return 6;
        // PerformInputRow: MIDI IN, LOOPER and its settings, PARTNER (HELPER and
        // its settings; phase 10: GENRE and LENGTH stay hidden at AUTO and
        // 100 %, see docs/design/CALL_RESPONSE_PHASE10_MENU.md).
        // The LOOPER is a separate object next to the PARTNER; the two share
        // one ResponsePlayer and never play together. The group headings are
        // drawn only and are not rows.
        case PerformanceToolContext::Input: return kInputRowCount;
        // The looper in six rows: one screen, no scroll, TO MELODY always visible.
        case PerformanceToolContext::Loop: return kLoopRowCount;
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

void PerformPage::cycleOutput(int direction) {
    GroovePuterOutput::Track track = GroovePuterOutput::Track::SynthA;
    if (!GroovePuterOutput::trackForTarget(keyboard_.target(), track)) {
        UI::showToast("DX: MIDI ONLY", 800);
        return;
    }

    const GroovePuterOutput::Mode current = GroovePuterOutput::mode(track);
    GroovePuterOutput::Mode next;
    if (direction >= 0) {
        next = GroovePuterOutput::cycleMode(current);
    } else {
        if (current == GroovePuterOutput::Mode::Internal) {
            next = GroovePuterOutput::Mode::Layer;
        } else if (current == GroovePuterOutput::Mode::Layer) {
            next = GroovePuterOutput::Mode::Midi;
        } else {
            next = GroovePuterOutput::Mode::Internal;
        }
    }

    // Release sounding notes on previous output route before applying updated route
    keyboard_.panic();

    const bool changed =
        GroovePuterOutput::applyModeWithLocalCleanup(miniAcid_, track, next);
    if (changed) {
        GroovePuterState::markSceneMutated();
    }

    char toast[48];
    std::snprintf(toast, sizeof(toast), "%s OUT: %s",
                  keyboard_.targetName(),
                  GroovePuterOutput::modeName(next));
    UI::showToast(toast, 1000);
}

void PerformPage::adjustInput(int direction) {
    namespace InputUi = GroovePuterUi::MidiInputUi;
    const auto current = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
    auto next = current;
    switch (currentRow()) {
        case ir(InRow::MidiIn): next = InputUi::stepEnabled(current); break;
        case ir(InRow::ListenChannel): next = InputUi::stepChannel(current, direction); break;
        case ir(InRow::SendTo): next = InputUi::stepTarget(current, direction); break;
        default: return;
    }
    if (!GroovePuterPlatform::setCardputerMidiInputRoutingConfig(next))
        UI::showToast("MIDI input save failed", 1400);
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
                case 5: cycleOutput(direction); break;
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
                case 5:
                    if (keyboard_.target() != MusicalEventTarget::Drums) {
                        keyboard_.setVoiceMode(direction > 0
                            ? PerformanceVoiceMode::Poly
                            : PerformanceVoiceMode::Mono);
                    }
                    break;
                default:
                    break;
            }
            return;

        case PerformanceToolContext::Arp:
            switch (row) {
                case 0: keyboard_.setArpeggiatorEnabled(direction > 0); break;
                case 1: keyboard_.setArpPitchSource(static_cast<GroovePuterPerformance::ArpPitchSource>(
                    (static_cast<uint8_t>(keyboard_.arpPitchSource()) +
                     (direction > 0 ? 1u : 3u)) % 4u)); break;
                case 2: if (arpEnabled) keyboard_.cycleArpDirection(direction); break;
                case 3: if (arpEnabled) keyboard_.cycleArpOctaves(direction); break;
                case 4: if (arpEnabled) keyboard_.setLatchEnabled(direction > 0); break;
                case 5: keyboard_.setHarmonyChangePolicy(static_cast<GroovePuterPerformance::HarmonyChangePolicy>(
                    (static_cast<uint8_t>(keyboard_.harmonyChangePolicy()) +
                     (direction > 0 ? 1u : 2u)) % 3u)); break;
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

        case PerformanceToolContext::Input:
            switch (row) {
                case ir(InRow::MidiIn): case ir(InRow::ListenChannel): case ir(InRow::SendTo):
                    adjustInput(direction);
                    break;
                case ir(InRow::PlayAs):
                    keyboard_.setPerformanceRole(static_cast<GroovePuterPerformance::PerformanceRole>(
                        (static_cast<uint8_t>(keyboard_.performanceRole()) +
                         (direction > 0 ? 1u : 4u)) % 5u));
                    break;
                case ir(InRow::LeadStyle):
                    if (keyboard_.performanceRole() == GroovePuterPerformance::PerformanceRole::Lead)
                        keyboard_.setLeadMode(static_cast<GroovePuterPerformance::LeadMode>(
                            (static_cast<uint8_t>(keyboard_.leadMode()) +
                             (direction > 0 ? 1u : 3u)) % 4u));
                    break;
                case ir(InRow::Helper):
                    cyclePartnerMode(direction);
                    break;
                case ir(InRow::ReplyBars): case ir(InRow::ReplyChannel): case ir(InRow::FreezeRiff):
                case ir(InRow::Variation): case ir(InRow::NewRiffIf):
                    cyclePartnerSetting(row, direction);
                    break;
                case ir(InRow::LoopChannel):
                    cycleLooperSetting(LooperSetting::LoopChannel, direction);
                    break;
                case ir(InRow::NoteLength):
                    cycleLooperSetting(LooperSetting::NoteLength, direction);
                    break;
                case ir(InRow::HoldLayer):
                    cycleLooperSetting(LooperSetting::HoldLayer, direction);
                    break;
                default:
                    break;
            }
            return;
        case PerformanceToolContext::Loop:
            switch (row) {
                case lr(LoopRow::ToMelody):
                    exportTarget_ = static_cast<int8_t>(exportVoiceNow() == 1 ? 0 : 1);  // A <-> B
                    break;
                case lr(LoopRow::RecordSource):
                    cycleLooperSetting(LooperSetting::RecordSource, direction);
                    break;
                case lr(LoopRow::LoopBars):
                    cycleLooperSetting(LooperSetting::LoopBars, direction);
                    break;
                case lr(LoopRow::Layer):
                    cycleLooperSetting(LooperSetting::Layer, direction);
                    break;
                case lr(LoopRow::MuteLayer):
                    cycleLooperSetting(LooperSetting::MuteLayer, direction);
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
            } else if (row == 5) {
                cycleOutput(1);
            }
            return;

        case PerformanceToolContext::Chord:
            if (row == 0) {
                openChordModePopup();
                return;
            }
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
                return;
            }
            if (row == 5 && keyboard_.target() != MusicalEventTarget::Drums) {
                keyboard_.toggleVoiceMode();
                return;
            }
            return;

        case PerformanceToolContext::Arp:
            if (row == 0) {
                keyboard_.toggleArpeggiator();
            } else if (row == 1) {
                keyboard_.setArpPitchSource(static_cast<GroovePuterPerformance::ArpPitchSource>(
                    (static_cast<uint8_t>(keyboard_.arpPitchSource()) + 1u) % 4u));
            } else if (row == 4 && arpEnabled) {
                keyboard_.toggleLatch();
            } else if (row == 5) {
                keyboard_.setHarmonyChangePolicy(static_cast<GroovePuterPerformance::HarmonyChangePolicy>(
                    (static_cast<uint8_t>(keyboard_.harmonyChangePolicy()) + 1u) % 3u));
            }
            return;

        case PerformanceToolContext::Rhythm:
            if (row == 3) {
                keyboard_.cycleEuclideanLength(1);
            } else if (row == 5 && strumIsAudible(keyboard_)) {
                keyboard_.cycleStrumDirection(1);
            }
            return;

        case PerformanceToolContext::Input:
            if (row == ir(InRow::PlayAs)) {
                keyboard_.setPerformanceRole(static_cast<GroovePuterPerformance::PerformanceRole>(
                    (static_cast<uint8_t>(keyboard_.performanceRole()) + 1u) % 5u));
            } else if (row == ir(InRow::LeadStyle)) {
                if (keyboard_.performanceRole() == GroovePuterPerformance::PerformanceRole::Lead)
                    keyboard_.setLeadMode(static_cast<GroovePuterPerformance::LeadMode>(
                        (static_cast<uint8_t>(keyboard_.leadMode()) + 1u) % 4u));
            } else if (row == ir(InRow::Helper)) {
                cyclePartnerMode(1);
            } else if (row > kPartnerRowFirst && row <= kPartnerRowLast) {
                cyclePartnerSetting(row, 1);
            } else if (row >= kLoopSetupRowFirst && row <= kLoopSetupRowLast) {  // LOOP CH, NOTE LENGTH, HOLD LAYER
                adjustSelectedValue(1);
            } else {
                adjustInput(1);
            }
            return;
        case PerformanceToolContext::Loop:
            if (row == lr(LoopRow::Looper)) {
                looperKey('z');  // ENTER on LOOPER = Z
            } else if (row == lr(LoopRow::ToMelody)) {
                exportLooperToMelody();
            } else {  // REC, LOOP BARS, LAYER, MUTE LAYER
                adjustSelectedValue(1);
            }
            return;
        case PerformanceToolContext::Count:
            return;
    }
}

// PARTNER settings rows (session only, not saved). </> and ENTER both cycle.
// Phase 10: GENRE and LENGTH have no row; the listener keeps AUTO and 100 %.
void PerformPage::cyclePartnerSetting(uint8_t row, int direction) {
    auto& partner = GroovePuterCallResponse::callResponseListener();
    switch (row) {
        case ir(InRow::ReplyBars): partner.cycleAnswerBars(direction); break;
        case ir(InRow::ReplyChannel): partner.cycleAnswerChannel(direction); break;
        case ir(InRow::FreezeRiff):
            if (partner.mode() == PartnerMode::Loop)
                partner.setLoopHold(!partner.loopHold());
            break;
        case ir(InRow::Variation):
            if (partner.mode() == PartnerMode::Loop)
                partner.cycleLoopEvolve(direction);
            break;
        case ir(InRow::NewRiffIf):
            if (partner.mode() == PartnerMode::Loop)
                partner.cycleLoopReplace(direction);
            break;
        default: break;
    }
}

// PARTNER mode row. The PARTNER and the LOOPER share one ResponsePlayer:
// the LOOPER refuses Z/C while the PARTNER is on, and turning the PARTNER on
// stops a sounding LOOPER first (its notes are kept, C plays it again once
// the PARTNER is OFF).
void PerformPage::cyclePartnerMode(int direction) {
    auto& partner = GroovePuterCallResponse::callResponseListener();
    const bool wasEnabled = partner.enabled();
    auto& loop = GroovePuterLooper::looper();
    if (!wasEnabled && loop.state() != Looper::State::Idle &&
        loop.state() != Looper::State::Stopped) {
        (void)loop.panicStop();
        UI::showToast("LOOP: STOPPED, HELPER ON", 1000);
    }
    partner.cycleMode(direction);
}

// LOOPER settings rows (session only, not saved). </> and ENTER both cycle;
// HOLD is B, LAYER is M. BARS keeps the notes (longer repeats, shorter cuts, V).
void PerformPage::cycleLooperSetting(LooperSetting setting, int direction) {
    auto& loop = GroovePuterLooper::looper();
    switch (setting) {
        case LooperSetting::RecordSource:
            if (!loop.setRecordSound(!loop.recordSound()))
                UI::showToast("LOOP: STOP REC TO CHANGE", 1600);
            break;
        case LooperSetting::LoopBars: {
            const bool notes = !loop.empty();
            const Looper::Result result = loop.cycleBars(direction);
            char toast[32];
            std::snprintf(toast, sizeof(toast), "LOOP: %u BAR%s%s", static_cast<unsigned>(loop.bars()),
                          loop.bars() == 1 ? "" : "S", notes ? " (V UNDO)" : "");
            if (notes || result != Looper::Result::Done) showLooperToast(result, toast);
            break;
        }
        case LooperSetting::LoopChannel: loop.cycleChannel(direction); break;
        case LooperSetting::NoteLength: loop.cycleLength(direction); break;
        case LooperSetting::HoldLayer: looperKey('b'); break;
        case LooperSetting::Layer: looperKey('m'); break;
        case LooperSetting::MuteLayer: {
            const Looper::Result result = loop.toggleMute();
            char toast[32];
            std::snprintf(toast, sizeof(toast), "LOOP: LAYER %u %s",
                          static_cast<unsigned>(loop.activeLayer() + 1u),
                          loop.layerMuted(loop.activeLayer()) ? "MUTED" : "ON");
            showLooperToast(result, toast);
            break;
        }
    }
}

// Target synth of TO MELODY: the A/B picked on the row (LEFT/RIGHT), or the
// GEN TARGET until one is picked. Picking never changes GEN TARGET.
int PerformPage::exportVoiceNow() const {
    if (exportTarget_ == 0 || exportTarget_ == 1) return exportTarget_;
    return GroovePuterState::melodyTargetVoice() == 1 ? 1 : 0;
}

// TO MELODY: the unmuted layers of the loop become the Working Melody of the
// synth picked on the row (default GEN TARGET), as played (no Smart Play). ENTER shows what would be taken,
// ENTER again within kExportConfirmMs takes it: one Undo step, nothing is saved
// (ALT+ENTER on the synth page saves). Main loop only; the converter and the
// 1284-byte buffer live only for the press (new, nothrow: no memory = a toast).
void PerformPage::exportLooperToMelody() {
    auto& loop = GroovePuterLooper::looper();
    const int voice = exportVoiceNow();
    const char synth = static_cast<char>('A' + voice);
    exportArmed_ = exportArmed_ && (millis() - exportArmedMs_ <= kExportConfirmMs);
    if (loop.empty()) {
        exportArmed_ = false;
        UI::showToast("LOOP: EMPTY, Z TO REC", 1000);
        return;
    }
    // As GRAB: an unsaved Working Melody in the target is never overwritten.
    if (SynthSequencerPage::voiceHasUnsavedEdits(miniAcid_, voice)) {
        exportArmed_ = false;
        char unsaved[40];
        std::snprintf(unsaved, sizeof(unsaved), "SAVE OR DISCARD %c FIRST", synth);
        UI::showToast(unsaved, 1400);
        return;
    }
    std::unique_ptr<PhraseRuntime::RuntimeSynthEventBuffer> melody(
        new (std::nothrow) PhraseRuntime::RuntimeSynthEventBuffer());
    if (!melody) {
        exportArmed_ = false;
        UI::showToast("TO MELODY: NO MEMORY", 1200);
        return;
    }
    const auto result = GroovePuterLooper::loopToMelody(
        loop.layer(), loop.passSteps(), loop.bars(), loop.muteMask(), *melody);
    if (result != GroovePuterLooper::MelodyExportResult::Ok) {
        exportArmed_ = false;
        const char* text = "TO MELODY FAILED";
        if (result == GroovePuterLooper::MelodyExportResult::Empty) text = "TO MELODY: ALL LAYERS MUTED";
        else if (result == GroovePuterLooper::MelodyExportResult::Overflow) text = "TO MELODY: TOO MANY NOTES";
        UI::showToast(text, 1200);
        return;
    }
    const uint8_t bars = loop.bars();
    const bool confirm = exportArmed_ && exportVoice_ == voice && exportBars_ == bars &&
                         exportMuteMask_ == loop.muteMask() && exportNotes_ == melody->count;
    char toast[40];
    if (!confirm) {
        exportArmed_ = true;
        exportArmedMs_ = millis();
        exportVoice_ = static_cast<int8_t>(voice);
        exportBars_ = bars;
        exportMuteMask_ = loop.muteMask();
        exportNotes_ = static_cast<uint8_t>(melody->count);
        std::snprintf(toast, sizeof(toast), "REPLACE MELODY %c WITH LOOP? ENTER", synth);
        UI::showToast(toast, kExportConfirmMs);
        return;
    }
    exportArmed_ = false;
    using Done = SynthSequencerPage::ReplaceOneUndoResult;
    const Done done = SynthSequencerPage::replaceMelodyOneUndo(miniAcid_, audioGuard_, voice, *melody);
    if (done == Done::Done) {
        std::snprintf(toast, sizeof(toast), "MELODY %c <- LOOP  CTRL+Z ON SYNTH %c", synth, synth);
    } else if (done == Done::NoMemory) {
        std::snprintf(toast, sizeof(toast), "TO MELODY: NO MEMORY");
    } else {
        std::snprintf(toast, sizeof(toast), "TO MELODY %c FAILED", synth);
    }
    UI::showToast(toast, 1800);
}

// The live LOOPER keys (also from the tools layer and the LOOPER row).
bool PerformPage::looperKey(char key) {
    auto& loop = GroovePuterLooper::looper();
    using State = Looper::State;
    switch (key) {
        case 'z': {
            const State before = loop.state();
            const bool starts = before == State::Idle || before == State::Play ||
                                before == State::Stopped;
            if (starts && !loop.hold() && keyboard_.target() == MusicalEventTarget::Drums) {
                UI::showToast("LOOP: N/A FOR DRUMS", 1000);
                return true;
            }
            const Looper::Result result = loop.recDub();
            const char* done = "LOOP: DUB";
            switch (loop.state()) {
                case State::Armed:
                    done = loop.clockSource() == GroovePuterLooper::LoopClock::Source::Transport
                        ? "LOOP: REC AT NEXT BAR" : "LOOP: PLAY A NOTE TO REC";
                    break;
                case State::Idle: done = "LOOP: REC CANCELLED"; break;
                case State::Record: done = loop.dubArmed() ? "LOOP: THEN DUB" : "LOOP: THEN PLAY"; break;
                case State::Play: done = "LOOP: DUB END"; break;
                default: break;
            }
            showLooperToast(result, done);
            // The first Z that starts a pass on a non-external target: the loop
            // sounds on the DX route only (no internal sound). Once per page.
            if (starts && result == Looper::Result::Done && !dxRouteNoted_ &&
                loop.channel() == 0 && keyboard_.target() != MusicalEventTarget::Dx) {
                dxRouteNoted_ = true;
                UI::showToast("LOOP: DX ROUTE, NO INTERNAL SOUND", 1600);
            }
            return true;
        }
        case 'c': {
            const Looper::Result result = loop.playStop();
            showLooperToast(result, loop.state() == State::Play ? "LOOP: PLAY" : "LOOP: STOP");
            return true;
        }
        case 'v': {
            const bool recording = loop.state() == State::Record;
            const Looper::Result result = loop.undo();
            showLooperToast(result, recording ? "LOOP: REC CANCELLED" : "LOOP: UNDO PASS");
            return true;
        }
        case 'b': {
            const Looper::Result result = loop.toggleHold();
            char toast[32];
            std::snprintf(toast, sizeof(toast), "LOOP: L%u HOLD %s",
                          static_cast<unsigned>(loop.activeLayer() + 1u), loop.hold() ? "ON" : "OFF");
            showLooperToast(result, loop.holdArmed() && !loop.hold() ? "LOOP: HOLD AFTER REC" : toast);
            return true;
        }
        case 'm': {
            // M: the other layer becomes active (REC/DUB, HOLD, CLEAR, MUTE go there).
            const Looper::Result result = loop.nextLayer();
            const uint8_t layer = loop.activeLayer();
            char toast[32];
            std::snprintf(toast, sizeof(toast), "LOOP: LAYER %u%s%s", static_cast<unsigned>(layer + 1u),
                          loop.layerHold(layer) ? " HOLD" : "", loop.layerMuted(layer) ? " MUTED" : "");
            showLooperToast(result, toast);
            return true;
        }
        case '\b': {
            const Looper::Result result = loop.clear();
            showLooperToast(result, loop.empty() && loop.canUndo() ? "LOOP: CLEARED (V UNDO)"
                                                                     : "LOOP: REC CANCELLED");
            return true;
        }
        default:
            return false;
    }
}

void PerformPage::onExit() {
    GroovePuterLooper::looper().onPageExit();
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
                case 3: return "</> VELOCITY (EXT KEYS: SCALE)";
                case 4: return "</> OR \\: TARGET INSTRUMENT";
                case 5:
                    return keyboard_.target() == MusicalEventTarget::Dx
                        ? "N/A: DX IS MIDI ONLY"
                        : "</> OR ENTER: INTERNAL / MIDI / LAYER";
                default: return "";
            }

        case PerformanceToolContext::Chord:
            switch (row) {
                case 0: return "ENTER: LIST  </> STEP";
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
                case 5:
                    return drums ? "N/A: DRUMS ARE ALWAYS 7 LANES"
                                 : "</> OR ENTER: MONO/POLY RECEIVER";
                default: return "";
            }

        case PerformanceToolContext::Arp:
            if (row == 0) return "</> OR ENTER: ON/OFF (CLEARS STRUM)";
            switch (row) {
                case 1:
                    if (keyboard_.arpPitchSource() == GroovePuterPerformance::ArpPitchSource::Memory &&
                        keyboard_.chordMemorySize() == 0) return "MEMORY EMPTY: CAPTURE IN CHORD";
                    if ((keyboard_.arpPitchSource() == GroovePuterPerformance::ArpPitchSource::Harmony ||
                         keyboard_.arpPitchSource() == GroovePuterPerformance::ArpPitchSource::Role) &&
                        !keyboard_.performanceHarmonyAvailable()) return "NO HARMONY: HELD STILL WORKS";
                    return "</> ARP PITCH SOURCE";
                case 2: return arpEnabled ? "</> NOTE ORDER" : "N/A: ENABLE ARP FIRST";
                case 3: return arpEnabled ? "</> OCTAVE RANGE 1..4" : "N/A: ENABLE ARP FIRST";
                case 4: return arpEnabled ? "</> OR ENTER: HOLD AFTER RELEASE" : "N/A: ENABLE ARP FIRST";
                case 5: return "</> HARMONY CHANGE: FOLLOW/HOLD/RESTART";
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

        case PerformanceToolContext::Input: {
            // One hint line (<= 38 characters at 6 px): what the setting gives;
            // only the non-standard keys (Z, C, V, B, M, ENTER twice) are named.
            const bool riff = GroovePuterCallResponse::callResponseListener().mode() == PartnerMode::Loop;
            switch (row) {
                case ir(InRow::MidiIn): return "MIDI KEYBOARD IN  USB: PROJECT>MIDI";
                case ir(InRow::ListenChannel): return "HEAR ONE MIDI CHANNEL OR ALL (OMNI)";
                case ir(InRow::SendTo): return "WHERE MIDI NOTES GO; PERFORM=KEY TAB";
                case ir(InRow::PlayAs): return "SHAPES NEW LIVE NOTES ONLY";
                case ir(InRow::LeadStyle):
                    if (keyboard_.performanceRole() != GroovePuterPerformance::PerformanceRole::Lead)
                        return "N/A: SET PLAY AS TO LEAD";
                    if (keyboard_.leadMode() != GroovePuterPerformance::LeadMode::Raw &&
                        !keyboard_.performanceHarmonyAvailable())
                        return "NO HARMONY: SCALE FALLBACK";
                    return "HOW LEAD NOTES FIT THE HARMONY";
                case ir(InRow::Helper): return "MIDI REPLIES TO WHAT YOU PLAY";
                case ir(InRow::ReplyBars): return "LENGTH OF CALL AND REPLY";
                case ir(InRow::ReplyChannel): return "DX ROUTE: SAME CH AS PERFORM DX";
                case ir(InRow::FreezeRiff):
                    return riff ? "ON: KEEP RIFF  OFF: NEW AT NEXT BAR" : "N/A: SET HELPER TO RIFF";
                case ir(InRow::Variation):
                    return riff ? "HOW MUCH THE RIFF CHANGES" : "N/A: SET HELPER TO RIFF";
                case ir(InRow::NewRiffIf):
                    return riff ? "NOTES YOU PLAY TO START A NEW RIFF" : "N/A: SET HELPER TO RIFF";
                case ir(InRow::LoopChannel): return "DX ROUTE: SAME CH AS PERFORM DX";
                case ir(InRow::NoteLength): return "SHORTER LOOP NOTES ON PLAYBACK";
                case ir(InRow::HoldLayer): return "B: HOLD LAYER, NO REC/UNDO/CLEAR";
                default: return "";
            }
        }

        case PerformanceToolContext::Loop:
            switch (row) {
                case lr(LoopRow::Looper): return "Z REC/DUB  C PLAY/STOP  V UNDO";
                case lr(LoopRow::RecordSource): return "KEYS: INPUT NOTES  SOUND: PLAYED NOTES";
                case lr(LoopRow::LoopBars): return GroovePuterLooper::looper().empty()
                    ? "LOOP LENGTH IN BARS" : "LONGER REPEATS THE NOTES  V UNDO";
                case lr(LoopRow::Layer): return "M: LAYER FOR REC/DUB, HOLD, CLEAR";
                case lr(LoopRow::MuteLayer): return "SILENCE THIS LAYER AT ONCE";
                case lr(LoopRow::ToMelody): return "LOOP TO SYNTH MELODY: ENTER TWICE";
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
    // Fixed slots: "[KEY] [CHORD] [ARP] [RHYTHM] [INPUT] [LOOP]" (len + 2 cells each: six
    // tabs of len + 3 would be 264 px wide, the screen is 240). The bracket cell to the
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
        x += (len + 2) * charW;
    }

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
    constexpr uint8_t kVisibleRows = 6;
    // INPUT draws a group heading line above MIDI IN, LOOP SETUP and HELPER; the
    // window scrolls by screen lines so the selected row is always visible.
    const bool inputContext = selectedContext_ == PerformanceToolContext::Input;
    auto lineOf = [&](uint8_t row) -> uint8_t { return inputContext ? inputLine(row) : row; };
    const uint8_t selectedLine = lineOf(selected);
    const uint8_t firstVisibleRow = selectedLine >= kVisibleRows
        ? static_cast<uint8_t>(selectedLine - kVisibleRows + 1u) : 0u;
    const bool drums = keyboard_.target() == MusicalEventTarget::Drums;
    const bool chordActive = keyboard_.chordMode() != PerformanceChordMode::Off;
    const bool arpEnabled = keyboard_.arpeggiatorEnabled();
    char value[48];

    drawToolTabs(gfx, LayoutManager::lineY(0));

    // A group heading ("--- HELPER ---"): drawn, never selected.
    auto drawHeading = [&](uint8_t line, const char* heading) {
        if (line < firstVisibleRow || line >= firstVisibleRow + kVisibleRows) return;
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(1 + static_cast<int>(line - firstVisibleRow)),
                     heading);
    };

    auto drawRow = [&](uint8_t row, const char* label, const char* rowValue,
                       bool available) {
        const uint8_t line = lineOf(row);
        if (line < firstVisibleRow || line >= firstVisibleRow + kVisibleRows) return;
        const int visibleRow = static_cast<int>(line - firstVisibleRow);
        const int y = LayoutManager::lineY(1 + visibleRow);
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
            drawRow(4, "TARGET", keyboard_.targetName(), true);
            if (keyboard_.target() == MusicalEventTarget::Dx) {
                drawRow(5, "OUT", "MIDI", false);
            } else {
                GroovePuterOutput::Track track = GroovePuterOutput::Track::SynthA;
                if (GroovePuterOutput::trackForTarget(keyboard_.target(), track)) {
                    drawRow(5, "OUT",
                            GroovePuterOutput::modeName(GroovePuterOutput::mode(track)),
                            true);
                } else {
                    drawRow(5, "OUT", "MIDI", false);
                }
            }
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
            drawRow(5, "VOICE", drums ? "N/A" : keyboard_.voiceModeName(), !drums);
            break;
        }

        case PerformanceToolContext::Arp: {
            const bool latched = arpEnabled && keyboard_.latchEnabled();
            drawRow(0, "ARP", !arpEnabled ? "OFF" : (latched ? "LATCHED" : "ON"), true);
            const auto pitchSource = keyboard_.arpPitchSource();
            const bool memoryReady = keyboard_.chordMemorySize() > 0;
            const bool harmonySource = pitchSource == GroovePuterPerformance::ArpPitchSource::Harmony ||
                                       pitchSource == GroovePuterPerformance::ArpPitchSource::Role;
            const bool harmonyReady = keyboard_.performanceHarmonyAvailable();
            const char* sourceValue = arpSourceName(pitchSource);
            if (pitchSource == GroovePuterPerformance::ArpPitchSource::Memory && !memoryReady)
                sourceValue = "EMPTY";
            else if (harmonySource && !harmonyReady)
                sourceValue = "NO HARM";
            drawRow(1, "SOURCE", sourceValue,
                    pitchSource == GroovePuterPerformance::ArpPitchSource::Held ||
                    (pitchSource == GroovePuterPerformance::ArpPitchSource::Memory && memoryReady) ||
                    (harmonySource && harmonyReady));
            drawRow(2, "ORDER", arpEnabled ? arpOrderLabel(keyboard_.arpDirection()) : "N/A", arpEnabled);
            std::snprintf(value, sizeof(value), "%u",
                          static_cast<unsigned>(keyboard_.arpOctaves()));
            drawRow(3, "OCTAVES", arpEnabled ? value : "N/A", arpEnabled);
            drawRow(4, "LATCH", !arpEnabled ? "N/A" : (latched ? "ON" : "OFF"), arpEnabled);
            drawRow(5, "HARMONY", harmonyPolicyName(keyboard_.harmonyChangePolicy()), true);
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
            if (keyboard_.euclideanPulses() > 0 && lineOf(3) >= firstVisibleRow) {
                // 16-step strip right of the digits: a filled square is a hit, a
                // small dot a rest, the running step is boxed (the current pulse of
                // the shared step clock modulo the length).
                const uint8_t length = std::min<uint8_t>(keyboard_.euclideanLength(),
                                                         PerformanceKeyboard::kEuclideanSteps);
                const uint8_t cursor = length ? static_cast<uint8_t>(keyboard_.musicalPulseOrdinal() % length) : 0;
                const int stripX = valueX + 36;
                const int stripY = LayoutManager::lineY(1 + static_cast<int>(lineOf(3) - firstVisibleRow)) + 1;
                const bool onSelected = selected == 3;
                for (uint8_t step = 0; step < length; ++step) {
                    const int cellX = stripX + step * 5;
                    const bool hit = PerformKeyHeader::euclidStepHit(
                        step, keyboard_.euclideanPulses(), keyboard_.euclideanLength(),
                        keyboard_.euclideanRotation());
                    const IGfxColor ink = onSelected ? palette.invert : palette.text;
                    if (hit) gfx.fillRect(cellX, stripY, 4, 5, ink);
                    else gfx.fillRect(cellX + 1, stripY + 2, 2, 1, onSelected ? palette.invert : palette.dim);
                    if (step == cursor) gfx.drawRect(cellX - 1, stripY - 2, 6, 9, MusicVisuals::accentForStyle());
                }
            }
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

        case PerformanceToolContext::Input: {
            namespace InputUi = GroovePuterUi::MidiInputUi;
            const auto input = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
            drawHeading(0, "--- MIDI IN ---");
            drawRow(ir(InRow::MidiIn), "MIDI IN", InputUi::enabledName(input.enabled), true);
            InputUi::formatChannel(input, value, sizeof(value));
            drawRow(ir(InRow::ListenChannel), "LISTEN CH", value, true);
            drawRow(ir(InRow::SendTo), "SEND TO", InputUi::targetName(input.target), true);
            drawRow(ir(InRow::PlayAs), "PLAY AS", roleName(keyboard_.performanceRole()), true);
            const bool lead = keyboard_.performanceRole() == GroovePuterPerformance::PerformanceRole::Lead;
            drawRow(ir(InRow::LeadStyle), "LEAD STYLE", lead ? leadModeName(keyboard_.leadMode()) : "N/A", lead);

            // PARTNER: HELPER is the listener mode (RIFF = Mode::Loop).
            const auto& partner = GroovePuterCallResponse::callResponseListener();
            drawHeading(inputLine(kPartnerRowFirst) - 1u, "--- HELPER ---");
            drawRow(ir(InRow::Helper), "HELPER", helperModeName(partner), true);
            const bool partnerOn = partner.enabled();
            std::snprintf(value, sizeof(value), "%u BAR%s",
                          static_cast<unsigned>(partner.answerBars()),
                          partner.answerBars() == 1 ? "" : "S");
            drawRow(ir(InRow::ReplyBars), "REPLY BARS", value, partnerOn);
            // Phase 10: no GENRE row (AUTO = project genre) and no LENGTH row
            // (100 %); the listener setters stay for tests and a later return.
            if (partner.answerChannel() == 0) {
                std::snprintf(value, sizeof(value), "DX ROUTE");
            } else {
                std::snprintf(value, sizeof(value), "CH%u",
                              static_cast<unsigned>(partner.answerChannel()));
            }
            drawRow(ir(InRow::ReplyChannel), "REPLY CH", value, partnerOn);
            const bool riff = partner.mode() == PartnerMode::Loop;
            drawRow(ir(InRow::FreezeRiff), "FREEZE RIFF",
                    !riff ? "N/A" : (!partner.hasLoopPhrase() ? "WAIT" : (partner.loopHold() ? "ON" : "OFF")),
                    riff && partner.hasLoopPhrase());
            drawRow(ir(InRow::Variation), "VARIATION", riff ? riffVariationName(partner.loopEvolve()) : "N/A", riff);
            drawRow(ir(InRow::NewRiffIf), "NEW RIFF IF", riff ? newRiffIfName(partner.loopReplace()) : "N/A", riff);

            // LOOP SETUP: the rarely changed looper settings (the looper itself
            // is on the [LOOP] tab; Z/C/V/B/Del/M work on the live surface).
            const auto& loop = GroovePuterLooper::looper();
            drawHeading(inputLine(kLoopSetupRowFirst) - 1u, "--- LOOP SETUP ---");
            if (loop.channel() == 0) {
                std::snprintf(value, sizeof(value), "DX ROUTE");
            } else {
                std::snprintf(value, sizeof(value), "CH%u", static_cast<unsigned>(loop.channel()));
            }
            drawRow(ir(InRow::LoopChannel), "LOOP CH", value, true);
            std::snprintf(value, sizeof(value), "%u%%", static_cast<unsigned>(loop.lengthPercent()));
            drawRow(ir(InRow::NoteLength), "NOTE LENGTH", value, true);
            const bool layerEmpty = loop.layerNoteCount(loop.activeLayer()) == 0;
            drawRow(ir(InRow::HoldLayer), "HOLD LAYER",
                    layerEmpty && !loop.hold() ? "N/A" : (loop.hold() ? "ON" : "OFF"), !layerEmpty);
            break;
        }

        case PerformanceToolContext::Loop: {
            const auto& partner = GroovePuterCallResponse::callResponseListener();
            const bool partnerOn = partner.enabled();
            const auto& loop = GroovePuterLooper::looper();
            formatLooperState(value, sizeof(value));
            drawRow(lr(LoopRow::Looper), "LOOPER", partnerOn ? "HELPER ON" : value, !partnerOn);
            drawRow(lr(LoopRow::RecordSource), "REC", loop.recordSound() ? "SOUND" : "KEYS", true);
            std::snprintf(value, sizeof(value), "%u BAR%s",
                          static_cast<unsigned>(loop.bars()), loop.bars() == 1 ? "" : "S");
            drawRow(lr(LoopRow::LoopBars), "LOOP BARS", value,
                    loop.holdMask() == 0 && loop.state() != Looper::State::Record);
            std::snprintf(value, sizeof(value), "%u  (%u NOTES)", static_cast<unsigned>(loop.activeLayer() + 1u),
                          static_cast<unsigned>(loop.layerNoteCount(loop.activeLayer())));
            drawRow(lr(LoopRow::Layer), "LAYER", value, loop.state() != Looper::State::Record);
            drawRow(lr(LoopRow::MuteLayer), "MUTE LAYER", loop.layerMuted(loop.activeLayer()) ? "ON" : "OFF", true);
            std::snprintf(value, sizeof(value), "SYN %c",
                          static_cast<char>('A' + exportVoiceNow()));
            drawRow(lr(LoopRow::ToMelody), "TO MELODY", loop.empty() ? "N/A" : value, !loop.empty());
            break;
        }

        case PerformanceToolContext::Count:
            break;
    }

    // INPUT is longer than the window: a 2 px scroll bar on the right edge
    // (outside the row highlight and the clipped value text).
    if (inputContext && kInputLineCount > kVisibleRows) {
        const int trackY = LayoutManager::lineY(1) - 1;
        const int trackH = kVisibleRows * Layout::LINE_HEIGHT - 2;
        const int barX = rowX + rowW;
        const int thumbH = std::max(4, trackH * kVisibleRows / kInputLineCount);
        const int span = kInputLineCount - kVisibleRows;
        const int thumbY = trackY + (trackH - thumbH) * firstVisibleRow / span;
        gfx.fillRect(barX, trackY, 2, trackH, palette.dim);
        gfx.fillRect(barX, thumbY, 2, thumbH, palette.secondary);
    }

    // The global performance HUD strip is composited over the bottom of the
    // content zone, so the hint sits two pixels above the nominal line 7.
    gfx.setTextColor(COLOR_LABEL);
    Widgets::drawClippedText(gfx, Layout::COL_1, LayoutManager::lineY(7) - 2,
                             Layout::CONTENT.w - 2 * Layout::CONTENT_PAD_X,
                             selectedRowHint());
    chordModePopup_.draw(gfx, kChordPopupLabels);
}

bool PerformPage::handleEvent(UIEvent& event) {
    // Ctrl+Z: the LOOPER's undo when it has one, otherwise passed on (the
    // global "UNDO: EMPTY / NOT HERE" toast stays as before).
    if (GroovePuterUndoUx::isUndoEvent(event)) {
        if (!GroovePuterLooper::looper().canUndo()) return false;
        return looperKey('v');
    }
    if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
    if (chordModePopup_.isOpen()) {
        const UI::SelectionPopupResult result = chordModePopup_.handleEvent(event);
        if (result == UI::SelectionPopupResult::Confirmed) applyChordModePopup();
        return true;
    }
    // The Cardputer keycaps label the arrows as Fn alternates, so arrows are
    // accepted with Fn (meta) held while the tools layer is open. Every other
    // modifier combination passes through to the global shortcuts.
    const bool toolsArrow = toolsLayerVisible_ && !event.ctrl && !event.alt &&
                            UIInput::navCode(event) != 0;
    const bool isAltO = event.alt && !event.ctrl && !event.meta &&
                        (event.key == 'o' || event.key == 'O' ||
                         event.scancode == GROOVEPUTER_O);
    if (isAltO) {
        cycleOutput(1);
        return true;
    }
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
            moveContext(1);
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

    // The Cardputer arrow keycaps are ; , . / and, since #465, deliver ONLY the arrow scancode (the
    // printed character is suppressed centrally). The live scale bindings below were written for the
    // characters, so without this the LEFT/RIGHT arrows stopped changing the scale on the live page.
    if (!event.ctrl && !event.alt && !event.meta) {
        switch (UIInput::navCode(event)) {
            case GROOVEPUTER_LEFT:
                cycleScaleWithToast(keyboard_, -1);
                return true;
            case GROOVEPUTER_RIGHT:
                cycleScaleWithToast(keyboard_, 1);
                return true;
            default:
                break;
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
            char toast[48];
            if (keyboard_.target() == MusicalEventTarget::Dx) {
                std::snprintf(toast, sizeof(toast), "TARGET: DX [MIDI CH10]");
            } else {
                GroovePuterOutput::Track track = GroovePuterOutput::Track::SynthA;
                if (GroovePuterOutput::trackForTarget(keyboard_.target(), track)) {
                    const GroovePuterOutput::Mode outMode = GroovePuterOutput::mode(track);
                    std::snprintf(toast, sizeof(toast), "TARGET: %s [%s]",
                                  keyboard_.targetName(),
                                  GroovePuterOutput::modeName(outMode));
                } else {
                    std::snprintf(toast, sizeof(toast), "TARGET: %s",
                                  keyboard_.targetName());
                }
            }
            UI::showToast(toast, 1000);
            return true;
        }
        case ',':
        case '<':
            cycleScaleWithToast(keyboard_, -1);
            return true;
        case '.':
        case '>':
            cycleScaleWithToast(keyboard_, 1);
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
            GroovePuterCallResponse::callResponseListener().silence();
            (void)GroovePuterLooper::looper().panicStop();  // the layer is kept
            keyboard_.panic();
            char toast[40];
            std::snprintf(toast, sizeof(toast), "PANIC: PARTNER SILENT, LOOP STOPPED");
            UI::showToast(toast, 1000);
            return true;
        }
        // LOOPER (live surface and tools layer): Z REC/DUB, C PLAY/STOP,
        // V UNDO, B HOLD. Del = CLEAR on the live surface only (in the tools
        // layer Backspace is "previous context").
        case 'z': case 'Z': return looperKey('z');
        case 'c': case 'C': return looperKey('c');
        case 'v': case 'V': return looperKey('v');
        case 'b': case 'B': return looperKey('b');
        case 'm': case 'M': return looperKey('m');
        case '\b':
        case 0x7F:
            if (toolsLayerVisible_) return false;
            return looperKey('\b');
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

    char routeChip[16];
    if (keyboard_.target() == MusicalEventTarget::Dx) {
        std::snprintf(routeChip, sizeof(routeChip), "MIDI CH10");
    } else {
        GroovePuterOutput::Track track = GroovePuterOutput::Track::SynthA;
        if (GroovePuterOutput::trackForTarget(keyboard_.target(), track)) {
            const GroovePuterOutput::Mode outMode = GroovePuterOutput::mode(track);
            if (outMode == GroovePuterOutput::Mode::Internal) {
                std::snprintf(routeChip, sizeof(routeChip), "INTERNAL");
            } else if (outMode == GroovePuterOutput::Mode::Midi) {
                if (drums) {
                    std::snprintf(routeChip, sizeof(routeChip), "MIDI CH1-7");
                } else {
                    std::snprintf(routeChip, sizeof(routeChip), "MIDI CH%u",
                                  static_cast<unsigned>(keyboard_.targetMidiChannel()));
                }
            } else {
                if (drums) {
                    std::snprintf(routeChip, sizeof(routeChip), "LAYER CH1-7");
                } else {
                    std::snprintf(routeChip, sizeof(routeChip), "LAYER CH%u",
                                  static_cast<unsigned>(keyboard_.targetMidiChannel()));
                }
            }
        } else {
            std::snprintf(routeChip, sizeof(routeChip), "CH%u",
                          static_cast<unsigned>(keyboard_.targetMidiChannel()));
        }
    }
    x += MusicVisuals::drawChip(gfx, x, chipY, routeChip, false) + 3;

    if (!drums) {
        MusicVisuals::drawChip(gfx, x, chipY,
                               keyboard_.voiceModeName(),
                               directPoly);
    }

    gfx.setTextColor(COLOR_LABEL);
    if (drums) {
        std::snprintf(line, sizeof(line), "NATIVE 7-LANE HELD:%u VEL:%u",
                      static_cast<unsigned>(keyboard_.heldCount()),
                      static_cast<unsigned>(keyboard_.velocity()));
    } else {
        PerformKeyHeader::KeyHeaderInput keyLine;
        keyLine.root = keyboard_.rootName();
        keyLine.scale = keyboard_.scaleName();
        keyLine.octaveShift = static_cast<int>(keyboard_.octaveShift());
        keyLine.chord = keyboard_.chordModeName();
        keyLine.arp = keyboard_.arpeggiatorEnabled() ? keyboard_.arpDirectionName() : nullptr;
        keyLine.ratchet = keyboard_.ratchetCount();
        keyLine.euclidPulses = keyboard_.euclideanPulses();
        keyLine.euclidLength = keyboard_.euclideanLength();
        keyLine.strumMs = static_cast<uint8_t>(keyboard_.strumMs() > 255 ? 255 : keyboard_.strumMs());
        PerformKeyHeader::formatKeyHeader(line, sizeof(line), keyLine,
                                          static_cast<std::size_t>((Layout::CONTENT.w - 8) / 6));
    }
    Widgets::drawClippedText(gfx, Layout::COL_1, LayoutManager::lineY(1),
                             Layout::CONTENT.w - 8, line);

    if (GroovePuterLooper::looper().takeTooManyNotice())
        UI::showToast("LOOP: TOO MANY NOTES", 1000);
    else if (GroovePuterLooper::looper().takeFullNotice())
        UI::showToast("LOOP: FULL, NOTES DROPPED", 1000);
    drawLooperStrip(gfx, LayoutManager::lineY(2));

    // Line 2 is the LOOPER strip; the keyboard starts on line 3.
    const int visualY = LayoutManager::lineY(3);
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
        std::snprintf(line, sizeof(line), "NOTE MODE OFF | N ENABLE | ALT+O OUT");
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
    if (!usbBlocked && !usbReady && active < 0 && keyboard_.heldCount() == 0) {
        const auto input = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
        const auto& loop = GroovePuterLooper::looper();
        if (noteMode && loop.state() == Looper::State::Idle && loop.channel() == 0 && !drums) {
            gfx.setTextColor(COLOR_LABEL);
            std::snprintf(line, sizeof(line), "LOOP PLAYS TO MIDI ONLY | TAB TOOLS");
        } else if (input.enabled) {
            const bool follows = input.target == GroovePuterMidi::MidiInputTarget::Perform;
            std::snprintf(line, sizeof(line), "IN: %s%s%s | TAB TOOLS",
                GroovePuterUi::MidiInputUi::targetName(input.target),
                follows ? " > " : "", follows ? keyboard_.targetName() : "");
        }
    }
    Widgets::drawClippedText(gfx, Layout::COL_1, LayoutManager::lastLineY(gfx),
                             Layout::CONTENT.w - 8, line);
}

void PerformPage::drawFooter(IGfx& gfx) {
    if (toolsLayerVisible_) {
        UI::drawStandardFooter(gfx,
                               "[TAB]CTX [U/D]ROW [L/R]VAL",
                               "[ENT]ACTION [ESC]LIVE");
        return;
    }
    // Live surface: the footer follows the LOOPER state (LOOPER_DESIGN §1.4).
    const auto& loop = GroovePuterLooper::looper();
    const bool undo = loop.canUndo();
    if (loop.hold()) {
        UI::drawStandardFooter(gfx, "[B]HOLD OFF [M]LAYER", "[C]PLAY/STOP [X]PANIC");
        return;
    }
    switch (loop.state()) {
        case Looper::State::Armed:
            UI::drawStandardFooter(gfx,
                loop.clockSource() == GroovePuterLooper::LoopClock::Source::Transport
                    ? "[Z]CANCEL  WAIT FOR BAR" : "[Z]CANCEL  PLAY A NOTE",
                "[X]PANIC");
            return;
        case Looper::State::Record:
            UI::drawStandardFooter(gfx, loop.dubArmed() ? "[Z]THEN PLAY [V]CANCEL"
                                                        : "[Z]THEN DUB [V]CANCEL",
                                   "[C]STOP [X]PANIC");
            return;
        case Looper::State::Play:
            UI::drawStandardFooter(gfx, "[Z]DUB [C]STOP [B]HOLD",
                                   undo ? "[V]UNDO [DEL]CLR [M]LYR" : "[DEL]CLEAR [M]LAYER");
            return;
        case Looper::State::Overdub:
            UI::drawStandardFooter(gfx, undo ? "[Z]END DUB [V]UNDO" : "[Z]END DUB",
                                   "[B]HOLD [C]STOP");
            return;
        case Looper::State::Stopped:
            UI::drawStandardFooter(gfx, "[C]PLAY [Z]DUB [M]LAYER",
                                   undo ? "[V]UNDO [DEL]CLEAR" : "[DEL]CLEAR");
            return;
        case Looper::State::Idle:
            break;
    }
    UI::drawStandardFooter(gfx,
                           undo ? "[Z]REC [V]UNDO Tab Tools" : "[Z]REC Tab Tools [\\]TGT",
                           "[-/+]OCT [N]NOTE [X]PANIC");
}

void PerformPage::formatLooperState(char* out, std::size_t size) const {
    const auto& loop = GroovePuterLooper::looper();
    const double position = loop.passPositionNow();
    const unsigned bar = position >= 0.0
        ? static_cast<unsigned>(position / GroovePuterLooper::kStepsPerBar) + 1u : 1u;
    const unsigned bars = loop.bars();
    switch (loop.state()) {
        case Looper::State::Record:
            std::snprintf(out, size, "REC %u/%u%s", bar > bars ? bars : bar, bars,
                          loop.holdArmed() ? ">HOLD" : (loop.dubArmed() ? ">DUB" : ""));
            return;
        case Looper::State::Play:
            std::snprintf(out, size, "PLAY %u/%u%s", bar > bars ? bars : bar, bars,
                          loop.hold() ? " HOLD" : "");
            return;
        default:
            std::snprintf(out, size, "%s%s", loop.stateName(), loop.hold() ? " HOLD" : "");
            return;
    }
}

// Line 2 of the live surface: state chip (+ HOLD chip), the pass with its bar
// and beat marks, one tick per loop note (notes of the undoable pass
// brighter), the playhead, and the note counter.
void PerformPage::drawLooperStrip(IGfx& gfx, int y) {
    const auto& loop = GroovePuterLooper::looper();
    const UI::ThemePalette palette = UI::themePalette();
    const bool blinkOn = ((millis() / 250u) & 1u) == 0u;
    const char* label = "LOOP";
    bool active = false;
    IGfxColor color = MusicVisuals::accentForStyle();
    switch (loop.state()) {
        case Looper::State::Idle: break;
        case Looper::State::Armed: label = "ARM"; active = blinkOn; color = COLOR_WARN; break;
        case Looper::State::Record:
            label = loop.holdArmed() ? "REC>H" : "REC"; active = true; color = COLOR_DANGER; break;
        case Looper::State::Overdub: label = "DUB"; active = true; color = COLOR_WARN; break;
        case Looper::State::Play: label = "PLAY"; active = true; break;
        case Looper::State::Stopped: label = "STOP"; break;
    }
    int x = Layout::COL_1;
    x += MusicVisuals::drawChip(gfx, x, y, label, active, color) + 2;
    if (loop.hold()) x += MusicVisuals::drawChip(gfx, x, y, "HOLD", true, palette.secondary) + 2;
    // Phase 3: the active layer (L1/L2, filled while it is muted).
    const uint8_t activeLayer = loop.activeLayer();
    x += MusicVisuals::drawChip(gfx, x, y, activeLayer == 0 ? "L1" : "L2",
                                loop.layerMuted(activeLayer), palette.dim) + 2;

    char counter[12];
    IGfxColor counterColor = COLOR_LABEL;
    const double toRecord = loop.stepsToRecord();
    if (loop.state() == Looper::State::Armed) {
        if (toRecord >= 0.0) {
            std::snprintf(counter, sizeof(counter), "WAIT %u",
                          static_cast<unsigned>(toRecord / GroovePuterLooper::kStepsPerBar) + 1u);
        } else {
            std::snprintf(counter, sizeof(counter), "WAIT");
        }
        counterColor = COLOR_WARN;
    } else if (loop.full()) {
        std::snprintf(counter, sizeof(counter), "FULL");
        counterColor = COLOR_DANGER;
    } else {
        if (loop.state() == Looper::State::Idle && loop.empty()) {
            // Nothing recorded yet: show the first recording hint.
            std::snprintf(counter, sizeof(counter), "Z REC");
        } else {
            std::snprintf(counter, sizeof(counter), "%u/%u",
                          static_cast<unsigned>(loop.noteCount()),
                          static_cast<unsigned>(GroovePuterLooper::kLoopCapacity));
            // Something to undo (V): the counter is brighter, no letter.
            if (loop.canUndo()) counterColor = COLOR_TEXT;
        }
        if (loop.layer().slotCount() + 4u >= GroovePuterLooper::kLoopCapacity) counterColor = COLOR_WARN;
    }
    const int right = Layout::CONTENT.x + Layout::CONTENT.w - Layout::CONTENT_PAD_X;
    const int counterW = static_cast<int>(std::strlen(counter)) * 6;
    gfx.setTextColor(counterColor);
    gfx.drawText(right - counterW, y + 1, counter);

    const int barX = x;
    const int barW = right - counterW - 3 - barX;
    const int barH = 10;
    if (barW < 16) return;
    gfx.drawRect(barX, y, barW, barH, palette.dim);
    const int steps = loop.passSteps();
    const int innerW = barW - 2;
    auto stepX = [&](double step) { return barX + 1 + static_cast<int>(step * innerW / steps); };
    // Beat marks only while a beat is at least 6 px wide. BARS 8 on the
    // ~150 px strip (~1.2 px per step): bar marks only, the middle one (bar 5)
    // full height.
    const bool beatMarks = innerW * 4 >= steps * 6;
    for (int beat = 4; beat < steps; beat += 4) {
        const bool bar = beat % GroovePuterLooper::kStepsPerBar == 0;
        if (!bar && !beatMarks) continue;
        const bool half = steps > 64 && beat == steps / 2;
        gfx.fillRect(stepX(beat), bar ? (half ? y : y + 1) : y + barH - 3, 1,
                     bar ? (half ? barH : barH - 2) : 2, palette.dim);
    }
    const double position = loop.passPositionNow();
    if (position >= 0.0 && !loop.hold()) {
        const int fillW = stepX(position) - (barX + 1);
        if (fillW > 0) gfx.fillRect(barX + 1, y + barH - 3, fillW, 2, active ? color : palette.dim);
    }
    // Two lanes of note ticks: layer 1 on top, layer 2 below. The active
    // layer is bright (the undoable pass brightest), the other one label
    // grey, a muted layer dim.
    const auto& layer = loop.layer();
    for (uint8_t i = 0; i < layer.slotCount(); ++i) {
        const auto& note = layer.note(i);
        if (note.shadow()) continue;
        const uint8_t lane = note.layer() & 1u;
        IGfxColor tick = palette.dim;
        if (!loop.layerMuted(lane)) {
            tick = lane != activeLayer ? COLOR_LABEL
                                       : (note.isNew() ? palette.text : palette.secondary);
        }
        // The onset is in ticks (6 to a step): a triplet line is placed as played.
        gfx.fillRect(stepX(static_cast<double>(note.onsetTicks()) / GroovePuterLooper::kTicksPerStep),
                     y + 2 + lane * 3, 1, 2, tick);
    }
    if (position >= 0.0) gfx.fillRect(stepX(position), y + 1, 1, barH - 2, color);
}
