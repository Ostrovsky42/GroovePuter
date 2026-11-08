# Performance Instrument V1 closure validation contract

This checkpoint closes the two characterized Performance Instrument V1 blockers only.
It does not add V2 features and does not change GF2, generation, Genre, Recipe,
Pattern/Phrase semantics, or external MIDI polyphony.

## Internal mono arbitration

Owner: `InternalSynthOutput`.

Per Synth A/B the arbiter stores only current fixed-size state:

- Pattern ownership bit;
- generated candidate (`Arpeggiator`);
- direct candidate (`PerformanceKeyboard`);
- other-live candidate (`MidiInput`);
- currently projected live candidate.

Priority is:

`PATTERN > PERFORMANCE GENERATED > PERFORMANCE DIRECT > OTHER LIVE`.

There is no displaced-note history stack. A NoteOff clears a candidate only when
its source and note identity match the current candidate. `PerformanceKeyboardPoly`
remains external-MIDI/poly-only.

Pattern playback remains in `MiniAcid` / `AudioTask`; only Pattern ownership is
observed by the arbiter.

## Transport cleanup

A transport epoch transition is ordered as:

`stopGeneratedOutput()` -> update `transportPlaying_` -> `resetPulseClock(true)`.

This discards pending generated events and emits cleanup NoteOff for accepted
generated notes while leaving direct held-key ownership intact.

## Focused proof

- RED: `cbc6bf9321d00a8cf70436fd7636c55073bea983`
- production implementation: `23841fa466401e5f6b8e98a54c2e1f14d040bccb`
- fixture correction uses the real project-timeline ARP path rather than injected events;
- A1-A5 arbitration: required PASS;
- T1-T5 transport cleanup: required PASS;
- held ROOT/SCALE/OCTAVE note identity: required PASS.

Broad host, SDL, Cardputer ADV/fixed-DRAM, and SEQTRAK MIDI-only results are CI
evidence attached to the child PR rather than duplicated as mutable claims here.
