# Player + PERFORM UI Polish

## Purpose

Make GroovePuter read visually as a compact musical instrument on-device and on camera without changing audio, MIDI timing, routing, or ownership.

This stage focuses on:

- a piano-shaped PERFORM view for Synth A / Synth B / DX;
- native seven-pad PERFORM view for SEQTRAK DRUMS;
- visible held-note / held-pad state and active velocity;
- a stage-readable MIDI Player layout with state, route, velocity, BPM, bar/beat and progress;
- theme-aware accents using the existing visual-style system;
- stable high-contrast geometry suitable for photo/video capture.

No animation timer, framebuffer, sprite, large canvas, or new task is introduced.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3
- microSD card with optional `/midi/*.mid` files
- optional Yamaha SEQTRAK for external MIDI validation
- USB-C data cable when testing SEQTRAK output

## Wiring

For standalone PERFORM visual testing, no external wiring is required.

For SEQTRAK validation:

```text
Cardputer-Adv USB-C data -> Yamaha SEQTRAK USB-C
```

PORT.A is unchanged and unused by this stage. Existing Cardputer-Adv PORT.A assumptions remain GPIO2 SDA / GPIO1 SCL.

## Build / flash

```bash
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Repository-pinned M5Stack ESP32 Arduino core remains 3.2.2 with PSRAM disabled.

## Expected behavior

### PERFORM

For Synth A, Synth B and DX:

- the screen presents a one-octave piano graphic;
- held pitch classes are highlighted;
- target, MIDI channel, octave, scale, held count, active note and velocity remain readable;
- `N`, `\\`, `,/.`, `-/+`, and `X` keep their existing behavior.

For DRUMS:

- piano is replaced by seven labeled pads;
- A/S/D/F/G/H/J pads highlight while held;
- existing native SEQTRAK CH1..7 routing is unchanged.

### MIDI Player

Now Playing presents:

- large PLAYING / PAUSED / ERROR state badge;
- RAW / SEQTRAK routing badge;
- velocity boost badge;
- filename;
- bar/beat and effective BPM;
- long progress bar and percent;
- ORIGINAL tempo/reset information;
- concise physical controls.

Existing tempo, velocity, seek, restart, routing and panic semantics remain unchanged.

### Camera behavior

The UI intentionally favors stable blocks and high contrast over animation:

- no timer-driven note glow;
- no rapid blinking;
- no scanline/strobe effect added by this stage;
- no high-frequency visualization loop;
- active states change only from musical/player state.

This reduces visual ambiguity in phone video while keeping CPU cost bounded.

## Troubleshooting

### Piano does not highlight

Confirm NOTE mode is ON and GroovePuter transport is stopped. Pattern playback intentionally blocks live PERFORM ownership.

### Drum pad does not highlight

Confirm target is DRUMS and use A/S/D/F/G/H/J. The display reads the same held-key state used by `PerformanceKeyboard`; it does not infer activity from MIDI output.

### MIDI Player is empty

Place `.mid` files under `/midi` on the microSD card and refresh the browser.

### Theme looks different between pages

This stage consumes the existing global visual style and does not replace the older per-page theme implementations. The new piano, pads, chips and progress primitives adapt their accents to the current style while preserving existing page behavior.

### UI or audio becomes less responsive

Enable the existing performance diagnostics and compare against `main`. This stage must not add sustained CPU load. There are no new tasks or periodic animations; any regression should be treated as a rendering-path bug.

## Acceptance checklist

- [ ] normal boot reaches the existing groovebox UI
- [ ] internal audio remains unchanged
- [ ] PERFORM Synth A shows piano and still plays internal + USB MIDI
- [ ] PERFORM Synth B shows piano and still plays internal + USB MIDI
- [ ] PERFORM DX shows piano and still sends external CH10 only
- [ ] held melodic keys visibly highlight corresponding pitch classes
- [ ] active note and velocity update correctly
- [ ] DRUMS replaces piano with seven pads
- [ ] A/S/D/F/G/H/J visibly highlight the correct pads
- [ ] DRUMS still reach SEQTRAK CH1..7 independently
- [ ] NOTE OFF / transport-blocked state remains obvious
- [ ] MIDI Player browser still opens and loads files
- [ ] Now Playing state is readable at arm's length
- [ ] progress bar advances smoothly enough without affecting audio
- [ ] RAW / SEQTRAK, BPM and velocity states are obvious on screen
- [ ] Space / R / seek / M / V / O / X behavior is unchanged
- [ ] switching existing visual styles keeps text and active states readable
- [ ] phone photo has recognizable piano/pads and current target without zooming into tiny debug text
- [ ] phone video shows no new flicker/strobe behavior
- [ ] no increasing audio underruns
- [ ] no watchdog/reset
- [ ] host tests pass
- [ ] SDL build passes
- [ ] Cardputer-Adv build passes
