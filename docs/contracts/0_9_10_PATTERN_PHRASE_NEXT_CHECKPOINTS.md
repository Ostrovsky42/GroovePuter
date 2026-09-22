# 0.9.10 PATTERN / PHRASE — Next Checkpoints

Status: **ORDERED CONTRACT PLAN**

Rule:

```text
one PR = one completed contract
```

Do not collapse these checkpoints into one implementation branch.

## P1 — Immutable Runtime Event Projection Contract

**Immediate next production checkpoint after P0.**

Goal: define and implement the immutable/read-only runtime event projection consumed by playback, without introducing persistent PHRASE editing.

Required boundary:

```text
PATTERN -> immutable runtime events -> scheduler/backends
```

This checkpoint may establish the projection shape and PATTERN projection path only. It must preserve current PATTERN playback behavior and existing MIDI ownership, prepare outside the audio callback, and publish through the existing PREPARE/COMMIT/AudioMutationGate architecture.

Not included: persistent PHRASE note storage, MAKE PHRASE, Scene format bump, Phrase Undo payload, cross-bar sustain, GRID/LENGTH editor semantics, UI source switch.

## P2 — Authoritative PHRASE Source / Persistence Ownership

Goal: introduce the separate authoritative PHRASE musical source using explicit `startTick` / `durationTicks`, length 1/2/4/8 bars, and PATTERN xor PHRASE ownership.

This checkpoint owns together because they are one persistent ownership transaction:

- PHRASE persistent data;
- explicit one-way `MAKE PHRASE` ownership transfer;
- hard Scene/project root-format bump at the Scene JSON owner;
- fail-closed handling of unsupported newer Phrase-bearing projects;
- loading existing PATTERN-only projects unchanged;
- canonical `GroovePuterUndo::UndoOwner` payload for Phrase edits/conversion;
- generation rollback extension where Song references PHRASE-owned state, while retaining `UndoKind::Generation` ownership for generated publication.

It must not create a second Undo stack, third musical source, or mirrored Pattern/Phrase synchronization.

## P3 — Cross-bar Physical Lifetime Execution

Goal: make authoritative PHRASE duration/lifetime physically audible across bar boundaries.

One backend-independent logical lifetime decision must drive:

```text
internal synth
USB MIDI
DIN/UART MIDI
```

The checkpoint consumes authoritative PHRASE duration/lifetime. It may reuse the existing `MelodicCrossBarLifetime` decision model where appropriate, but must not resurrect historical PR #396 blindly and must not promote legacy `SynthStep::note == -2` to PHRASE representation.

Acceptance must explicitly close the current Song-boundary MIDI/internal-synth asymmetry for PHRASE semantics without creating a second MIDI lifetime owner.

## P4 — GRID / LENGTH Editing Semantics

Goal: implement the accepted editor axes without changing note identity.

GRID:

```text
1/8  = 48 ticks
1/16 = 24 ticks
1/32 = 12 ticks
```

GRID owns cursor step, snap for newly created/moved material, and editing zoom/window granularity. Changing GRID must not move or reinterpret existing notes.

LENGTH is independently 1/2/4/8 bars and must not be inferred from GRID, FEEL CYCLE, Pattern page, or Song rows.

## P5 — PATTERN / PHRASE UI Exposure

Goal: expose the authoritative source and source-specific editing actions visibly to the musician.

Preserve the existing Synth parent Tab cycle:

```text
NOTES -> KNOBS -> MORE -> NOTES
```

Do not steal plain Tab, do not use a bare single-letter switch, and do not collide with Paste. `Alt+V` is unavailable because it is hard-global GENERATION/GENRE navigation. P0 reserves `Alt+Y` as the current collision-audited candidate for the visible PATTERN/PHRASE source view/action.

This checkpoint must re-run the keymap collision audit before implementation and display the active source explicitly.

## P6 — Hardware Acceptance / Freeze

Goal: hardware-validate the complete production chain on Cardputer ADV after the preceding contracts are independently green.

At minimum validate:

- PATTERN compatibility on Synth A and Synth B;
- PHRASE 1/2/4/8-bar playback;
- cross-bar duration on internal synth;
- byte-consistent logical MIDI behavior across USB and DIN/UART accepting endpoints;
- STOP and source/row transitions;
- MAKE PHRASE persistence + reload;
- one-slot Undo/Redo behavior for manual Phrase edits and generated Phrase publication;
- GRID 8/16/32 editing semantics without retiming existing notes;
- source UI visibility and NOTES/KNOBS/MORE Tab preservation;
- DRAM and target compile gates.

## Dependency order

```text
P0 characterization freeze
  ↓
P1 immutable runtime event projection
  ↓
P2 authoritative PHRASE source + persistence/MAKE PHRASE/Undo
  ↓
P3 physical cross-bar lifetime execution
  ↓
P4 GRID/LENGTH editing semantics
  ↓
P5 PATTERN/PHRASE UI exposure
  ↓
P6 hardware acceptance / freeze
```

If a checkpoint discovers a missing owner or representation contract, stop and split that contract rather than broadening the current PR.
