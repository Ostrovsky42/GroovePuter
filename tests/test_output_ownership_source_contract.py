#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_pattern_drums_split_local_and_midi() -> None:
    text = (ROOT / "src/dsp/pattern_drum_event_tap.h").read_text(encoding="utf-8")
    start = text.index("void triggerPattern")
    end = text.index("void triggerKick", start)
    block = text[start:end]
    local = block.index("GroovePuterOutput::allowsInternal")
    trigger = block.index("triggerLocal")
    publish = block.index("publishPatternDrumTrigger")
    require(local < trigger < publish,
            "Pattern Drums must gate local trigger independently before MIDI publication")
    require("Track::Drums" in block and "SourceClass::Pattern" in block,
            "Pattern Drums must use the canonical Drum owner")


def test_sampler_is_internal_source_layer_not_output_owner() -> None:
    text = (ROOT / "src/sampler/drum_sampler_track.cpp").read_text(encoding="utf-8")
    start = text.index("void DrumSamplerTrack::triggerPad")
    end = text.index("void DrumSamplerTrack::stopPad", start)
    block = text[start:end]
    owner = block.index("GroovePuterOutput::allowsInternal")
    pool = block.index("pool_.trigger")
    require(owner < pool,
            "Sampler local ownership must be decided before starting a sample voice")
    require("Track::Drums" in block,
            "Sampler must inherit Drums output ownership")
    require("setEnabled" not in block and "preload" not in block,
            "OutputMode must not mutate sampler source enable or load state")

    process_start = text.index("void DrumSamplerTrack::process")
    process_block = text[process_start:]
    require("GroovePuterOutput" not in process_block,
            "Sampler process path must not read OutputMode per frame")


def test_perform_drums_reuse_existing_local_owner() -> None:
    cpp = (ROOT / "src/input/internal_synth_output.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/input/internal_synth_output.h").read_text(encoding="utf-8")

    start = cpp.index("if (event.target == MusicalEventTarget::Drums)")
    end = cpp.index("// <=0.9.5 PERFORM", start)
    block = cpp[start:end]
    require("allowsInternalNoteOn(event)" in block,
            "PERFORM Drums NoteOn must use canonical output ownership")
    require("triggerRegisteredLocalDrumVoice" in block,
            "PERFORM Drums must reuse the existing local drum engine")
    require("samplerTrack->triggerPad" in block,
            "PERFORM Drums must preserve the optional internal sampler source layer")
    require("samplerTrack->setEnabled" not in block and "preload" not in block,
            "PERFORM output switching must not unload or reconfigure sampler assets")
    require("liveDrumPadMask_" in header,
            "PERFORM Drum sampler cleanup must be bounded to lanes owned by this sink")
    require("samplerTrack->pad(lane).loop" in block and
            "samplerTrack->stopPad(lane)" in block,
            "looping sample key-up must release only its owned pad")
    require("case MusicalEventType::AllNotesOff" in block,
            "PERFORM Drums must retain explicit sampler panic cleanup")


def test_no_new_routing_framework() -> None:
    files = [
        ROOT / "src/dsp/pattern_drum_event_tap.h",
        ROOT / "src/sampler/drum_sampler_track.cpp",
        ROOT / "src/input/internal_synth_output.cpp",
    ]
    combined = "\n".join(path.read_text(encoding="utf-8") for path in files)
    require("new MusicalEventQueue" not in combined,
            "0.9.6-D must not allocate a second MIDI queue")
    require("UsbMidiOutput" not in combined and "TinyUSB" not in combined,
            "Drum local ownership must stay independent from physical USB routing")


if __name__ == "__main__":
    test_pattern_drums_split_local_and_midi()
    test_sampler_is_internal_source_layer_not_output_owner()
    test_perform_drums_reuse_existing_local_owner()
    test_no_new_routing_framework()
    print("Output ownership source contract: PASS")
