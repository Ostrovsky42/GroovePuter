#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

settings_h = (ROOT / "src/midi/midi_companion_settings.h").read_text()
settings_cpp = (ROOT / "src/midi/midi_companion_settings.cpp").read_text()
transport_h = (ROOT / "src/midi/midi_transport_capabilities.h").read_text()
codec_h = (ROOT / "src/midi/midi_companion_settings_codec.h").read_text()
session_cpp = (ROOT / "src/platform/cardputer_midi_settings_session.cpp").read_text()
capabilities_h = (ROOT / "src/midi/midi_device_capabilities.h").read_text()

assert "GenericMidi = 3" in settings_h
assert 'return "GENERAL MIDI";' in settings_cpp
assert 'return "GENERIC MIDI";' in settings_cpp
assert "genericMidiDrumRoutes" in settings_cpp
assert "drumRoute.enabled = false" in settings_cpp

assert "conservativeGenericMidiTransportCapabilities" in transport_h
generic_body = transport_h.split(
    "constexpr MidiTransportCapabilities conservativeGenericMidiTransportCapabilities()",
    1,
)[1].split("// Yamaha SEQTRAK", 1)[0]
assert "capabilities.clockTx = true" in generic_body
assert "capabilities.startTx = true" in generic_body
assert "capabilities.stopTx = true" in generic_body
assert "capabilities.continueTx = true" not in generic_body
assert "capabilities.songPositionTx = true" not in generic_body
assert "capabilities.clockRx = true" not in generic_body

assert "MidiDrumMappingKind::SeqtrakNative" in capabilities_h
assert "MidiDrumMappingKind::GeneralMidiPercussion" in capabilities_h
assert "MidiDrumMappingKind::None" in capabilities_h
assert "MidiDrumMappingKind::UserDefined" in capabilities_h

# R2 is an API/capability checkpoint only. Persisted record shape and the
# Cardputer runtime selection path intentionally stay untouched.
assert "static constexpr uint16_t kSchemaVersion = 2;" in codec_h
assert "static constexpr std::size_t kPayloadSize = 34;" in codec_h
assert "GenericMidi" not in session_cpp
assert "applyMidiDeviceProfile(" not in session_cpp

print("0.9.7-R2 device capability source regressions: PASS")
