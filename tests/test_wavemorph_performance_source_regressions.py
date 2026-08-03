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
    header = (ROOT / "src/input/performance_keyboard.h").read_text(encoding="utf-8")
    source = (ROOT / "src/input/performance_keyboard.cpp").read_text(encoding="utf-8")
    page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")

    require("ScheduledEvent scheduled_[kMaxScheduledEvents]" in header and
            "uint8_t generatedNotes_[kMaxGeneratedNotes]" in header,
            "performance scheduling must use fixed-size storage")
    require("std::vector" not in header and "std::vector" not in source,
            "performance scheduling must not allocate")
    require("MusicalEventSource::Arpeggiator" in source and
            "router_.route" in source,
            "generated notes must use the existing musical event router")
    require("serviceHardwareClock();" in source and
            "setTransportPlaying" in source,
            "the existing main-loop heartbeat must service generated events")
    require("target_ == MusicalEventTarget::Drums" in source,
            "native drum routing must remain outside melodic transforms")

    for key in ("case 'a':", "case 'c':", "case 'k':", "case 's':",
                "case 'r':", "case 'e':", "case 'v':"):
        require(key in page, f"PERFORM must expose {key} tool control")
    require("if (event.meta)" in page,
            "musical tools must use Fn/meta and not steal playable letters")
    require("keyboard_.setTempoBpm(miniAcid_.bpm());" in page,
            "arp/ratchet timing must follow the current GroovePuter BPM")
    require("Fn A/C/K/S/R/E/V" in page and "Fn+M" not in page,
            "PERFORM help must expose tools without colliding with the launcher")


if __name__ == "__main__":
    test_wavemorph_is_bounded_and_sample_loop_is_lightweight()
    test_engine_catalog_and_legacy_slot_are_stable()
    test_performance_tools_use_fixed_control_rate_state()
    print("wavemorph/performance source regressions: PASS")
