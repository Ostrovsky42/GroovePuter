# Bar-Bound Material Commit — Cardputer ADV Acceptance

## Purpose

Verify that generated musical material is prepared while a bar is playing and becomes active only on the next real transport bar boundary.

Initial implementation scope:

- Synth A `G`;
- Synth B `G`;
- Drums `G`;
- Drums `Ctrl+G` focused voice;
- Drums `Alt+G` CHAOS.

SONG/Phrase/section/RhythmArchetype staging is intentionally not part of this first hardware slice.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3;
- USB-C cable for power, flash, and Serial;
- optional headphones/speaker path normally used for GroovePuter.

## Wiring

No external wiring is required.

This test does not change PORT.A, I2C, I2S, SD, USB MIDI, or GPIO ownership.

## Build / flash

From the repository root:

```bash
./tests/run_host_tests.sh
./scripts/build.sh
./scripts/upload.sh
```

Build assumptions are the repository defaults: Cardputer/M5Cardputer target, PSRAM disabled, `huge_app`, native USB CDC/MIDI enabled.

## Expected behavior

### 1. Synth generation while playing

1. Open Synth A PATTERN.
2. Select a clearly audible pattern such as `1A1`.
3. Start transport.
4. Wait until the playhead is visibly inside the bar, not on step 1.
5. Press `G`.

Expected:

```text
GEN -> NEXT BAR
CURRENT KEEPS PLAYING
```

The remainder of the current bar must use the old pattern. The replacement starts on the following bar boundary without transport stop/restart.

Repeat on Synth B.

### 2. Repeated G before the boundary

1. During one playing bar, press `G` twice before the next bar begins.
2. Keep transport running.

Expected: the active pattern remains unchanged during the current bar. Only pending material changes. Exactly one activation occurs at the next bar boundary; there is no mid-bar first-generation flash.

### 3. Full drum generation

1. Open DRUMS sequencer.
2. Start transport and press plain `G` mid-bar.

Expected: current drums finish the bar unchanged, `GEN -> NEXT BAR` is shown, and the new drum material starts on the next bar.

### 4. Focused drum voice

1. Put the cursor on one drum voice with several easily recognised hits.
2. Press `Ctrl+G` mid-bar.

Expected: no immediate audible change. At the next bar, only the focused voice is regenerated; all other drum voices retain their material.

### 5. CHAOS

1. Press `Alt+G` mid-bar.

Expected:

```text
CHAOS -> NEXT BAR
```

The current bar remains intact. CHAOS becomes active only at the next bar. CHAOS remains a distinct non-strict generation operation; it is not presented as normal genre-safe `GEN`.

### 6. Stopped transport

1. Stop transport.
2. Press `G` on Synth or Drums.

Expected: generation commits immediately and the UI reports `GEN COMMITTED`. There is no permanently pending `NEXT BAR` state while stopped.

### 7. Pattern-page safety

1. Start transport.
2. Press `G` mid-bar so `GEN -> NEXT BAR` is visible.
3. Before the next bar, change to another pattern page.

Expected: pending generation is cancelled rather than written into the numerically matching bank/slot on the new page. No unrelated pattern is modified.

### 8. Genre/Recipe consistency

Run at least these reviewed recipe families:

- Chicago Jack;
- Classic 2-Step;
- Deep Chord.

For each, compare local Synth/Drum `G` with Genre/SONG materialization. Local generation must use Atlas material when the recipe has an Atlas runtime entry and the existing procedural Genre/Recipe path otherwise.

## Serial validation

Useful expected messages when debug logging is enabled:

```text
[MaterialCommit] staged action=GEN ... -> NEXT_BAR
[MaterialCommit] committed action=GEN ... at BAR_START
```

For page mismatch:

```text
[MaterialCommit] cancelled: page changed before BAR_START
```

There must not be repeated commit messages for one pending action.

## Troubleshooting

- **Material changes immediately while transport is playing:** confirm the active page is using the new public G router and not falling through to the retained legacy `randomize*()` handler.
- **Pending never commits:** verify transport is advancing and the engine reaches `barTick == 0`; do not add a UI timer as a workaround.
- **Wrong page changed:** treat as a release blocker. Pending page identity must be checked before Scene bank writes.
- **Audio crack/pop or underrun spike at every bar:** inspect the BAR_START commit path. It may copy prepared material only; generation, filesystem I/O, allocation, and rendering do not belong there.
- **Atlas recipe always sounds procedural:** verify `AtlasRuntime::applyRecipe()` succeeds before procedural fallback.
- **Scene becomes dirty as soon as G is pressed:** staging must not mark Scene mutated. Revision changes only on immediate commit or successful BAR_START activation.

## Acceptance checklist

- [ ] Host regression suite passes.
- [ ] Cardputer ADV normal DRAM build passes.
- [ ] Synth A `G` does not alter the remainder of the current bar.
- [ ] Synth B `G` does not alter the remainder of the current bar.
- [ ] Drums `G` commits on the next bar only.
- [ ] `Ctrl+G` changes only the focused drum voice at commit.
- [ ] `Alt+G` displays CHAOS and commits on the next bar only.
- [ ] Stopped transport commits immediately.
- [ ] Repeated G before BAR_START causes one activation, not intermediate active mutations.
- [ ] Page change before BAR_START cannot corrupt another page.
- [ ] Chicago Jack uses the reviewed Atlas path locally.
- [ ] Classic 2-Step uses the reviewed Atlas path locally.
- [ ] Deep Chord uses the reviewed Atlas path locally.
- [ ] No watchdog reset, deadlock, allocation failure, or new sustained underrun growth during repeated generation.
- [ ] Existing SONG copy-on-write generation still behaves exactly as before in this first slice.
