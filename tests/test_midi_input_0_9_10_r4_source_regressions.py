#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
router = (ROOT / "src/input/midi_input_router.h").read_text(encoding="utf-8")
output = (ROOT / "src/input/internal_synth_output.cpp").read_text(encoding="utf-8")

for token in (
    "MidiInputTarget::Drums",
    "case 36u: logicalLane = 0u",
    "case 38u: logicalLane = 1u",
    "case 42u: logicalLane = 2u",
    "case 46u: logicalLane = 3u",
    "case 43u: logicalLane = 4u",
    "case 47u: logicalLane = 5u",
    "case 37u: logicalLane = 6u",
    "case 39u: logicalLane = 7u",
    "inputChannel",
    "routedChannel",
    "MusicalEventTarget::Drums",
):
    assert token in router, token

includes = "\n".join(line for line in router.splitlines()
                     if line.lstrip().startswith("#include"))
for forbidden in ("output_ownership", "midi_device_profile", "seqtrak", "sampler", "ui/"):
    assert forbidden not in includes, forbidden

assert "isMidiInputSource" in output
assert "event.source == MusicalEventSource::MidiInput" in output
assert "performanceSource &&" in output
assert "GroovePuterOutput::allowsInternalNoteOn(event)" in output
assert "triggerRegisteredLocalDrumVoice(lane, event.velocity);" in output
assert "samplerTrack->triggerPad" in output
print("0.9.10 R4 drum-input ownership/source boundaries: PASS")
