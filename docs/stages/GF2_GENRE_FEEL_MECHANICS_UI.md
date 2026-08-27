# 0.9.9-GF2 — Genre / Feel mechanics UI scaffold

## Purpose

Reinterpret the existing two-page GENERATE workflow without replacing Genre/Feel and without adding a second musical-policy owner.

This checkpoint exposes only mechanisms that already have authoritative state on the current base:

- `GENRE` — identity/corridor selection.
- `RECIPE` — existing genre vocabulary recipe.
- `RHYTHM` — existing AUTO/fixed rhythm archetype selection.
- `DEPTH` — existing session P1/P2/P3 generation level.
- `FEEL` — existing timing/velocity realization controls.
- `FEEL CYCLE` — existing `scene.feel.patternBars` value, limited to 1/2/4/8 bars.

`FEEL CYCLE` is deliberately not phrase length. It is the UI framing for the existing `scene.feel.patternBars` owner only. The authoritative logical phrase-length concept belongs to the Phrase/composition lineage (`requestedPhraseBars` there), and GF2 must not mirror or own it.

`ACTIVITY` is deliberately absent. No cadence/activity production owner is present on this base, so GF2 must not ship a no-op UI control or duplicate state.

Production musical policy delta: **0**.
Scene ABI delta: **0**.
Persistence schema delta: **0**.
New fixed runtime state: **0**.

## Scaffold status

GF2 is UI design evidence, naming evidence, and an ownership guard. It is not a future production-integration ancestor.

When the current Phrase lineage reaches its integration point, re-apply the proven UI decisions from the then-current production ancestor. Do not merge or mechanically stack this old GF2 branch into that lineage.

The proven mental model is:

```text
GENRE   WHAT LANGUAGE
RECIPE  WHICH VOCABULARY
RHYTHM  WHICH CANONICAL IDENTITY
DEPTH   HOW FAR          (UI interpretation of the existing P1/P2/P3 owner)

FEEL    HOW IT FEELS
```

Future temporal/activity controls are intentionally not frozen here.

## Hardware list

- M5Stack Cardputer ADV.
- USB-C data cable for build/flash and serial monitoring.
- Optional Yamaha SEQTRAK only for normal downstream MIDI listening; it is not required for the UI contract test.

## Wiring

No external wiring is required.

GF2 does not touch PORT.A, I2C, SPI display ownership, USB-MIDI routing, or audio hardware.

## Build / flash

```bash
git fetch origin
git checkout agent/20260826-01-0.9.9-gf2-ui-scaffold

python3 tests/test_four_axis_ui_source_regressions.py
./tests/run_host_tests.sh
./scripts/build_cardputer_adv.sh
```

Flash with the repository's normal Cardputer ADV procedure after the firmware build succeeds.

## Expected behavior

GENERATE remains exactly two pages:

```text
GENRE -> FEEL
```

### GENRE

The page exposes:

```text
GENRE
RECIPE
RHYTHM
DEPTH
APPLY
```

`DEPTH` displays the same P1/P2/P3 session state already controlled by the `P` shortcut. Left/Right while `DEPTH` is focused changes that same owner; it does not create a page-local copy.

`DEPTH` is only a UI interpretation of the existing P1/P2/P3 state. GF2 does not redefine the production semantics frozen by the corresponding realization/mutation contracts.

The existing generation/materialization path, recipe selection, rhythm selection, apply modes, Undo ownership, and quantized commit path remain unchanged.

### FEEL

The historical `REPEATS` owner is presented as:

```text
FEEL CYCLE  1/2/4/8
```

Focused explanation:

```text
FEEL WINDOW: 1/2/4/8 bars
```

It still writes only `scene.feel.patternBars`. It is not presented as phrase length and does not read/write `requestedPhraseBars` or any other Phrase/composition length owner.

The `P` shortcut is described consistently as `DEPTH` on both GENERATE pages.

### Reserved future axes

There is no editable `ACTIVITY` row in GF2. Activity becomes UI-visible only after a production cadence owner is merged and has a stable contract.

GF2 also does not freeze placement of future Activity, final phrase controls, final APPLY/publication semantics, transport-loop visualization, storage preflight, or semantic-bar visualization.

## Troubleshooting

### DEPTH changes with `P` but not with Left/Right

Treat this as a GF2 failure. The focused `DEPTH` row and the `P` shortcut must call the same `GroovePuterState` generation-level owner.

### DEPTH value differs between GENRE and FEEL

Treat this as a duplicate-state regression. There must be one P1/P2/P3 session selector.

### FEEL CYCLE changes something other than `scene.feel.patternBars`

Treat this as an ownership failure. GF2 must not introduce a second temporal or phrase-length field.

### FEEL shows PHRASE LENGTH or PHRASE/CYCLE

Treat this as a semantic ownership failure. Phrase length belongs to the Phrase/composition lineage; the GF2 FEEL surface owns only the existing feel-cycle window.

### ACTIVITY appears as an editable control

Treat this as a failure on this checkpoint. The UI must not expose an owner that does not yet exist on the base.

### GENERATE has more than two pages

Treat this as a workflow regression. Historical GENERATION/TEXTURE persisted IDs remain compatibility aliases only and normalize to FEEL.

## Acceptance checklist

- [ ] `python3 tests/test_four_axis_ui_source_regressions.py` passes.
- [ ] Core host tests pass.
- [ ] Cardputer ADV firmware builds with the normal memory gates.
- [ ] GENERATE navigation is still exactly `GENRE -> FEEL`.
- [ ] GENRE shows `RECIPE`, not a second variation/lifecycle owner.
- [ ] GENRE shows a focusable `DEPTH` row.
- [ ] Left/Right on `DEPTH` and plain `P` operate the same P1/P2/P3 state.
- [ ] FEEL shows `FEEL CYCLE` with 1/2/4/8 values.
- [ ] `FEEL CYCLE` still edits only `scene.feel.patternBars`.
- [ ] FEEL explanation is `FEEL WINDOW: 1/2/4/8 bars`.
- [ ] FEEL does not expose `PHRASE LENGTH`, `PHRASE/CYCLE`, or `requestedPhraseBars`.
- [ ] No `ACTIVITY` control exists on this checkpoint.
- [ ] No `VariationProfile`, new Scene field, persistence field, or fixed global state is added.
- [ ] Existing generation, Undo, quantized publication, timing, velocity, and transport behavior remain unchanged.

After these checks, treat GF2 as **UI SCAFFOLD COMPLETE**. Further temporal/activity/product-integration work belongs on a new branch from the then-current production ancestor, not on GF2.
