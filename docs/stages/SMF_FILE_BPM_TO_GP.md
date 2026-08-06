# SMF FILE BPM → GroovePuter

## Purpose

Copy the original tempo metadata of the loaded Standard MIDI File into the
GroovePuter project tempo without switching the SMF scheduler between
`ORIGINAL` and `PROJECT` modes.

This is a bounded setup action for recording into SEQTRAK:

```text
SMF FILE BPM → GroovePuter BPM → USB MIDI CLOCK → SEQTRAK
```

It replaces the failed experimental FILE MASTER state machine. The action does
not start either transport, does not enqueue SMF commands, and does not change
the current SMF tempo mode.

## Hardware list

- M5Stack Cardputer-Adv.
- Yamaha SEQTRAK.
- USB-C data cable or the already accepted USB-MIDI host connection.

## Wiring

No GPIO or PORT.A wiring is required.

- Connect Cardputer-Adv to the SEQTRAK USB-MIDI path using a data-capable cable.
- Configure SEQTRAK to follow external MIDI Clock when GroovePuter is the master.
- Audio may be monitored separately through the existing audio connection.

## Build / Flash steps

```bash
git fetch origin
git checkout agent/smf-file-bpm-to-gp
./tests/run_host_tests.sh
./scripts/build.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

Flash the generated Cardputer-Adv firmware using the normal project workflow.

## Expected behavior

1. Open `MIDI PLAYER` and load a MIDI file.
2. Keep both GroovePuter and MIDI playback stopped.
3. Press `Alt+O`.
4. The footer shows `ALT+O FileBPM`.
5. A 120 BPM file shows:

```text
FILE 120.0 -> GP 120.0
```

6. The Clock source is changed to `GroovePuterInternal`.
7. The GroovePuter BPM becomes the file BPM through the existing audio guard.
8. The SMF remains in its current `ORIGINAL` or `GP MASTER` mode.
9. No `toggleTempoMode()`, `resetTempo()`, `togglePlayPause()`, MIDI Start, or
   direct TinyUSB call is issued by this action.
10. Start GroovePuter with `G`, then start the SMF with `Space` using the already
    accepted recording sequence.

The first version is intended for constant-tempo files. For a file containing
multiple tempo events, `originalBpmX10` represents the original tempo at the
current SMF position; do not claim full tempo-map following.

## Troubleshooting

- `LOAD MIDI FIRST`: the player has no usable loaded file.
- `STOP GP + MIDI FIRST`: GroovePuter is running, or the SMF is `PLAYING` / `ARMED`.
  Stop both transports and press `Alt+O` again.
- SEQTRAK still uses its own speed: configure it to follow external MIDI Clock.
- Recording is evenly spaced but starts between bars: BPM is now matched, but
  `G` and `Space` are still separate start actions. This PR does not add a
  combined transport launch.
- A 3/4 file does not align with SEQTRAK bar labels: MIDI Clock synchronizes
  quarter-note phase and tempo, not the source time signature. Eight 3/4 bars
  occupy the same duration as six 4/4 bars.
- Tempo changes later in the file are not followed: use a constant-tempo file
  for this acceptance pass. Full tempo-map transmission is outside this PR.

## Acceptance checklist

- [ ] Host source regressions pass.
- [ ] SDL build passes.
- [ ] Cardputer-Adv build and fixed-DRAM gate pass.
- [ ] Load a known constant-tempo MIDI file.
- [ ] With both transports stopped, `Alt+O` shows the exact file and GP BPM.
- [ ] GroovePuter Clock source becomes GP internal.
- [ ] The selected `ORIGINAL` / `GP MASTER` SMF mode does not change.
- [ ] Pressing `Alt+O` while GP runs is rejected.
- [ ] Pressing `Alt+O` while SMF is `PLAYING` or `ARMED` is rejected.
- [ ] No MIDI playback starts as a side effect.
- [ ] Start `G`, then `Space`; SEQTRAK follows external Clock.
- [ ] Record at least four bars and confirm even note spacing.
- [ ] Repeat with the supplied 120 BPM S.T.A.L.K.E.R. file.
- [ ] For its 3/4 material, validate duration rather than expecting 4/4 bar labels.
- [ ] Stop both transports and confirm no stuck notes.
