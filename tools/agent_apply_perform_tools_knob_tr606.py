#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}: {old[:80]!r}")
    write(path, text.replace(old, new, 1))


def replace_all_exact(path: str, old: str, new: str, expected: int) -> None:
    text = read(path)
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} matches, found {count}: {old[:80]!r}")
    write(path, text.replace(old, new))


# ---------------------------------------------------------------------------
# Knob keyboard acceleration: direct A/Z S/X D/C F/V controls now use the
# existing coarse step. Shift/Ctrl still select the fine step where available.
# ---------------------------------------------------------------------------
replace_once(
    "src/ui/pages/tb303_params_page.cpp",
    "  switch (lowerKey) {\n    case 't':",
    "  const int directKnobStep = fine ? kKnobStepFine : kKnobStepCoarse;\n\n"
    "  switch (lowerKey) {\n    case 't':",
)
replace_once(
    "src/ui/pages/tb303_params_page.cpp",
    "    case 'a': if (cutoff_knob_) cutoff_knob_->setValue(1); return true;\n"
    "    case 'z': if (cutoff_knob_) cutoff_knob_->setValue(-1); return true;\n"
    "    case 's': if (resonance_knob_) resonance_knob_->setValue(1); return true;\n"
    "    case 'x': if (resonance_knob_) resonance_knob_->setValue(-1); return true;\n"
    "    case 'd': if (env_amount_knob_) env_amount_knob_->setValue(1); return true;\n"
    "    case 'c': if (env_amount_knob_) env_amount_knob_->setValue(-1); return true;\n"
    "    case 'f': if (env_decay_knob_) env_decay_knob_->setValue(1); return true;\n"
    "    case 'v': if (env_decay_knob_) env_decay_knob_->setValue(-1); return true;",
    "    case 'a': if (cutoff_knob_) cutoff_knob_->setValue(directKnobStep); return true;\n"
    "    case 'z': if (cutoff_knob_) cutoff_knob_->setValue(-directKnobStep); return true;\n"
    "    case 's': if (resonance_knob_) resonance_knob_->setValue(directKnobStep); return true;\n"
    "    case 'x': if (resonance_knob_) resonance_knob_->setValue(-directKnobStep); return true;\n"
    "    case 'd': if (env_amount_knob_) env_amount_knob_->setValue(directKnobStep); return true;\n"
    "    case 'c': if (env_amount_knob_) env_amount_knob_->setValue(-directKnobStep); return true;\n"
    "    case 'f': if (env_decay_knob_) env_decay_knob_->setValue(directKnobStep); return true;\n"
    "    case 'v': if (env_decay_knob_) env_decay_knob_->setValue(-directKnobStep); return true;",
)

# ---------------------------------------------------------------------------
# TR-606: advance shared metal oscillators once per rendered sample, independent
# of kick mute/processing. Tame the cymbal band so it is less piercing at 22.05k.
# ---------------------------------------------------------------------------
replace_once(
    "src/dsp/mini_drumvoices.h",
    "  virtual void reset() = 0;\n  virtual void setSampleRate(float sampleRate) = 0;",
    "  virtual void reset() = 0;\n"
    "  virtual void setSampleRate(float sampleRate) = 0;\n"
    "  // Advance engine-wide state exactly once per rendered sample.\n"
    "  virtual void beginSample() {}",
)
replace_once(
    "src/dsp/mini_drumvoices.h",
    "class TR606DrumSynthVoice : public DrumSynthVoice {\npublic:\n"
    "  explicit TR606DrumSynthVoice(float sampleRate);\n\n"
    "  void reset() override;\n  void setSampleRate(float sampleRate) override;",
    "class TR606DrumSynthVoice : public DrumSynthVoice {\npublic:\n"
    "  explicit TR606DrumSynthVoice(float sampleRate);\n\n"
    "  void reset() override;\n  void setSampleRate(float sampleRate) override;\n"
    "  void beginSample() override;",
)
replace_once(
    "src/dsp/mini_drumvoices.cpp",
    "void TR606DrumSynthVoice::triggerKick(bool accent, uint8_t velocity) {",
    "void TR606DrumSynthVoice::beginSample() {\n"
    "  accentEnv *= accentDecay;\n"
    "  updateMetalBank();\n"
    "}\n\n"
    "void TR606DrumSynthVoice::triggerKick(bool accent, uint8_t velocity) {",
)
replace_once(
    "src/dsp/mini_drumvoices.cpp",
    "float TR606DrumSynthVoice::processKick() {\n"
    "  accentEnv *= accentDecay;\n"
    "  updateMetalBank();\n\n"
    "  if (!kickActive)",
    "float TR606DrumSynthVoice::processKick() {\n"
    "  if (!kickActive)",
)
replace_once(
    "src/dsp/mini_drumvoices.cpp",
    "  float clipped = tanhf(metalSignal * 2.2f);\n"
    "  float out = cymbalBandpass.process(clipped) * cymbalEnv;",
    "  float clipped = fast_tanh(metalSignal * 1.6f);\n"
    "  float out = cymbalBandpass.process(clipped) * cymbalEnv * 0.55f;",
)
replace_once(
    "src/dsp/mini_drumvoices.cpp",
    "  float freq = 8000.0f * (1.0f + 0.2f * accent);\n"
    "  float q = 0.9f;",
    "  float freq = 6200.0f * (1.0f + 0.10f * accent);\n"
    "  const float maxFreq = sampleRate * 0.40f;\n"
    "  if (freq > maxFreq) freq = maxFreq;\n"
    "  float q = 0.70f;",
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    "    if (playing) {\n"
    "      if (!muteKick)    drumsMix += drums->processKick()",
    "    if (playing) {\n"
    "      // Engine-wide drum state must not depend on any individual mute.\n"
    "      drums->beginSample();\n"
    "      if (!muteKick)    drumsMix += drums->processKick()",
)

# Label the fixed 606 layout honestly: the generic RS lane carries its cymbal,
# while the generic clap lane is unavailable for this seven-instrument machine.
replace_once(
    "src/ui/components/drum_sequencer_grid.cpp",
    "bool stepHasAccent(const DrumPatternSet& patternSet, int step) {\n"
    "  if (step < 0 || step >= SEQ_STEPS) return false;\n"
    "  for (int v = 0; v < NUM_DRUM_VOICES; ++v) {\n"
    "    if (patternSet.voices[v].steps[step].accent) return true;\n"
    "  }\n"
    "  return false;\n"
    "}\n",
    "bool stepHasAccent(const DrumPatternSet& patternSet, int step) {\n"
    "  if (step < 0 || step >= SEQ_STEPS) return false;\n"
    "  for (int v = 0; v < NUM_DRUM_VOICES; ++v) {\n"
    "    if (patternSet.voices[v].steps[step].accent) return true;\n"
    "  }\n"
    "  return false;\n"
    "}\n\n"
    "const char* drumVoiceLabel(const MiniAcid& miniAcid, int voice) {\n"
    "  static const char* const kDefault[NUM_DRUM_VOICES] =\n"
    "      {\"BD\", \"SD\", \"CH\", \"OH\", \"MT\", \"HT\", \"RS\", \"CP\"};\n"
    "  if (voice < 0 || voice >= NUM_DRUM_VOICES) return \"--\";\n"
    "  if (miniAcid.currentDrumEngineName() == \"606\") {\n"
    "    if (voice == 6) return \"CY\";\n"
    "    if (voice == 7) return \"--\";\n"
    "  }\n"
    "  return kDefault[voice];\n"
    "}\n",
)
replace_all_exact(
    "src/ui/components/drum_sequencer_grid.cpp",
    "  const char* voiceLabels[NUM_DRUM_VOICES] = {\"BD\", \"SD\", \"CH\", \"OH\", \"MT\", \"HT\", \"RS\", \"CP\"};\n",
    "",
    3,
)
replace_all_exact(
    "src/ui/components/drum_sequencer_grid.cpp",
    "voiceLabels[v]",
    "drumVoiceLabel(mini_acid_, v)",
    3,
)

# ---------------------------------------------------------------------------
# PERFORM controls: replace overlapping Fn+letter commands with a local layer.
# Plain Tab opens it; 1..8 operate tools, Shift reverses cyclic adjustments.
# ---------------------------------------------------------------------------
replace_once(
    "src/ui/pages/perform_page.h",
    "private:\n    static const char* noteName(int midiNote);\n\n"
    "    MiniAcid& miniAcid_;",
    "private:\n    static const char* noteName(int midiNote);\n"
    "    bool handleToolKey(const UIEvent& event);\n"
    "    void drawToolsLayer(IGfx& gfx);\n\n"
    "    MiniAcid& miniAcid_;",
)
replace_once(
    "src/ui/pages/perform_page.h",
    "    PerformanceKeyboard& keyboard_;\n    std::string title_{\"PERFORM\"};",
    "    PerformanceKeyboard& keyboard_;\n"
    "    bool toolsLayerVisible_{false};\n"
    "    std::string title_{\"PERFORM\"};",
)

page_path = "src/ui/pages/perform_page.cpp"
page = read(page_path)
start = page.index("bool PerformPage::handleEvent(UIEvent& event) {")
end = page.index("void PerformPage::drawHeader", start)
replacement = r'''bool PerformPage::handleToolKey(const UIEvent& event) {
    const int direction = event.shift ? -1 : 1;
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
            std::snprintf(toast, sizeof(toast), "STRUM: %u MS",
                          static_cast<unsigned>(keyboard_.strumMs()));
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
            std::snprintf(toast, sizeof(toast), "EUCLID ROT: %u",
                          static_cast<unsigned>(keyboard_.euclideanRotation()));
            break;
        default:
            return false;
    }

    UI::showToast(toast, 900);
    return true;
}

void PerformPage::drawToolsLayer(IGfx& gfx) {
    const int leftX = Layout::COL_1;
    const int rightX = Layout::COL_2;
    char value[40];

    gfx.setTextColor(COLOR_ACCENT);
    gfx.drawText(leftX, LayoutManager::lineY(2), "PERFORMANCE TOOLS");

    gfx.setTextColor(COLOR_WHITE);
    std::snprintf(value, sizeof(value), "1 ARP  %s",
                  keyboard_.arpeggiatorEnabled() ? "ON" : "OFF");
    gfx.drawText(leftX, LayoutManager::lineY(3), value);
    std::snprintf(value, sizeof(value), "5 STR  %ums",
                  static_cast<unsigned>(keyboard_.strumMs()));
    gfx.drawText(rightX, LayoutManager::lineY(3), value);

    std::snprintf(value, sizeof(value), "2 DIR  %s", keyboard_.arpDirectionName());
    gfx.drawText(leftX, LayoutManager::lineY(4), value);
    std::snprintf(value, sizeof(value), "6 RAT  x%u",
                  static_cast<unsigned>(keyboard_.ratchetCount()));
    gfx.drawText(rightX, LayoutManager::lineY(4), value);

    std::snprintf(value, sizeof(value), "3 CHD  %s", keyboard_.chordModeName());
    gfx.drawText(leftX, LayoutManager::lineY(5), value);
    std::snprintf(value, sizeof(value), "7 EUC  %u/16",
                  static_cast<unsigned>(keyboard_.euclideanPulses()));
    gfx.drawText(rightX, LayoutManager::lineY(5), value);

    std::snprintf(value, sizeof(value), "4 MEM  %u",
                  static_cast<unsigned>(keyboard_.chordMemorySize()));
    gfx.drawText(leftX, LayoutManager::lineY(6), value);
    std::snprintf(value, sizeof(value), "8 ROT  %u",
                  static_cast<unsigned>(keyboard_.euclideanRotation()));
    gfx.drawText(rightX, LayoutManager::lineY(6), value);

    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(leftX, LayoutManager::lineY(7),
                 "SHIFT+NUMBER: BACK | TAB: CLOSE");
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
                          ? "PERFORMANCE TOOLS: 1-8"
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

'''
write(page_path, page[:start] + replacement + page[end:])

replace_once(
    page_path,
    "    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);\n\n"
    "    const int visualY = LayoutManager::lineY(2);",
    "    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);\n\n"
    "    if (toolsLayerVisible_) {\n"
    "        drawToolsLayer(gfx);\n"
    "        return;\n"
    "    }\n\n"
    "    const int visualY = LayoutManager::lineY(2);",
)
replace_once(
    page_path,
    "USB ONLY | FN+A/C/K/S/R/E/V TOOLS",
    "USB ONLY | TAB PERFORMANCE TOOLS",
)
replace_once(
    page_path,
    "INT+USB | FN+A/C/K/S/R/E/V TOOLS",
    "INT+USB | TAB PERFORMANCE TOOLS",
)
replace_once(
    page_path,
    "void PerformPage::drawFooter(IGfx& gfx) {\n"
    "    UI::drawStandardFooter(gfx,\n"
    "                           \"\\\\ Target  N Note  ,/. Scale\",\n"
    "                           \"Fn A/C/K/S/R/E/V  X Panic\");\n"
    "}",
    "void PerformPage::drawFooter(IGfx& gfx) {\n"
    "    if (toolsLayerVisible_) {\n"
    "        UI::drawStandardFooter(gfx,\n"
    "                               \"1 Arp 2 Dir 3 Chord 4 Memory\",\n"
    "                               \"5 Strum 6 Ratchet 7 Euclid 8 Rotate\");\n"
    "        return;\n"
    "    }\n"
    "    UI::drawStandardFooter(gfx,\n"
    "                           \"\\\\ Target  N Note  ,/. Scale\",\n"
    "                           \"Tab Tools  -/= Oct  X Panic\");\n"
    "}",
)

# ---------------------------------------------------------------------------
# Regression coverage.
# ---------------------------------------------------------------------------
reg_path = "tests/test_wavemorph_performance_source_regressions.py"
reg = read(reg_path)
start = reg.index("def test_performance_tools_use_fixed_control_rate_state()")
end = reg.index("\n\nif __name__ == \"__main__\":", start)
new_tests = r'''def test_performance_tools_use_fixed_control_rate_state() -> None:
    keyboard_header = (ROOT / "src/input/performance_keyboard.h").read_text(encoding="utf-8")
    keyboard_source = (ROOT / "src/input/performance_keyboard.cpp").read_text(encoding="utf-8")
    page_header = (ROOT / "src/ui/pages/perform_page.h").read_text(encoding="utf-8")
    page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")

    require("ScheduledEvent scheduled_[kMaxScheduledEvents]" in keyboard_header and
            "uint8_t generatedNotes_[kMaxGeneratedNotes]" in keyboard_header,
            "performance scheduling must use fixed-size storage")
    require("std::vector" not in keyboard_header and "std::vector" not in keyboard_source,
            "performance scheduling must not allocate")
    require("MusicalEventSource::Arpeggiator" in keyboard_source and
            "router_.route" in keyboard_source,
            "generated notes must use the existing musical event router")
    require("serviceHardwareClock();" in keyboard_source and
            "setTransportPlaying" in keyboard_source,
            "the existing main-loop heartbeat must service generated events")
    require("target_ == MusicalEventTarget::Drums" in keyboard_source,
            "native drum routing must remain outside melodic transforms")

    require("bool toolsLayerVisible_{false};" in page_header,
            "PERFORM must keep tool-layer state local to the page")
    require("event.key == '\\t' || event.scancode == GROOVEPUTER_TAB" in page,
            "plain Tab must open the local performance tool layer")
    tool_block = block(page, "bool PerformPage::handleToolKey", "void PerformPage::drawToolsLayer")
    for key in ("case '1':", "case '2':", "case '3':", "case '4':",
                "case '5':", "case '6':", "case '7':", "case '8':"):
        require(key in tool_block, f"PERFORM tool layer must expose {key}")
    handle_block = block(page, "bool PerformPage::handleEvent", "void PerformPage::drawHeader")
    require("event.meta" in handle_block and "return false" in handle_block,
            "Fn/meta commands must pass through instead of stealing global shortcuts")
    require("Fn A/C/K/S/R/E/V" not in page and "Tab Tools" in page,
            "PERFORM hints must describe the local tool layer, not conflicting Fn keys")
    require("keyboard_.setTempoBpm(miniAcid_.bpm());" in page,
            "arp/ratchet timing must follow the current GroovePuter BPM")


def test_knob_keys_use_coarse_and_fine_steps() -> None:
    page = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")
    require("const int directKnobStep = fine ? kKnobStepFine : kKnobStepCoarse;" in page,
            "direct knob keys must use the existing coarse/fine step policy")
    for expression in ("setValue(directKnobStep)", "setValue(-directKnobStep)"):
        require(page.count(expression) == 4,
                "all four knob key pairs must use the accelerated step")


def test_tr606_shared_clock_is_not_owned_by_kick() -> None:
    header = (ROOT / "src/dsp/mini_drumvoices.h").read_text(encoding="utf-8")
    source = (ROOT / "src/dsp/mini_drumvoices.cpp").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    grid = (ROOT / "src/ui/components/drum_sequencer_grid.cpp").read_text(encoding="utf-8")

    require("virtual void beginSample() {}" in header and
            "void beginSample() override;" in header,
            "drum engines must expose a once-per-sample shared-state hook")
    begin_sample = block(source, "void TR606DrumSynthVoice::beginSample()", "void TR606DrumSynthVoice::triggerKick")
    kick_process = block(source, "float TR606DrumSynthVoice::processKick()", "float TR606DrumSynthVoice::processSnare()")
    require("updateMetalBank();" in begin_sample and "accentEnv *= accentDecay;" in begin_sample,
            "TR-606 metal/accent state must advance in beginSample")
    require("updateMetalBank" not in kick_process,
            "TR-606 hats/cymbal must not depend on processKick")
    require("drums->beginSample();" in engine,
            "the mixer must advance shared drum state before mute-gated voices")
    require("6200.0f" in source and "0.55f" in source and
            "fast_tanh(metalSignal * 1.6f)" in source,
            "TR-606 cymbal must retain the tamed 22.05 kHz profile")
    require('if (voice == 6) return "CY";' in grid and
            'if (voice == 7) return "--";' in grid,
            "the 606 grid must label its actual cymbal and unavailable clap lane")
'''
reg = reg[:start] + new_tests + reg[end:]
reg = reg.replace(
    "    test_performance_tools_use_fixed_control_rate_state()\n"
    "    print(\"wavemorph/performance source regressions: PASS\")",
    "    test_performance_tools_use_fixed_control_rate_state()\n"
    "    test_knob_keys_use_coarse_and_fine_steps()\n"
    "    test_tr606_shared_clock_is_not_owned_by_kick()\n"
    "    print(\"wavemorph/performance source regressions: PASS\")",
)
write(reg_path, reg)

write(
    "tests/test_tr606_drum_voice.cpp",
    r'''#include <cassert>
#include <cmath>
#include <iostream>

#include "src/dsp/mini_drumvoices.h"

namespace {
constexpr float kSampleRate = 22050.0f;

void requireFinite(float sample) {
    assert(std::isfinite(sample));
    assert(std::fabs(sample) < 4.0f);
}

void testHatClockDoesNotRequireKickProcessing() {
    TR606DrumSynthVoice voice(kSampleRate);
    voice.triggerHat(false, 100);

    float energy = 0.0f;
    float motion = 0.0f;
    float previous = 0.0f;
    for (int i = 0; i < 2048; ++i) {
        voice.beginSample();
        const float sample = voice.processHat();
        requireFinite(sample);
        energy += std::fabs(sample);
        motion += std::fabs(sample - previous);
        previous = sample;
    }

    assert(energy > 0.5f);
    assert(motion > 0.5f);
}

void testCymbalLaneIsBoundedWithoutKickProcessing() {
    TR606DrumSynthVoice voice(kSampleRate);
    voice.triggerRim(true, 100);  // Generic RS lane carries 606 CY.

    float energy = 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < 8192; ++i) {
        voice.beginSample();
        const float sample = voice.processRim();
        requireFinite(sample);
        energy += std::fabs(sample);
        peak = std::max(peak, std::fabs(sample));
    }

    assert(energy > 1.0f);
    assert(peak < 1.5f);
}
}  // namespace

int main() {
    testHatClockDoesNotRequireKickProcessing();
    testCymbalLaneIsBoundedWithoutKickProcessing();
    std::cout << "TR-606 drum voice tests: PASS\n";
    return 0;
}
''',
)

run_tests = read("tests/run_host_tests.sh")
append = r'''

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_tr606_drum_voice.cpp" \
  "${ROOT_DIR}/src/dsp/mini_drumvoices.cpp" \
  "${ROOT_DIR}/src/dsp/audio_wavetables.cpp" \
  "${ROOT_DIR}/src/dsp/tube_distortion.cpp" \
  -o "${BUILD_DIR}/test_tr606_drum_voice"

"${BUILD_DIR}/test_tr606_drum_voice"
'''
if "test_tr606_drum_voice.cpp" in run_tests:
    raise RuntimeError("tests/run_host_tests.sh already contains TR-606 test")
write("tests/run_host_tests.sh", run_tests.rstrip() + append + "\n")

write(
    "docs/stages/PERFORM_TOOLS_KNOB_TR606_FIX_STAGE.md",
    r'''# PERFORM tools, knob step, and TR-606 fix

## Purpose

Make the synth parameter knobs faster on Cardputer ADV, remove conflicting
Fn+letter performance shortcuts, and stop the TR-606 metal voices from depending
on the kick processing path.

## Hardware

- M5Stack Cardputer ADV;
- optional headphones or Yamaha SEQTRAK AUDIO IN for checking the 606 cymbal.

## Wiring

No external wiring is required. For external monitoring, connect the Cardputer
audio output to headphones or the SEQTRAK AUDIO IN at a conservative level.

## Build and flash

```bash
git checkout fix/perform-tools-knob-tr606
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
```

Flash the generated Cardputer ADV firmware with the normal project workflow.

## Expected behavior

### Synth knob pages

- `A/Z`, `S/X`, `D/C`, and `F/V` move the four visible knobs by the normal
  coarse step (5 internal parameter steps instead of 1);
- hold Shift for the original fine one-step adjustment;
- arrow-key focus adjustment remains unchanged.

### PERFORM

- plain `Tab` opens or closes **PERFORMANCE TOOLS**;
- while open: `1` arp, `2` direction, `3` chord, `4` memory, `5` strum,
  `6` ratchet, `7` Euclidean pulses, `8` Euclidean rotation;
- Shift reverses cyclic controls;
- playable note keys remain active, so a chord can be held and captured with `4`;
- old Fn+A/C/K/S/R/E/V commands no longer consume global Fn shortcuts.

### TR-606

- hats and cymbal continue correctly when kick is muted;
- the generic `RS` row is shown as `CY` for 606;
- the generic clap row is shown as unavailable (`--`);
- the cymbal is less piercing but remains recognizably metallic.

## Troubleshooting

- If `Tab` changes workflow, verify that Fn is not held; Fn+Tab remains global
  workflow navigation.
- If knob movement is still slow, verify the flashed commit and test A/Z rather
  than pointer drag; pointer drag intentionally remains fine-grained.
- If 606 metal voices stop with the kick mute, confirm that the build contains
  `TR606DrumSynthVoice::beginSample()` and the mixer calls `drums->beginSample()`.
- Disable drum reverb, compression, Lo-Fi, and external input gain when judging
  the raw 606 cymbal tone.

## Acceptance checklist

- [ ] A/Z, S/X, D/C, F/V reach useful knob ranges noticeably faster.
- [ ] Shift still provides fine adjustment.
- [ ] Tab opens a local 1-8 performance tool layer.
- [ ] Fn+M, Fn+Tab, and Fn+[ / ] retain their global behavior.
- [ ] Chord memory can be captured while notes are held.
- [ ] Muting track 3 (kick) does not freeze or alter active 606 hats/cymbal.
- [ ] 606 row 7 is labelled CY and row 8 is labelled --.
- [ ] Host tests, SDL build, and Cardputer ADV build pass.
''',
)

print("Applied PERFORM tools, knob, and TR-606 fix")
