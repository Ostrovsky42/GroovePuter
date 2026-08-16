#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
output = (ROOT / "src/input/internal_synth_output.cpp").read_text(encoding="utf-8")
router = (ROOT / "src/input/midi_input_router.h").read_text(encoding="utf-8")

assert "void MiniAcid::liveNoteOn" in engine
assert "playing_) return;" in engine, "0.9.10 R3c freezes STOP-only live Synth monitoring"
assert "engine_.liveNoteOn(voice, internalNote, event.velocity);" in output
assert "engine_.liveNoteOff(voice, internalNote);" in output
assert "MusicalEventSource::MidiInput" in router
assert "miniacid_engine" not in "\n".join(
    line for line in router.splitlines() if line.lstrip().startswith("#include")
), "input core must not learn engine PLAY state"
assert "playing_" not in router, "PLAY ownership stays in the synth engine boundary"
print("0.9.10 R3c STOP-only Synth live-monitoring contract: PASS")
