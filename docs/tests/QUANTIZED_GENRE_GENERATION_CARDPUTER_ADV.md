# Quantized Genre Generation — Cardputer ADV

## Purpose

Verify that full GENRE materialization never replaces a sounding pattern in the middle of a bar and never pauses the renderer merely to prepare the next pattern.

While transport is running, `GENRE -> G` prepares the complete Stage 15 A+B+Drums candidate in unpublished fixed-size scratch storage while AudioTask keeps rendering the current material. The candidate commits at the next real `BAR_START`. With transport stopped, generation remains immediate.

This first recovery slice is for the current pattern target. Song/Phrase row materialization remains a separate transaction problem: if playback changes page/bank/slot while a candidate is pending, this slice cancels the candidate rather than redirecting it.

## Hardware list

- M5Stack Cardputer ADV (ESP32-S3)
- USB-C cable for power/flash/Serial
- Optional Yamaha SEQTRAK for MIDI smoke only

No external PORT.A wiring is required. This test does not change I2C, I2S, USB-MIDI, SD, GPIO, or audio codec wiring.

## Wiring

None beyond normal Cardputer ADV USB-C power/flash.

PORT.A remains unchanged: GPIO2 SDA / GPIO1 SCL if other project tests use it.

## Build / Flash

```bash
git checkout agent/20260812-quantized-generation-commit
git rev-parse HEAD
./scripts/build_cardputer_adv.sh
```

Flash the normal Cardputer ADV artifact using the repository's normal 0.9.1 flashing procedure. Do not use an older `dev_0.9` image: the historical #159 implementation is not the code under test here.

## Expected behavior

### Screen

When transport is playing and full generation is requested:

```text
GEN -> NEXT BAR
```

The active status line also carries `NEXT` while material is pending.

After the next bar starts, `NEXT` disappears and the selected Genre/Variant becomes active.

### Audio

The remainder of the current bar must stay on the old material. Synth A, Synth B, drums, Genre state, mode, BPM/swing (when `MATERIALIZE+BPM` is used), and generated Stage 15 material change together at the next bar start.

There must be no transport stop/restart, AudioMutationGate renderer pause during PLAY preparation, mid-bar silence, half-old/half-new pattern, or manual timing requirement.

### Runtime boundary

PLAY preparation uses a fixed two-slot `Writing -> Ready -> Reading -> Empty` handoff. The control side may generate only into its private unpublished slot. `BAR_START` may only claim a complete Ready slot and copy the prepared transaction.

Repeated `G` before the boundary replaces the still-Ready pending transaction when BAR_START has not started reading it. If the keypress races the tiny BAR_START publish window, the request may fail as `Busy`; the audio thread must never wait, spin, or read a slot being written.

### Serial

No repeated generation should occur from the audio-thread BAR_START hook. The hook commits only the already prepared fixed-size transaction; it must not fall through to `regeneratePatternsWithGenre()`.

## Test procedure

1. Select a clearly audible one-bar groove and set BPM to 160–180.
2. Start transport and let it loop for several bars.
3. On GENRE, change Genre or Variant and press plain `G` around step 5.
4. Confirm there is no click, silence, transport reset, or audible pause at the moment `G` is pressed.
5. Confirm the old bar continues unchanged through step 16.
6. Confirm the complete new groove starts on step 1 of the next bar.
7. Repeat at approximately steps 9 and 13.
8. During one bar press `G` twice before the boundary. Confirm only the newest successfully staged transaction becomes active at the next bar.
9. Repeat `G` very close to step 16/step 1. A boundary-race request may report failure/Busy, but playback must remain uninterrupted and the already claimed transaction must remain coherent.
10. While a generation is pending, change pattern page/bank/slot before the boundary. Confirm the pending transaction is cancelled rather than written into the wrong target.
11. Stop transport and press `G`. Confirm generation applies immediately with no `NEXT BAR` wait.
12. Set APPLY to `MATERIALIZE+BPM`, start playback, select a profile with a clearly different suggested BPM, and press Enter. Confirm BPM/material change together at the next bar rather than changing the length of the current bar.
13. Run for at least 10 minutes while repeatedly changing Genre/Variant and generating. Confirm no watchdog reset, stuck note, crackle burst, or sustained underrun growth.

## Troubleshooting

- **Audio cuts as soon as G is pressed:** verify the PLAY branch in `GenrePage::applyCurrent()` calls `regenerateWithQuantizedCommit()` outside `withAudioGuard()`. A transport stop/start fix alone is insufficient because a long AudioMutationGate section can still pause AudioTask.
- **New pattern starts immediately while PLAY is active:** verify `regenerateWithQuantizedCommit()` sees `engine.isPlaying() == true` and publishes a Ready slot instead of using the immediate STOP path.
- **Nothing changes at the boundary:** verify the existing sequencer path reaches `barTick == 0` and `GenreSceneView::commitPendingRecipe()` invokes the installed bounded commit hook.
- **GEN FAILED very close to the boundary:** one request may lose the intentional non-blocking race against BAR_START. Confirm playback and any already-claimed transaction remain correct; AudioTask must never wait for the UI writer.
- **Wrong pattern changes:** treat as a blocker. Exact page + A/B/drum bank/slot targets must still match at commit time; otherwise pending must cancel.
- **Song row advance cancels pending:** expected in this first current-pattern slice when the exact target changes. Song/Phrase materialization requires its own destination reservation and `NextPhrase` decision and is tracked separately.
- **BPM changes mid-bar in MATERIALIZE+BPM:** treat as a blocker. BPM is part of the pending full-generation transaction.
- **BAR_START causes a CPU/audio spike:** the audio boundary must only claim/copy prepared material and apply bounded mode/BPM state. Atlas/procedural/Stage 15 generation belongs to control-side scratch preparation.

## Acceptance checklist

- [ ] PLAY + GENRE `G` never stops/restarts transport.
- [ ] PLAY candidate preparation does not enter AudioMutationGate / pause AudioTask.
- [ ] Current bar remains musically unchanged after `G` until its end.
- [ ] No click or silence is audible when `G` starts candidate preparation.
- [ ] Screen shows `GEN -> NEXT BAR` while pending.
- [ ] A+B+Drums activate together at the next step-0/bar boundary.
- [ ] Selected Genre/Variant/Rhythm becomes active at the same boundary.
- [ ] `MATERIALIZE+BPM` changes BPM/swing at the same boundary.
- [ ] Repeated `G` before the boundary uses the newest successfully staged candidate.
- [ ] A boundary race never blocks AudioTask or tears the transaction.
- [ ] Changing page/bank/slot before commit cannot mutate the wrong target.
- [ ] STOP + `G` commits immediately.
- [ ] No heavy generation runs from the audio BAR_START callback.
- [ ] Song/Phrase quantized destination behavior is not falsely claimed by this slice.
- [ ] No watchdog reset, deadlock, stuck notes, crackle burst, or sustained underrun growth.
