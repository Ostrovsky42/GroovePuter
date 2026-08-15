#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

settings_h = (ROOT / "src/midi/midi_companion_settings.h").read_text()
settings_cpp = (ROOT / "src/midi/midi_companion_settings.cpp").read_text()
transport_h = (ROOT / "src/midi/midi_transport_capabilities.h").read_text()
session_cpp = (ROOT / "src/platform/cardputer_midi_settings_session.cpp").read_text()

# Persisted profile identity is compatibility-sensitive in schema v2.
assert "SeqtrakNative = 0" in settings_h
assert "GeneralMidi = 1" in settings_h
assert "Custom = 2" in settings_h

# The existing implementation already owns concrete SEQTRAK defaults.
assert "seqtrakNativeDrumRoutes" in settings_cpp
assert "settings.liveChannel = 7" in settings_cpp
assert "settings.synthAChannel = 7" in settings_cpp
assert "settings.synthBChannel = 8" in settings_cpp

# Record the current semantic conflation explicitly: GeneralMidi provides a GM
# percussion map while transport capabilities treat the same profile as generic
# class-compliant MIDI. Production 0.9.7 must resolve this intentionally.
assert "generalMidiDrumRoutes" in settings_cpp
assert "kGeneralMidiDrumChannel = 9" in settings_cpp
assert "case MidiDeviceProfile::GeneralMidi:" in transport_h
assert "return genericClassCompliantTransportCapabilities();" in transport_h

# SEQTRAK transport claims remain hardware-evidence based rather than assuming
# all generic MIDI realtime messages are supported.
assert "seqtrakValidatedTransportCapabilities" in transport_h
assert "capabilities.continueRx = true" in transport_h
assert "capabilities.continueBehavior = MidiContinueBehavior::RestartFromBeginning" in transport_h

# Current Cardputer boot consumes a persisted profile for transport capability
# selection, but there is no runtime profile-selection/apply path here yet.
assert "midiTransportCapabilityRuntime().setDeviceProfile" in session_cpp
assert "applyMidiDeviceProfile(" not in session_cpp

print("0.9.7-R1 MIDI device-profile source contract: PASS")
