# 0.9.9 — Synth Song Follow Foundation

## Purpose

Keep SYNTH A/B `NOTES` views aligned with the pattern material that Song playback is actually using.

Contract:

- During Song PLAY, the NOTES view follows the current Song pattern address.
- The visible global pattern page, bank, pattern slot `1..8`, and note material follow Song row changes.
- After STOP, the last resolved page/bank/slot remains selected so the user can edit the material at the stop location immediately.
- The UI is a follower only. Song playback remains the owner of audible pattern selection and async pattern-page residency.

This stage does not change generation, audio scheduling, Song lifecycle, Undo ownership, or persistent pattern semantics.

## Ownership

`MiniAcid::applySongPositionSelection()` remains authoritative for Song position, global pattern IDs, requested resident page, and Synth A/B bank/slot selection.

`PatternEditPage::syncRuntimePatternSelection()` only mirrors that resolved runtime address into editor cursor state. It does not request page switches or mutate persistent state.

## Pattern addressing

One bank contains exactly 8 visible pattern slots. One global pattern page contains two banks, so it currently contains 16 global pattern addresses.

The repository currently defines `kMaxPages = 16`; this foundation does not hard-code an eight-page global limit. The `1..8` contract is the visible pattern slots inside a bank.

Canonical conversion remains `PatternAddress` / `patternAddressFromGlobal()`.

## Async page residency rule

The NOTES editor adopts a Song address only after the target global pattern page is resident:

```text
address.valid()
AND
address.page == currentPageIndex()
```

`MiniAcidDisplay::handlePaging_()` remains the UI-thread pattern-page loading owner.

## STOP behavior

`MiniAcid::stop()` stops transport without disabling Song mode or forcing a page reset. The last Song runtime page/bank/slot therefore remains resident and is mirrored into the NOTES editor.

Explicitly leaving Song mode is outside this stage. Existing `setSongMode(false)` behavior may restore the pre-Song Pattern-mode selection; changing that is a separate UX/lifecycle decision.

## Current residency limitation

There is only one resident global pattern page at a time. `applySongPositionSelection()` currently chooses the row page from the first available pattern in this priority:

```text
Synth A -> Synth B -> Drums
```

A Song row that references Synth A and Synth B patterns from different global pages cannot make both pages resident simultaneously. This foundation deliberately does not introduce a second page cache or residency owner.

For hardware acceptance, keep all non-empty pattern tracks in one Song row on the same global page. Cross-page mixed rows require a separate design.

## Hardware list

- M5Stack Cardputer-Adv running GroovePuter.
- USB cable for firmware update and serial monitoring.
- Normal GroovePuter audio or MIDI output path.

No additional external hardware is required.

## Wiring

No special wiring. Use the normal USB connection used for firmware update and serial monitoring. This test does not use PORT.A or external I2C devices.

## Build / flash steps

Run the source contract:

```bash
bash tests/run_0_9_9_synth_song_follow_tests.sh
```

Then run the normal repository regression/build matrix and update the Cardputer-Adv with the resulting firmware using the existing project procedure.

## Expected behavior

Prepare a Song with several consecutive rows whose Synth A/B assignments differ. Include at least two pattern slots, a bank A/B change, a global page change, and different note contents.

During PLAY on `SYNTH A -> NOTES` or `SYNTH B -> NOTES`:

- note material changes when the Song row changes;
- selected pattern slot `1..8` follows the Song assignment;
- bank indicator follows A/B;
- title `P<n>` follows the resident global page;
- step playhead continues normally.

Press STOP away from the original edit location. The title, bank, selected slot, and note grid must remain on the stopped Song material. Editing a note must target that stopped pattern.

Switch NOTES -> KNOBS/MORE -> NOTES during PLAY or immediately after STOP. NOTES must return to the current/stopped Song selection, not the pre-PLAY editor cursor.

## Troubleshooting

### Title changes but bank/pattern selection is stale

Verify the branch contains `PatternEditPage::syncRuntimePatternSelection()` and `SynthSequencerPage::tick()` invokes it before the NOTES-only tick gate.

### Page title lags briefly at a row boundary

Pattern pages are loaded asynchronously. The follower intentionally waits for the requested page to become resident. Persistent lag or repeated loading indicates a paging problem; page loading must not be moved into the editor.

### Synth A follows but Synth B shows unexpected material

Check whether A and B reference different global pages on the same Song row. That is outside the current single-page residency contract.

### An old pattern returns only after explicitly leaving Song mode

That is the existing Pattern-mode snapshot restore in `setSongMode(false)` and is outside this foundation.

## Acceptance checklist

- [ ] `bash tests/run_0_9_9_synth_song_follow_tests.sh` prints `PASS`.
- [ ] Normal host regressions pass.
- [ ] SDL build passes.
- [ ] Cardputer-Adv build passes.
- [ ] Song PLAY updates SYNTH A NOTES at row boundaries.
- [ ] Song PLAY updates SYNTH B NOTES at row boundaries.
- [ ] Pattern slot `1..8` follows the Song assignment.
- [ ] Bank A/B follows the Song assignment.
- [ ] Global page `P<n>` follows Song paging.
- [ ] STOP preserves the last page/bank/slot and note grid.
- [ ] A note edited after STOP changes the stopped pattern, not the pre-PLAY pattern.
- [ ] NOTES -> KNOBS/MORE -> NOTES does not restore the pre-PLAY cursor.
- [ ] Serial log shows no page-switch loop or repeated page-load failure.
