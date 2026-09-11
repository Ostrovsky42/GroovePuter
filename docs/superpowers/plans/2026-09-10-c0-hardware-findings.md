# 0.9.11 C0 Hardware Findings Repair

Base hardware-tested SHA: `56e96c893cb0bf5790c91f24e87b206f3224a471`

Repair branch: `fix/20260910-0.9.11-c0-hardware-findings`

## Goal

Close the three findings observed on Cardputer ADV without admitting A2, GF2, nanoKEY2 M2, Song orchestration, or a new persistent slot identity model.

## Invariants

1. Phrase remains the owner of Phrase events. Generating a Phrase must not modify the Pattern merely as an intermediate representation.
2. A refused LENGTH shrink must preserve every event and report the musical reason, not the generic `LENGTH UNCHANGED`.
3. Plain `G` keeps the musician-level meaning GENERATE in Synth NOTES. Phrase GRID is secondary and moves behind `Alt+G`.
4. Phrase generation is a single RuntimePhrase undo transaction and preserves the current Phrase extent.
5. Q/W/E/R/T/Y/U/I must not be advertised as a persistent Phrase Bank until the bank can bind to the stable Material identity contract. `PhraseBankState` alone is not a product surface.
6. No silent truncation, no Pattern mutation, no storage fallback, no allocations added to the audio path.

## Slice H1 — truthful LENGTH refusal

RED:
- a 2-bar Phrase containing an onset in bar 2 is asked to shrink to 1 bar;
- preflight returns `WouldTruncateEvent`;
- live Phrase is unchanged;
- UI has a dedicated refusal message naming the boundary.

GREEN:
- add a detailed `PhraseInstrumentControls` outcome that preserves the existing `RuntimePhraseEdit::LengthEditResult` reason;
- Synth Phrase NOTES maps `WouldTruncateEvent` to `NOTES AFTER BAR <target>`;
- successful expansion/shrink keeps the existing commit path.

## Slice H2 — direct Synth Phrase generation

RED:
- Phrase NOTES has no generation command because the Pattern generator is gated by `!phraseNotes` and plain `G` currently cycles GRID;
- generated material must be projected directly from an in-memory generated `SynthPattern` into a `RuntimeSynthEventBuffer`;
- Pattern state is never used as the commit destination.

GREEN:
- add a pure fixed-capacity Phrase generation projection helper;
- preserve the current Phrase length and repeat the generated one-bar idea over that extent;
- plain `G` generates when stopped; `Alt+G` cycles GRID;
- commit through `commitRuntimePhraseEditWithUndo()` so Phrase ownership and one-step Undo remain intact;
- show explicit failure/status toast; do not report success for an empty/unchanged candidate.

## Slice H3 — Phrase Bank reachability

The imported P1 bank is only key mapping + per-voice EDIT selection state. It has no binding to runtime PLAY/material identity and no UI owner. Wiring it now to `(voice, slot)` would resurrect coordinate-as-identity immediately before A2 establishes stable `MaterialId`/resolution semantics.

Disposition for this repair: **DEFER persistent QWERTYUI Bank surface to the Material admission.** Keep the tiny state/helper as an evidence donor, but do not expose a UI that suggests eight persistent playable Phrase identities when the storage/identity contract is not yet true.

The next Material checkpoint must either bind Q/W/E/R/T/Y/U/I to stable Material IDs or explicitly remove the provisional PhraseBank state.

## Verification

On one exact repair SHA:

1. focused H1/H2 tests;
2. existing Phrase/Pattern/P3/P3-U1 regressions;
3. full `tests/run_host_tests.sh`;
4. Cardputer ADV build;
5. fixed DRAM gate;
6. firmware artifact + SHA-256 provenance;
7. second physical hardware smoke before C0 is declared closed.
