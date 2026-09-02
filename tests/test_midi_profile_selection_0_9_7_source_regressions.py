#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "src/platform/cardputer_midi_settings_session.h").read_text(encoding="utf-8")
source = (ROOT / "src/platform/cardputer_midi_settings_session.cpp").read_text(encoding="utf-8")
transport_source = (ROOT / "src/midi/transport_clock_source.h").read_text(encoding="utf-8")

assert "pendingCardputerMidiDeviceProfile" in header
assert "selectCardputerMidiDeviceProfileForNextBoot" in header
assert "cardputerMidiDeviceProfileRestartRequired" in header

# R7 makes midi_companion_settings.h reachable from the UI/platform header. The
# Arduino sketch preprocessor can then see transport_clock_source.h through two
# textual include paths, so a real macro guard is required in addition to
# pragma once.
assert "#ifndef GROOVEPUTER_TRANSPORT_CLOCK_SOURCE_H" in transport_source
assert "#define GROOVEPUTER_TRANSPORT_CLOCK_SOURCE_H" in transport_source
assert "#endif  // GROOVEPUTER_TRANSPORT_CLOCK_SOURCE_H" in transport_source

select_start = source.index("bool selectProfileForNextBoot")
select_end = source.index("bool persist(", select_start)
select_body = source[select_start:select_end]
assert "applyMidiDeviceProfile(profile, candidate)" in select_body
assert "persistence_.save(candidate)" in select_body
assert "profileRuntime.applyProfile" not in select_body
assert ".applyProfile(" not in select_body
assert "publishMidiPatternStartupRoutes" not in select_body
save_pos = select_body.index("persistence_.save(candidate)")
pending_pos = select_body.index("pendingProfile_ = profile")
assert save_pos < pending_pos

# The frozen USB startup snapshot is published exactly once during initialize.
assert source.count("publishMidiPatternStartupRoutes(settings)") == 1
init_start = source.index("void initialize()")
init_end = source.index("MidiDeviceProfile pendingProfile()", init_start)
assert "publishMidiPatternStartupRoutes(settings)" in source[init_start:init_end]

# Later transport saves must preserve pending next-boot profile intent.
persist_start = source.index("bool persist(")
persist_end = source.index("private:", persist_start)
persist_body = source[persist_start:persist_end]
assert "profileRuntime.updateTransportControl(source, externalFollowEnabled)" in persist_body
assert "if (pendingProfile_ != record.profile)" in persist_body
assert "applyMidiDeviceProfile(pendingProfile_, record)" in persist_body
assert "persistence_.save(record)" in persist_body
assert persist_body.index("applyMidiDeviceProfile(pendingProfile_, record)") < persist_body.index("persistence_.save(record)")

# R7 adds only one tiny pending enum to the Cardputer session; it does not add a
# second MidiOutputSettings snapshot or another route runtime.
member_tail = source[source.index("CardputerMidiSettingsStorage storage_"):source.index("CardputerMidiSettingsSession& settingsSession")]
assert "MidiDeviceProfile pendingProfile_" in member_tail
assert "MidiOutputSettings" not in member_tail
assert "MidiPatternStartupRouteRuntime" not in source

print("0.9.7-R7 next-boot profile selection source regressions: PASS")
