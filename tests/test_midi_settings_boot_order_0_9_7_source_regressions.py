#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
session_cpp = (
    ROOT / "src/platform/cardputer_midi_settings_session.cpp"
).read_text(encoding="utf-8")
usb_transport = (
    ROOT / "src/platform/cardputer_usb_midi_transport.cpp"
).read_text(encoding="utf-8")
usb_output_h = (ROOT / "src/midi/usb_midi_output.h").read_text(encoding="utf-8")
usb_output_cpp = (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")
display_h = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")

bootstrap = "GroovePuterPlatform::initializeCardputerMidiSettingsSession();"
register_usb = "registerCardputerUsbMidiSink("
create_display = "new (std::nothrow) MiniAcidDisplay("

assert '#include "src/platform/cardputer_midi_settings_session.h"' in sketch
assert bootstrap in sketch
assert register_usb in sketch
assert create_display in sketch

setup_start = sketch.index("void setup()")
bootstrap_pos = sketch.index(bootstrap, setup_start)
register_pos = sketch.index(register_usb, setup_start)
display_pos = sketch.index(create_display, setup_start)

assert bootstrap_pos < register_pos, (
    "persisted MIDI settings must initialize before USB dispatcher registration"
)
assert register_pos < display_pos, (
    "USB dispatcher is intentionally started before the heavy UI allocation"
)

# Settings storage stays on the control-side platform unit. The bootstrap may
# call it during setup, but NVS/Preferences must not leak into dispatcher/audio.
assert "Preferences" in session_cpp
assert "if (initialized_) return;" in session_cpp
assert "Preferences" not in sketch
assert "Preferences" not in usb_transport

# The historical UI binding may remain as an idempotent compatibility call in
# R5a, but UI construction is no longer the owner of global settings restore.
assert "CardputerMidiSettingsBinding midi_settings_binding_" in display_h

# R5a is lifecycle only. Physical route projection/binding remains the next
# checkpoint and must not leak into the live USB output here.
assert "midi_output_route_projection.h" not in usb_output_h
assert "midi_output_route_projection.h" not in usb_output_cpp
assert "patternDrumChannel" in usb_output_h
assert "patternDrumChannel" in usb_output_cpp

print("0.9.7-R5a MIDI settings boot-order regressions: PASS")
