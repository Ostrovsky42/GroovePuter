# Bar-Bound Material Commit Intent

Status: **approved direction / implementation input**  
Parent intent: `docs/intentions/GENERATION_CONSISTENCY_PR_INTENT.md`  
Base: `dev_0.9_test`  
Scope: musical-material staging and deterministic commit at the next real bar boundary

## Purpose

Generation must not mutate musical material in the middle of a sounding bar.

When transport is running, an operation first prepares replacement material off the active playback path, then commits it atomically at the next real `BAR_START` emitted by the audio transport.

Canonical model:

```text
Pattern current
Pattern pending

G / variation / fill / materialization
        |
        v
pending.valid = true
pending.commitAt = NEXT_BAR

... current pattern keeps playing unchanged ...

BAR_START
        |
        v
current <- pending
pending.valid = false
```

User-visible example:

```text
playing: 1A1
press G
screen:  1A1   GEN -> NEXT BAR

next BAR_START
screen:  1A1*
```

The `*` is a transient indication that fresh musical material became active. The exact glyph/lifetime may follow existing UI constraints; the important invariant is that pending versus active material is visible and unambiguous.

## Decision

### BMC-01 — transport owns commit timing

The commit boundary is the existing sequencer/audio `barTick == 0` boundary at 96 PPQN / 384 ticks per 4/4 bar.

The implementation must not add:

- a UI timer;
- a second transport scheduler;
- a `millis()`-based approximation;
- a separate MIDI-clock interpretation.

External/internal clock behavior therefore converges on the same musical boundary.

### BMC-02 — generation is preparation while playing

While transport is running, generation must not write the currently sounding pattern in-place.

The generator writes a complete candidate to pending storage first. Only a successful candidate may set `pending.valid = true`.

A generation failure leaves current material byte-for-byte unchanged.

### BMC-03 — stopped transport commits immediately

When transport is stopped there is no future musical bar boundary to wait for.

Therefore the safe default is:

```text
transport stopped -> prepare -> validate -> commit immediately
transport playing -> prepare -> validate -> NEXT_BAR
```

This preserves fast editing while stopped and prevents mid-bar mutations during playback.

### BMC-04 — one atomic musical transaction may contain several lanes

Pending material is a transaction, not one pointer per UI page.

A transaction may contain any subset of:

- Synth A;
- Synth B;
- Drums;
- Song pattern assignment/materialization metadata;
- later Phrase/section/archetype material.

If several compatible generation actions are staged during the same bar, their lane payloads may merge into one transaction. Re-generating the same lane replaces only that pending lane with the newest candidate.

A coordinated multi-track operation such as SONG double-`G` must remain one transaction and commit all requested lanes together.

### BMC-05 — no partial commit

At `BAR_START`, either every lane/reference in a pending transaction is still valid and the whole transaction commits, or none of it does.

In particular Song copy-on-write materialization must revalidate reserved destinations immediately before commit.

If a destination became occupied/referenced, commit is cancelled rather than partially applying a row.

### BMC-06 — page identity is captured

Pattern storage is page-scoped. Pending material therefore captures the page/bank/slot identity it was prepared for.

A pending write must never be redirected to the same bank/slot number on another pattern page.

For the first release-safe implementation, a page mismatch at commit time cancels the pending transaction. It must not silently mutate another page.

### BMC-07 — newest intent wins per lane

Repeated generation before the boundary is legal.

Example:

```text
bar is playing
G -> pending Synth A candidate #1
G -> pending Synth A candidate #2
BAR_START -> candidate #2 becomes current
```

For different compatible lanes:

```text
G on Synth A -> pending A
G on Drums   -> pending A + Drums
BAR_START    -> both commit atomically
```

This also provides the correct foundation for SONG double-`G`: the provisional single-cell generation never has to be rolled back from active Scene state if it never committed.

### BMC-08 — active Scene revision changes at commit, not at stage

Staging pending material is not an active Scene mutation.

Therefore:

- staging must not increment Scene revision;
- cancelling pending material must not increment Scene revision;
- successful immediate commit increments revision once;
- successful NEXT_BAR transaction increments revision once regardless of lane count.

### BMC-09 — audio thread does not generate

Heavy generation remains on the control/UI side under the existing audio mutation gate.

The audio `BAR_START` path performs only bounded validation/copy/activation work. It must not run Atlas generation, procedural generation, filesystem I/O, allocation-heavy work, or UI rendering.

### BMC-10 — no transport pause is required

The user should be able to press generation/variation/fill controls during playback without pausing transport.

The current bar continues with current material. The new material starts cleanly on the next bar.

### BMC-11 — pending status is visible

When a playing-transport operation stages successfully, the relevant screen should expose a compact state such as:

```text
GEN -> NEXT BAR
VAR -> NEXT BAR
FILL -> NEXT BAR
```

The status is not a promise based on elapsed wall-clock time; it reflects an actual pending transaction.

### BMC-12 — operation vocabulary

The same commit mechanism is intended to support these musical-material operations:

```text
GENERATION
VARIATION
PHRASE
FILL
SECTION
SONG_MATERIALIZATION
RHYTHM_ARCHETYPE
```

This list defines transaction semantics, not keyboard assignments.

In particular, current `O`/`P` bindings must not be overwritten merely to create variation shortcuts. SONG `P` already owns playhead navigation. A future O/P variation mapping must be specified separately and then use this same commit path.

## Initial implementation slice

The first implementation should stay narrow and prove the mechanism before Phrase/section architecture is moved onto it.

Required first slice:

1. shared allocation-free pending transaction state;
2. staging from Synth A/B `G`;
3. staging from Drums `G`;
4. staging from Drums `Ctrl+G` focused-voice generation;
5. staging from Drums `Alt+G` CHAOS generation;
6. real audio `BAR_START` commit using the existing transport boundary;
7. immediate commit while transport is stopped;
8. active Genre/Recipe realization remains mandatory;
9. Atlas-backed local generation uses Atlas-first realization with procedural fallback;
10. persistent UI feedback `GEN -> NEXT BAR` / `CHAOS -> NEXT BAR` while a transaction is pending;
11. host/source regressions plus Cardputer ADV acceptance documentation.

Second slice after the mechanism is proven:

- SONG single-cell / row / area materialization;
- GENERATION page materialization;
- Phrase variation/fill;
- section changes;
- future RhythmArchetype replacement.

The split is deliberate: SONG copy-on-write needs destination reservation/revalidation and must not be forced into the first patch by weakening its existing atomic safety.

## Runtime budget

Pending state is fixed-size and allocation-free after boot.

The `BAR_START` commit path should be bounded by copies of at most:

- two `SynthPattern` values;
- one `DrumPatternSet` value;
- small target/status metadata.

No heap allocation is required for the initial slice.

## Hardware assumptions

Target hardware: M5Stack Cardputer ADV / ESP32-S3 using the current Arduino/M5Cardputer profile.

No external wiring is required. This change does not alter PORT.A, I2C, I2S, SD, USB MIDI, ES8311, or GPIO ownership.

## Required host invariants

1. A playing-transport Synth `G` does not modify the active pattern before `BAR_START`.
2. A playing-transport Drum `G` does not modify the active pattern before `BAR_START`.
3. Focused-voice generation preserves all other pending/current drum voices.
4. Staging sets pending valid only after a complete candidate exists.
5. A second generation of the same lane before commit replaces that lane candidate.
6. Compatible different lanes may coexist in one pending transaction.
7. `BAR_START` is the only playing-transport activation point.
8. Stopped transport commits immediately.
9. Page mismatch cancels rather than writes into another page.
10. Stage/cancel does not increment Scene revision.
11. Successful transaction increments Scene revision exactly once.
12. Atlas-backed generation remains Atlas-first.
13. Non-Atlas generation falls back to the existing procedural path.
14. Audio `BAR_START` performs no generation or filesystem I/O.
15. No new O/P keyboard meaning is introduced by the commit mechanism itself.

## Acceptance checklist

- [ ] Current material remains audible and unchanged for the rest of the current bar after pressing `G`.
- [ ] `GEN -> NEXT BAR` is visible while pending.
- [ ] New material starts exactly on the next bar boundary.
- [ ] Repeated `G` before the boundary activates only the newest candidate for that lane.
- [ ] Synth A, Synth B and Drums can be staged without stopping playback.
- [ ] Focused drum generation changes only that voice at commit.
- [ ] CHAOS is still labelled separately from normal genre-safe generation.
- [ ] Stopped transport applies generation immediately.
- [ ] Changing pattern page before commit cannot corrupt another page.
- [ ] Scene revision changes only when material actually becomes active.
- [ ] No watchdog reset, allocation failure, audio deadlock, or new sustained underrun growth appears during repeated staging.
- [ ] Song copy-on-write semantics remain unchanged until the dedicated second integration slice lands.
