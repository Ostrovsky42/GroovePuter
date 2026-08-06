# Review notes

The implementation deliberately separates numerical tuning policy from voice rendering:

- `chip_tuning.h` contains deterministic frequency mapping and stack ratios;
- AY and SN voices consume those helpers;
- `clamped_live_note_identity.h` contains the NoteOn/NoteOff identity rule;
- `test_chip_tuning.cpp` validates both contracts without audio hardware.

This keeps the PR testable without introducing a synth framework rewrite.
