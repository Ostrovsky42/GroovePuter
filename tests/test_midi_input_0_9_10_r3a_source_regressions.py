#!/usr/bin/env python3
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
ROUTER = ROOT / "src/input/midi_input_router.h"
router = ROUTER.read_text(encoding="utf-8")
includes = "\n".join(line for line in router.splitlines() if line.lstrip().startswith("#include"))
for forbidden in ("Arduino", "TinyUSB", "FreeRTOS", "Preferences", "output_ownership", "midi_device_profile", "ui/", "miniacid_engine"):
    assert forbidden not in includes
for token in ("bool enabled{false}", "kMaxActiveNotes = 24u", "kDefaultDrainBudget = 32u", "kSynthNoteMin = 24u", "kSynthNoteMax = 71u", "sourceNote", "routedNote", "MidiInputTransportId", "MidiInputSessionId", "MusicalEventSource::MidiInput", "overflowRecoveries", "Never publish an unowned NoteOn", "MidiInputTarget::SynthA", "MidiInputTarget::SynthB", "sizeof(MidiInputRouter) <= 320u"):
    assert token in router, token
r4 = (ROOT / "docs/releases/0_9_10_R4_DRUM_INPUT.md").exists()
if r4:
    assert "owner.inputChannel == message.channel" in router
    assert "owner.routedChannel" in router
    assert "findResolvedOwner" in router
    assert "MidiInputTarget::Drums" in router
else:
    assert "owner.channel == message.channel" in router
    assert "findRoutedPitchOwner" in router
    assert "MidiInputTarget::Drums" not in router
clamped = (ROOT / "src/dsp/clamped_live_note_identity.h").read_text(encoding="utf-8")
engine = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
assert "kMinNote = 24" in clamped and "kMaxNote = 71" in clamped
assert "kMin303Note = 24" in engine and "kMax303Note = 71" in engine
usb = (ROOT / "src/midi/usb_midi_output.h").read_text(encoding="utf-8") + (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")
assert "MusicalEventSource::MidiInput" not in usb
print("0.9.10 R3a source/ownership boundaries: PASS")
