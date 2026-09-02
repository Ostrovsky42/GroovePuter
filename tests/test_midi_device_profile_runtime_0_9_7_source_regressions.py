#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

runtime_h = (ROOT / "src/midi/midi_device_profile_runtime.h").read_text()
session_cpp = (ROOT / "src/platform/cardputer_midi_settings_session.cpp").read_text()
usb_h = (ROOT / "src/midi/usb_midi_output.h").read_text()
usb_cpp = (ROOT / "src/midi/usb_midi_output.cpp").read_text()

# R3 owner is bounded control-side state only.
for forbidden in ("new ", "malloc(", "std::mutex", "std::vector", "std::string"):
    assert forbidden not in runtime_h

assert "MidiOutputSettings candidate = settings_;" in runtime_h
assert "applyMidiDeviceProfile(profile, candidate);" in runtime_h
assert "settings_ = candidate;" in runtime_h
assert "++revision_;" in runtime_h
assert "midiTransportCapabilityRuntime().setDeviceProfile(settings_.profile);" in runtime_h

# Cardputer persistence session delegates ownership to the runtime and no longer
# keeps a second long-lived MidiOutputSettings snapshot.
assert 'midi_device_profile_runtime.h' in session_cpp
assert "profileRuntime.initialize(loadedSettings)" in session_cpp
assert "profileRuntime.updateTransportControl" in session_cpp
assert "MidiOutputSettings settings_{}" not in session_cpp
assert "midiTransportCapabilityRuntime().setDeviceProfile" not in session_cpp

# R3 explicitly does not bind profiles into note routing yet. Existing USB lane
# configuration and SEQTRAK-specific hardcoded drum routing remain untouched for
# the later profile->route adapter stage.
assert "midi_device_profile_runtime.h" not in usb_h
assert "midi_device_profile_runtime.h" not in usb_cpp
assert "MidiOutputSettings" not in usb_h
assert "MidiOutputSettings" not in usb_cpp
assert "patternDrumChannel" in usb_h
assert "patternDrumChannel" in usb_cpp

print("0.9.7-R3 device profile runtime source regressions: PASS")
