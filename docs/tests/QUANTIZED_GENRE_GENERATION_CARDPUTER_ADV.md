# Quantized Genre Generation — Cardputer ADV

## Purpose

Verify that full GENRE materialization never replaces a sounding pattern in the middle of a bar.

While transport is running, `GENRE -> G` prepares the complete Stage 15 A+B+Drums candidate off the active playback path and commits it at the next real `BAR_START`. With transport stopped, generation remains immediate.

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

The remainder of the current bar must stay on the old material. Synth A, Synth B, drums, Genre state, mode, BPM/swing (when MATERIALIZE+BPM is used), and generated Stage 15 material change together at the next bar start.

There must be no transport stop/restart, mid-bar silence, half-old/half-new pattern, or manual timing requirement.

### Serial

No repeated generation should occur from the audio-thread BAR_START hook. The hook commits only the already prepared fixed-size transaction; it must not fall through to `regeneratePatternsWithGenre()`.

## Test procedure

1. Select a clearly audible one-bar groove and set BPM to 160–180.
2. Start transport and let it loop for several bars.
3. On GENRE, change Genre or Variant and press plain `G` around step 5.
4. Confirm the old bar continues unchanged through step 16.
5. Confirm the complete new groove starts on step 1 of the next bar.
6. Repeat at approximately steps 9 and 13.
7. During one bar press `G` twice before the boundary. Confirm only the newest pending transaction becomes active at the next bar.
8. While a generation is pending, change pattern page/bank/slot before the boundary. Confirm the pending transaction is cancelled rather than written into the wrong target.
9. Stop transport and press `G`. Confirm generation applies immediately with no `NEXT BAR` wait.
10. Set APPLY to `MATERIALIZE+BPM`, start playback, select a profile with a clearly different suggested BPM, and press Enter. Confirm BPM/material change together at the next bar rather than changing the length of the current bar.
11. Run for at least 10 minutes while repeatedly changing Genre/Variant and generating. Confirm no watchdog reset, stuck note, crackle burst, or sustained underrun growth.

## Troubleshooting

- **Audio still cuts when G is pressed:** verify `GenrePage::applyCurrent()` contains no `mini_acid_.stop()` / `mini_acid_.start()` generation pair and that the flashed SHA is the branch under test.
- **New pattern starts immediately while PLAY is active:** verify `regenerateWithQuantizedCommit()` sees `engine.isPlaying() == true` and returns `PendingNextBar`.
- **Nothing changes at the boundary:** verify the existing sequencer path reaches `barTick == 0` and `GenreSceneView::commitPendingRecipe()` invokes the installed bounded commit hook.
- **Wrong pattern changes:** treat as a blocker. Exact page + A/B/drum bank/slot targets must still match at commit time; otherwise pending must cancel.
- **BPM changes mid-bar in MATERIALIZE+BPM:** treat as a blocker. BPM is part of the pending full-generation transaction.
- **BAR_START causes a CPU/audio spike:** the audio boundary must only copy prepared material and apply bounded mode/BPM state. Heavy Stage 15 generation belongs to the control-side preparation path.

## Acceptance checklist

- [ ] PLAY + GENRE `G` never stops/restarts transport.
- [ ] Current bar remains musically unchanged after `G` until its end.
- [ ] Screen shows `GEN -> NEXT BAR` while pending.
- [ ] A+B+Drums activate together at the next step-0/bar boundary.
- [ ] Selected Genre/Variant/Rhythm becomes active at the same boundary.
- [ ] MATERIALIZE+BPM changes BPM/swing at the same boundary.
- [ ] Repeated `G` before the boundary uses the newest pending candidate.
- [ ] Changing page/bank/slot before commit cannot mutate the wrong target.
- [ ] STOP + `G` commits immediately.
- [ ] No heavy generation runs from the audio BAR_START callback.
- [ ] No watchdog reset, deadlock, stuck notes, crackle burst, or sustained underrun growth.
