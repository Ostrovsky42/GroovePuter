# Song playhead — Cardputer-Adv acceptance

## Purpose

Verify that Song rows advance by musical bars rather than by an accidental
sixteenth-step multiplier, while Pattern mode and the existing internal audio
remain unchanged.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3, no PSRAM)
- USB-C data cable for flashing
- optional Yamaha SEQTRAK for the second validation stage
- headphones or the built-in Cardputer speaker

## Wiring

Flash/test on a computer:

```text
Cardputer-Adv USB-C -> USB data cable -> computer
```

Optional external MIDI check:

```text
Cardputer-Adv USB-C -> USB data cable -> SEQTRAK USB-C
```

No GPIO or PORT.A wiring is used.

## Build and flash

Use the repository-pinned M5Stack ESP32 core `3.2.2`.

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

## Test preparation

Create four Song rows with clearly different Synth A patterns. Keep drums and
Synth B distinct as an additional visual/audible cross-check. Disable Loop for
the first pass.

## Expected behavior

- `1B`: each row lasts exactly one bar.
- `2B`: each row lasts exactly two bars.
- `4B`: each row lasts exactly four bars.
- `8B`: each row lasts exactly eight bars.
- Stop followed by Play starts a complete row cycle.
- Manually selecting another Song row starts that row from bar 1.
- Enabling Song mode starts a complete row cycle.
- Loading a scene starts a complete row cycle.
- Reverse changes traversal direction only at a row boundary.
- Loop preserves the configured row duration.
- Pattern mode never advances the Song playhead.

## SEQTRAK check

After the internal test passes, connect SEQTRAK directly. Synth A and Synth B
should follow the same Song-row changes on MIDI channels 8 and 9. Drums remain
internal in this stage. MIDI timing jitter is handled by the separate draft
PR #8 and is not part of this fix.

## Troubleshooting

- A `1B` row lasting 16 bars means the legacy step multiplier is still present.
- A row changing immediately on Play means the pre-first-bar phase was lost.
- A manual row selection inheriting the previous row's remaining bars means the
  lifecycle reset is missing.
- Only two tracks sounding on SEQTRAK is expected until drum MIDI is added.
- Use core `3.2.2`; a build with `3.2.5` is not an acceptance result.

## Acceptance checklist

- [ ] firmware boots without reset or watchdog
- [ ] internal Synth A, Synth B and drums remain audible
- [ ] `1B` rows advance every one bar
- [ ] `2B` rows advance every two bars
- [ ] `4B` rows advance every four bars
- [ ] `8B` rows advance every eight bars
- [ ] Stop -> Play restarts the current row from bar 1
- [ ] manual row selection restarts the selected row from bar 1
- [ ] Song ON/OFF does not leave stale notes
- [ ] Reverse changes direction at a row boundary
- [ ] Loop keeps the configured row duration
- [ ] Pattern mode does not move the Song playhead
- [ ] optional SEQTRAK channels 8/9 follow the same row changes
- [ ] no continual increase in audio underruns
