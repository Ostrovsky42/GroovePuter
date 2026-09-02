#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

projection_h = (ROOT / "src/midi/midi_output_route_projection.h").read_text()
capabilities_h = (ROOT / "src/midi/midi_device_capabilities.h").read_text()
producer_h = (ROOT / "src/dsp/pattern_drum_event_tap.h").read_text()
usb_h = (ROOT / "src/midi/usb_midi_output.h").read_text()
usb_cpp = (ROOT / "src/midi/usb_midi_output.cpp").read_text()

# Pure fixed-size projection only: no allocation, I/O, locks, USB, platform or
# audio dependencies are allowed in the projection model.
for forbidden in (
    "new ", "malloc(", "std::vector", "std::string", "std::mutex",
    "Arduino", "Preferences", "UsbMidiOutput", "TinyUSB",
):
    assert forbidden not in projection_h

assert "std::array<DrumMidiRoute, kMidiDrumVoiceCount> patternDrums" in projection_h
assert "settings.synthAChannel" in projection_h
assert "settings.synthBChannel" in projection_h
assert "projection.patternDrums = settings.drumRoutes" in projection_h
assert "settings.enabled && settings.drumsEnabled && route.enabled" in projection_h
assert "completePerformanceTargetTable{false}" in projection_h

# Vendor behavior is a concrete capability, not inferred from a vague generic
# vendor flag.
assert "enum class MidiReceiverModeControl" in capabilities_h
assert "SeqtrakCc26" in capabilities_h
assert "capabilities.receiverModeControl = MidiReceiverModeControl::SeqtrakCc26" in capabilities_h

# R4 explicitly records, but does not yet rewrite, the historical wire details
# in producers/USB. R5 will consume the projection and remove these duplicate
# physical-route constants with active-note safety.
assert "logicalVoice," in producer_h
assert "        60," in producer_h
assert "patternDrumChannel" in usb_h
assert "patternDrumChannel" in usb_cpp
assert "midi_output_route_projection.h" not in usb_h
assert "midi_output_route_projection.h" not in usb_cpp

print("0.9.7-R4 MIDI route projection source regressions: PASS")
