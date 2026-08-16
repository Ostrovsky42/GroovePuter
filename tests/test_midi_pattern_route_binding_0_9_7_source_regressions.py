#!/usr/bin/env python3
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
startup_h = (ROOT / "src/midi/midi_pattern_startup_routes.h").read_text(encoding="utf-8")
output_h = (ROOT / "src/midi/usb_midi_output.h").read_text(encoding="utf-8")
output_cpp = (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")
session_cpp = (
    ROOT / "src/platform/cardputer_midi_settings_session.cpp"
).read_text(encoding="utf-8")

# The startup snapshot is derived, bounded control state. It must not acquire
# persistence, USB, heap or realtime-task ownership.
assert "MidiPatternStartupRoutes" in startup_h
assert "std::array<DrumMidiRoute, kMidiDrumVoiceCount> drums" in startup_h
for forbidden in ("Preferences", "IUsbMidiTransport", "FreeRTOS"):
    assert forbidden not in startup_h
startup_code = re.sub(r"/\*.*?\*/", "", startup_h, flags=re.S)
startup_code = re.sub(r"//.*", "", startup_code)
assert "malloc(" not in startup_code
assert "calloc(" not in startup_code
assert "realloc(" not in startup_code
assert re.search(r"\bnew\s+(?:\([^)]*\)\s*)?[A-Za-z_:]", startup_code) is None

# Cardputer persistence remains the root settings owner and publishes the
# derived snapshot only after the single MidiDeviceProfileRuntime is initialized.
init_pos = session_cpp.index("profileRuntime.initialize(loadedSettings)")
publish_pos = session_cpp.index("publishMidiPatternStartupRoutes(settings)")
assert init_pos < publish_pos

# UsbMidiOutput consumes the snapshot while lanes are configured for begin().
# Event handling must not resnapshot profile state, otherwise a UI change could
# silently move active NoteOff ownership to a new physical address.
assert "midiPatternStartupRouteRuntime().snapshot(startup)" in output_cpp
configure_start = output_cpp.index("void UsbMidiOutput::configureLanes()")
configure_end = output_cpp.index("uint8_t UsbMidiOutput::clampChannel", configure_start)
assert "snapshot(startup)" in output_cpp[configure_start:configure_end]
handle_start = output_cpp.index("void UsbMidiOutput::handleMusicalEvent")
assert "snapshot(" not in output_cpp[handle_start:]

# Pattern drums own only eight bytes of post-begin physical note identity.
assert "uint8_t patternDrumNotes_[kPatternDrumVoiceCount]" in output_h
assert "MidiOutputRouteProjection" not in output_h
assert "wireNoteFor(*lane, event.note)" in output_cpp

# R5 is Pattern-only. Performance routing and SEQTRAK CC26 remain on the
# established config path for the later explicit Performance checkpoint.
generated_start = output_cpp.index("uint8_t UsbMidiOutput::generatedChannel")
generated_end = output_cpp.index("void UsbMidiOutput::ensurePerformanceReceiverMode", generated_start)
generated_body = output_cpp[generated_start:generated_end]
assert "config_.performanceSynthAChannel" in generated_body
assert "config_.performanceSynthBChannel" in generated_body
assert "config_.performanceDxChannel" in generated_body
assert "startup" not in generated_body

print("0.9.7-R5 Pattern route binding source regressions: PASS")
