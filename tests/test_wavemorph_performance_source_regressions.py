#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def block(text: str, start: str, end: str) -> str:
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b]


def test_wavemorph_is_bounded_and_sample_loop_is_lightweight() -> None:
    header = (ROOT / "src/dsp/wavemorph_synth_voice.h").read_text(encoding="utf-8")
    source = (ROOT / "src/dsp/wavemorph_synth_voice.cpp").read_text(encoding="utf-8")
    require("constexpr int kWaveformCount" in header,
            "WAVEMORPH must keep a bounded waveform catalog")
    require("processSample" in source and "sinf" not in block(
        source, "float WaveMorphSynthVoice::processSample", "void WaveMorphSynthVoice::trigger"),
            "WAVEMORPH sample loop must not call sinf")


def test_engine_catalog_and_legacy_slot_are_stable() -> None:
    header = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
    source = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    require("WAVEMORPH" in header and "WAVEMORPH" in source,
            "WAVEMORPH must remain in the synth engine catalog")
    require("kSynthEngineCount" in header,
            "synth engine count must remain explicit")


def test_performance_tools_use_fixed_control_rate_state() -> None:
    header = (ROOT / "src/input/performance_keyboard.h").read_text(encoding="utf-8")
    source = (ROOT / "src/input/performance_keyboard.cpp").read_text(encoding="utf-8")
    require("std::vector" not in header,
            "performance keyboard state must stay fixed-size")
    require("std::vector" not in source,
            "performance tool processing must avoid runtime vectors")


def test_knob_keys_use_coarse_and_fine_steps() -> None:
    page = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")
    require("kCoarseStep" in page and "kFineStep" in page,
            "synth knob key editing must expose coarse/fine steps")


def test_compact_synth_controls_fit_the_cardputer_screen() -> None:
    page = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")
    require("layoutComponents" in page,
            "synth controls must keep explicit compact layout")
    require("distortion_control_->setEnabled(true)" in page and
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
