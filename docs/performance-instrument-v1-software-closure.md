# Performance Instrument V1 — software closure

This document records the frozen scope of the final software-closure checkpoint for PR #431.

## Scope

- Production ownership/arbitration implementation is frozen at commit `23841fa466401e5f6b8e98a54c2e1f14d040bccb`.
- `InternalSynthOutput` is the authoritative owner for internal live-mono arbitration.
- Arbitration priority remains: Pattern > Performance Generated > Performance Direct > Other Live.
- External/poly Performance ownership is outside the internal mono cleanup model.
- No GF2, generation semantics, Genre/Recipe, Pattern/Phrase, UI, MIDI source model, Pattern scheduler, or Stage15 tonal changes belong to this checkpoint.
- Stage15 `PerformanceKeyboard::intervalForDegree` failures are inherited and out of scope.

## Required cleanup behavior

The focused closure regression must prove through the production `InternalSynthOutput` path that target-scoped `AllNotesOff`:

1. clears the authoritative candidate for the selected Synth target;
2. clears/releases the selected target's projected live note;
3. leaves the other Synth target untouched;
4. does not absorb `PerformanceKeyboardPoly` into internal mono ownership; and
5. prevents a Performance candidate suppressed by Pattern ownership from resurrecting after Pattern ownership ends.

The historical MiniAcid live-note mirror is not an ownership oracle and must not be restored to satisfy tests.

## Final validation

The final software verdict is intentionally not embedded in this document. It must come from fresh exact-head GitHub Actions evidence attached to PR #431: focused closure plus Host, SDL, Cardputer ADV/fixed-DRAM, and SEQTRAK MIDI-only validation.
