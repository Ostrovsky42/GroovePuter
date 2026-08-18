#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def block(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def test_wavemorph_is_bounded_and_sample_loop_is_lightweight() -> None:
    header = (ROOT / "src/dsp/wave_morph_synth_voice.h").read_text(encoding="utf-8")
    source = (ROOT / "src/dsp/wave_morph_synth_voice.cpp").read_text(encoding="utf-8")
    process = block(source, "float WaveMorphSynthVoice::process()", "void WaveMorphSynthVoice::setParameterNormalized")

    require("kTableSize = 128" in header and "kTableCount = 8" in header,
            "WaveMorph must retain the fixed 8x128 wavetable budget")
    require("std::vector" not in header and "new " not in source and "malloc" not in source,
            "WaveMorph must remain allocation-free")
    for expensive in ("std::sin", "sinf(", "std::exp", "expf(", "std::pow", "powf("):
        require(expensive not in process,
                f"WaveMorph process() must not call {expensive}")
    require("std::clamp(dcBlocked, -1.0f, 1.0f)" in process,
            "WaveMorph output must remain explicitly bounded")
    require("updateEnvelopeCoefficients" in source and
            "if (index == 5) updateEnvelopeCoefficients();" in source,
            "decay coefficients must update outside the sample loop")


def test_engine_catalog_and_legacy_slot_are_stable() -> None:
    header = (ROOT / "src/dsp/swappable_synth_voice.h").read_text(encoding="utf-8")
    source = (ROOT / "src/dsp/swappable_synth_voice.cpp").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    makefile = (ROOT / "platform_sdl/Makefile").read_text(encoding="utf-8")

    require("OPL2      = 3" in header and "SH101     = 4" in header and
            "SN76489   = 5" in header and "WAVEMORPH = 6" in header,
            "persisted synth enum values must not be renumbered")
    require("case SynthEngineType::WAVEMORPH" in source and
            "std::make_unique<WaveMorphSynthVoice>" in source,
            "the swappable voice must instantiate WaveMorph")
    require("case SynthEngineType::OPL2" in source and
            "return SynthEngineType::TB303" in source,
            "legacy OPL2 state must continue to normalize to TB303")
    require('return {"TB303", "SID", "AY", "SH101", "SN76489", "WAVEMORPH"};' in engine,
            "MiniAcid must expose the complete safe synth catalog")
    require('targetName = "WAVEMORPH";' in engine and
            "SynthEngineType::WAVEMORPH" in engine,
            "MiniAcid must route WaveMorph selection to its enum slot")
    require("wave_morph_synth_voice.cpp" in makefile,
            "SDL build must link WaveMorph")
    require(not (ROOT / "src/dsp/opl2_synth_voice.cpp").exists() and
            not (ROOT / "src/dsp/opl2_synth_voice.h").exists(),
            "the removed OPL2 implementation must not return")


def test_performance_tools_use_fixed_control_rate_state() -> None:
    keyboard_header = (ROOT / "src/input/performance_keyboard.h").read_text(encoding="utf-8")
    keyboard_source = (ROOT / "src/input/performance_keyboard.cpp").read_text(encoding="utf-8")
    page_header = (ROOT / "src/ui/pages/perform_page.h").read_text(encoding="utf-8")
    page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")
    usb_header = (ROOT / "src/midi/usb_midi_output.h").read_text(encoding="utf-8")
    usb_source = (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")
    usb_test = (ROOT / "tests/test_usb_midi_output.cpp").read_text(encoding="utf-8")

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
    require("event.key == '\t' || event.scancode == GROOVEPUTER_TAB" in page,
            "plain Tab must open the local performance tool layer")
    tool_block = block(page, "bool PerformPage::handleToolKey", "void PerformPage::drawToolsLayer")
    for key in ("case '1':", "case '2':", "case '3':", "case '4':",
                "case '5':", "case '6':", "case '7':", "case '8':"):
        require(key in tool_block, f"PERFORM tool layer must expose {key}")
    require("captureHeldPerformanceKeys(keyboard_)" in tool_block and
            "restoreHeldPerformanceKeys(keyboard_, heldSnapshot)" in tool_block,
            "tool changes must rehydrate physically held notes after legacy panic-based setters")
    require("keyboard.heldCount() != 0" in page and
            "keyboard.isPhysicalKeyHeld(key)" in page,
            "revoice must be bounded and avoid duplicate logical key ownership")
    require("new " not in page and "std::vector" not in page,
            "held-note revoice must remain fixed-allocation")
    handle_block = block(page, "bool PerformPage::handleEvent", "void PerformPage::drawHeader")
    require("event.meta" in handle_block and "return false" in handle_block,
            "Fn/meta commands must pass through instead of stealing global shortcuts")
    require("Fn A/C/K/S/R/E/V" not in page and "Tab Tools" in page,
            "PERFORM hints must describe the local tool layer, not conflicting Fn keys")
    require("keyboard_.setTempoBpm(miniAcid_.bpm());" in page,
            "arp/ratchet timing must follow the current GroovePuter BPM")
    for label in ("1 ARPEGGIATOR", "2 DIRECTION", "3 CHORD", "4 MEMORY",
                  "5 STRUM", "6 RATCHET", "7 EUCLIDEAN", "8 ROTATE"):
        require(label in page, f"PERFORM tools must show the full label {label}")
    for abbreviated in ("1 ARP  ", "2 DIR  ", "3 CHD  ", "4 MEM  ",
                        "5 STR  ", "6 RAT  ", "7 EUC  ", "8 ROT  "):
        require(abbreviated not in page,
                f"PERFORM tools must not regress to abbreviated label {abbreviated}")

    require("kGeneratedBitsetBytes" in usb_header and
            "generatedActive_" in usb_header and
            "generatedPendingRelease_" in usb_header,
            "generated performance MIDI must use bounded fixed-size polyphonic state")
    performance_sources = block(
        usb_source,
        "bool UsbMidiOutput::isSynthPerformanceSource",
        "bool UsbMidiOutput::sourceRequestsPolyReceiver")
    for source_name in ("PerformanceKeyboard", "PerformanceKeyboardPoly", "Arpeggiator"):
        require(f"MusicalEventSource::{source_name}" in performance_sources,
                f"USB MIDI performance-source domain must include {source_name}")
    require("isSynthPerformanceSource(event.source)" in usb_source and
            "acquireGeneratedNote" in usb_source and
            "releaseGeneratedNote" in usb_source and
            "releaseGeneratedTarget" in usb_source,
            "Arpeggiator/Chord/Strum/Ratchet/Euclidean events must reach USB MIDI")
    require("isSynthPerformanceSource(source)" in usb_source and
            "releaseGeneratedTarget(target);" in usb_source,
            "the shared performance panic domain must clean generated MIDI too")
    require("testPerformanceTransformMidiPolyphony" in usb_test and
            "MusicalEventSource::MidiInput" in usb_test,
            "host tests must prove generated polyphony while raw MidiInput stays closed")


def test_knob_keys_use_coarse_and_fine_steps() -> None:
    page = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")
    require("const int directKnobStep = fine ? kKnobStepFine : kKnobStepCoarse;" in page,
            "direct knob keys must use the existing coarse/fine step policy")
    for expression in ("setValue(directKnobStep)", "setValue(-directKnobStep)"):
        require(page.count(expression) == 4,
                "all four knob key pairs must use the accelerated step")


def test_compact_synth_controls_fit_the_cardputer_screen() -> None:
    header = (ROOT / "src/ui/pages/tb303_params_page.h").read_text(encoding="utf-8")
    page = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")
    synth_page = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(
        encoding="utf-8"
    )

    require("kMainKnobRadius = 18" in page and "kMainKnobRadius = 13" not in page,
            "MAIN must spend the freed vertical budget on radius-18 performance knobs")
    require("knobRowY = content.y + kMainKnobCenterY" in page and
            "keyY = content.y + kMainKeyHintY" in page and
            "const int y = content.y + kMainSummaryY" in page,
            "the compact knob layout must use its explicit vertical budget")
    require("kMainSummaryY + kMainSummaryH <=" in page and
            "synth summary must stay above the performance HUD" in page,
            "summary must remain above the global performance HUD")
    require('"[N]KM"' in synth_page and '"N[K]M"' in synth_page and
            '"NK[M]"' in synth_page,
            "SynthSequencerPage must own the single visible NOTES/KNOBS/MORE switcher")
    require("drawTabSwitcher" not in page and '"TAB >"' not in page,
            "params page must not duplicate the parent tab affordance")
    require("const Rect contentRect{content.x, content.y, content.w, content.h}" in page and
            "drawMainSummary(gfx, contentRect)" in page,
            "summary must receive explicit content geometry")
    require("UIInput::isTab(ui_event)" in synth_page and
            "UIInput::isTab(ui_event)" not in page,
            "plain Tab must be owned once by the parent three-state cycle")
    require("main_focus_slot_" in header and "more_focus_slot_" in header and
            "rememberFocusedSlot" in page and "restoreFocusedSlot" in page,
            "Knobs and More must remember focus independently")
    require(page.count("LabelValueComponent::Style::Stepper") == 3,
            "TYPE/OSC/FLT must use full-row steppers")
    require(page.count("LabelValueComponent::Style::Toggle") == 2,
            "DST/DLY must use full-row switches")
    require("if (focused)" in page and
            "gfx.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, focus_color_)" in page,
            "the active MORE row must use a filled focus state")
    require("kMoreRowHeight = 13" in page and "LabelValueComponent* rows[5]" in page and
            "synth MORE rows must stay above the performance HUD" in page,
            "MORE must use five stable full-width rows above the HUD")
    require("setEnabled(oscAvailable)" in page and
            "setEnabled(filterAvailable)" in page and
            'setValue("--")' in page,
            "unavailable engine parameters must remain visible but disabled")

    disabled_render = block(page,
                            "    if (!enabled_) {",
                            "    if (style_ == Style::Stepper)")
    require('gfx.textWidth("--")' in disabled_render and "return;" in disabled_render and
            "drawRect" not in disabled_render and "fillCircle" not in disabled_render,
            "an unavailable row must render only a dim marker, never a switch or arrows")
    toggle_render = block(page,
                          "    constexpr int kTrackWidth = 30;",
                          "\n  }\n\n private:")
    require("gfx.drawRect(trackX, trackY, kTrackWidth, kTrackHeight, switchColor)" in toggle_render and
            "trackX + 5" in toggle_render and "gfx.fillCircle" in toggle_render,
            "an available OFF toggle must retain its outlined track and left thumb")
    require("Unavailable is distinct from an available OFF toggle" in page,
            "the unavailable/OFF visual distinction must remain documented beside the renderer")

    navigation = block(page,
                       "  const int nav = UIInput::navCode(ui_event);",
                       "  char key = ui_event.key;")
    more_navigation = block(navigation,
                            "  if (more_tab_) {",
                            "  } else {")
    require("case GROOVEPUTER_UP:" in more_navigation and
            "focusPrev();" in more_navigation and
            "case GROOVEPUTER_DOWN:" in more_navigation and
            "focusNext();" in more_navigation and
            "case GROOVEPUTER_LEFT:" in more_navigation and
            "adjustFocusedElement(-1, fine);" in more_navigation and
            "case GROOVEPUTER_RIGHT:" in more_navigation and
            "adjustFocusedElement(1, fine);" in more_navigation,
            "MORE must use Up/Down for rows and Left/Right for values")
    require('"[TAB]N [U/D]ROW [L/R]CHANGE"' in page,
            "the MORE footer must advertise the physical arrow mapping")

    require("Both effects are per-voice post-engine stages" in page and
            "distortion_control_->setEnabled(true)" in page and
            "delay_control_->setEnabled(true)" in page,
            "current DST/DLY rows must remain available for every synth engine")
    require("const char* keyHints[4]" in page and
            "if (more_tab_ && !ui_event.ctrl" in page,
            "direct A/Z-S/X-D/C-F/V controls must be advertised and active only on MAIN")
    require("new " not in block(page,
                                "void TB303ParamsPage::layoutComponents()",
                                "void TB303ParamsPage::adjustFocusedElement"),
            "tab layout must not add explicit heap allocation during draw")


def test_tr606_shared_clock_is_not_owned_by_kick() -> None:
    header = (ROOT / "src/dsp/mini_drumvoices.h").read_text(encoding="utf-8")
    source = (ROOT / "src/dsp/mini_drumvoices.cpp").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    wrapper = (ROOT / "src/dsp/pattern_drum_event_tap.h").read_text(encoding="utf-8")
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
    require("void beginSample() { if (voice_) voice_->beginSample(); }" in wrapper,
            "the pattern-publishing drum decorator must forward shared sample state")
    require("6200.0f" in source and "0.55f" in source and
            "fast_tanh(metalSignal * 1.6f)" in source,
            "TR-606 cymbal must retain the tamed 22.05 kHz profile")
    require('if (voice == 6) return "CYM";' in grid and
            'if (voice == 7) return "---";' in grid,
            "the 606 grid must label its actual cymbal and unavailable clap lane")


if __name__ == "__main__":
    test_wavemorph_is_bounded_and_sample_loop_is_lightweight()
    test_engine_catalog_and_legacy_slot_are_stable()
    test_performance_tools_use_fixed_control_rate_state()
    test_knob_keys_use_coarse_and_fine_steps()
    test_compact_synth_controls_fit_the_cardputer_screen()
    test_tr606_shared_clock_is_not_owned_by_kick()
    print("wavemorph/performance source regressions: PASS")