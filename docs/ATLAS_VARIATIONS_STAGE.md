# Atlas Variations Stage

## Purpose

Expose the already compiled Atlas P1/P2/P3 patterns as an explicit, safe workflow on Cardputer ADV without changing the legacy probabilistic generators and without starting SEQTRAK/MIDI work.

This stage is based on `main` after the successful squash merge of recovery PR #3.

## Non-negotiable compatibility rules

- Legacy recipe IDs 1–5 remain present and keep their current probabilistic behavior.
- Atlas recipe IDs 6–11 remain additive.
- Atlas pattern structure and GroovePuter sound profiles remain separate contracts.
- Sampler events remain explicitly excluded until sample-slot semantics are defined.
- A variation selection must not modify the active pattern before an explicit commit.
- Invalid Atlas data must leave the active scene unchanged.
- Replacing a non-empty user pattern must require an explicit confirmation; no silent overwrite.

## User workflow

```text
Genre -> Recipe -> Variation preview -> Apply mode -> Commit
```

For Atlas recipes the variation selector exposes:

- `P1 BASE`
- `P2 DEVELOPMENT`
- `P3 FILL`, `P3 BREAK`, or `P3 SPACE`, using the Atlas `slotFunction`

Changing recipe or variation updates only preview metadata. The current pattern continues playing unchanged until Apply is committed.

## First vertical slice

The first implementation slice covers all six compiled Atlas recipes:

- Chicago Jack
- Rolling Acid
- Classic 2-Step
- Dark Skippy
- Deep Chord
- Minimal Space

Required behavior:

1. The Genre page displays the selected P1/P2/P3 variation and its slot function.
2. Left/Right changes variation while the Apply area is focused.
3. Existing recipe navigation remains available.
4. Apply passes the selected variation index into the Atlas runtime.
5. The selection is restored from scene state after Save/Load.
6. Applying to non-empty patterns requires a second explicit confirmation.
7. Preview and failed validation never mutate the active pattern.

## Runtime boundaries

### AtlasRuntime

Add a read-only description API that returns metadata for a recipe variation without materializing it into the active scene.

The existing apply path remains transactional:

```text
validate recipe and variation
-> build temporary Synth A / Synth B / Drums
-> commit all three destinations together
```

### MiniAcid

`regeneratePatternsWithGenre` must accept the selected Atlas variation. Legacy recipes ignore the Atlas variation and continue through the current probabilistic path.

### GenrePage

The page owns temporary selection and confirmation UI state. Persisted selection belongs to `GenreSettings`; preview buffers do not.

## Persistence

Add one backward-compatible field to `GenreSettings`:

```cpp
uint8_t atlasVariation = 0;
```

Scene JSON key:

```text
var
```

Missing values load as P1. Values outside the available range are clamped to P1 before use.

## Regression tests

Host tests must prove:

- each Atlas recipe exposes exactly three variations;
- description calls do not modify sentinel patterns;
- P1/P2/P3 have distinct Atlas pattern IDs or slot IDs;
- invalid recipe/variation leaves all destinations unchanged;
- scene round-trip preserves `atlasVariation`;
- legacy recipe IDs 1–5 remain present;
- source regression detects removal of the visible variation selector and overwrite confirmation.

## Hardware acceptance

For each Atlas recipe:

1. Start with a non-empty current pattern.
2. Select P2 or P3 and confirm that playback remains unchanged before Apply.
3. Press Apply once and verify that overwrite confirmation appears.
4. Confirm Apply and verify that all synth/drum tracks switch atomically.
5. Stop/Play and Save/Load the scene.
6. Verify the selected variation, BPM and swing are restored.
7. Repeat the same recipe/variation without reset, stuck notes or accumulating events.

## Out of scope

- constrained stochastic mutation inside P2/P3;
- sampler mapping;
- polyphonic chord playback;
- C.P.S.-inspired Performance UX;
- MIDI importer changes;
- USB-MIDI and SEQTRAK integration;
- verified factory preset IDs.

These remain separate branches after this stage is stable.
