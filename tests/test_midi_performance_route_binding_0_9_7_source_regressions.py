#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
projection_h = (ROOT / "src/midi/midi_performance_route_projection.h").read_text(encoding="utf-8")
startup_h = (ROOT / "src/midi/midi_pattern_startup_routes.h").read_text(encoding="utf-8")
output_h = (ROOT / "src/midi/usb_midi_output.h").read_text(encoding="utf-8")
output_cpp = (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")

assert "kPerformanceDrumLaneCount = 7" in projection_h
assert "settings.profile == MidiDeviceProfile::Custom" in projection_h
assert "projection.complete = false" in projection_h
assert "MidiDeviceProfile::SeqtrakNative" in projection_h
assert "MidiDeviceProfile::GeneralMidi" in projection_h
assert "MidiDeviceProfile::GenericMidi" in projection_h

# R6 extends the existing single startup snapshot; it must not add a second
# global runtime owner for Performance routing.
assert "performanceRoutesComplete" in startup_h
assert "performanceDrums" in startup_h
assert startup_h.count("static MidiPatternStartupRouteRuntime runtime") == 1
assert "MidiPerformanceStartupRouteRuntime" not in startup_h

# Only bounded physical-note identity is retained after begin().
assert "uint8_t performanceDrumNotes_[kSeqtrakDrumLaneCount]" in output_h
assert "bool performanceStartupRoutesComplete_" in output_h
assert "bool seqtrakReceiverModeControl_" in output_h
assert "MidiPerformanceRouteProjection" not in output_h

# Profile state is consumed only while lanes are configured. The realtime event
# path uses frozen channels/notes and never asks the control runtime again.
configure_start = output_cpp.index("void UsbMidiOutput::configureLanes()")
configure_end = output_cpp.index("uint8_t UsbMidiOutput::clampChannel", configure_start)
assert "performanceRoutesComplete" in output_cpp[configure_start:configure_end]
handle_start = output_cpp.index("void UsbMidiOutput::handleMusicalEvent")
assert "midiPatternStartupRouteRuntime" not in output_cpp[handle_start:]
assert "projectMidiPerformanceRoutes" not in output_cpp[handle_start:]

# SEQTRAK CC26 is capability-gated; GM/Generic/Custom must not inherit the
# historical vendor parameter merely by using UsbMidiOutput.
receiver_start = output_cpp.index("void UsbMidiOutput::ensurePerformanceReceiverMode")
receiver_end = output_cpp.index("bool UsbMidiOutput::generatedNoteActive", receiver_start)
receiver_body = output_cpp[receiver_start:receiver_end]
assert "if (!seqtrakReceiverModeControl_)" in receiver_body
assert "kSeqtrakMonoPolyController" in receiver_body

print("0.9.7-R6 Performance route binding source regressions: PASS")
