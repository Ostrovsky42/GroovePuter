#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

settings_h = (ROOT / "src/platform/cardputer_midi_settings_session.h").read_text()
settings_cpp = (ROOT / "src/platform/cardputer_midi_settings_session.cpp").read_text()
project_h = (ROOT / "src/ui/pages/project_page.h").read_text()
project_cpp = (ROOT / "src/ui/pages/project_page.cpp").read_text()
transport_cpp = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text()

assert "pendingCardputerMidiOutputEnabled" in settings_h
assert "selectCardputerMidiOutputEnabledForNextBoot" in settings_h
assert "pendingOutputEnabled_" in settings_cpp
assert "composeNextBootMidiOutputSettings" in settings_cpp
assert "persistence_.save(candidate)" in settings_cpp
assert "pendingOutputEnabled_" in settings_cpp

assert "midiDeviceProfileRuntime().settings().enabled" in transport_cpp
assert "g_output.setEnabled(" in transport_cpp
assert "!g_output.enabled()" in transport_cpp

assert "MidiDevice, MidiOutput, MidiInputEnabled" in project_h
assert "midi_output_preview_" in project_h
assert "pendingCardputerMidiOutputEnabled()" in project_cpp
assert "selectCardputerMidiOutputEnabledForNextBoot" in project_cpp
assert "MidiOutputModeUi::outputModeName" in project_cpp

print("MIDI output destination source regressions: PASS")
