# Performance Workflow — Cardputer ADV

## Purpose

Validate the first `PERFORM / PATTERN / ARRANGE` vertical slice on real Cardputer-Adv hardware.

This test covers only the live Cardputer keyboard path:

```text
Cardputer keyboard matrix
→ PerformanceKeyboard
→ MusicalEventRouter
→ InternalSynthOutput
→ Synth A
```

Atlas remains an optional factory seed-pattern source. This test does not add or validate USB MIDI, SEQTRAK routing, an arpeggiator, polyphony, or live recording into patterns.

## Hardware list

- M5Stack Cardputer-Adv with Stamp-S3A / ESP32-S3FN8.
- USB-C data cable.
- Development computer with `arduino-cli`.
- Optional 3.5 mm headphones for easier note-release testing.

Cardputer-Adv is built as a DRAM-only target:

```text
m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app
```

## Wiring

No external wiring is required.

The test uses:

- the built-in Cardputer keyboard;
- the built-in ES8311 audio codec and speaker;
- USB-C for power, flashing, and Serial monitoring.

PORT.A is not used. Its project invariant remains GPIO2 SDA / GPIO1 SCL.

## Build and flash

Install the pinned Arduino dependencies and compile all warnings:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
```

Flash current sources. Replace the port if the device is not `/dev/ttyACM0`:

```bash
bash scripts/upload.sh /dev/ttyACM0
```

Open a Serial monitor using the baud rate already used by the firmware:

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Controls

### Workflow

- Startup: `PERFORM`.
- `Fn + Tab`: cycle `PERFORM → PATTERN → ARRANGE`.
- `Fn + Shift + Tab`: cycle backward.
- On the PERFORM page: `1` = PERFORM, `2` = PATTERN, `3` = ARRANGE.

### Musical keyboard

```text
Q W E R T Y U I O P
A S D F G H J K L
```

Both rows use the same bounded scale-degree span. Synth A is monophonic and uses last-note priority.

### Performance settings

- `[` / `]`: previous / next scale.
- `-` / `=`: octave down / up.
- `X`: Panic / All Notes Off.
- `Space` or the Cardputer action button: transport start/stop.

Defaults after every reboot:

```text
Root: C
Scale: Natural Minor
Octave shift: 0
```

Scale and octave are runtime-only in this stage and are not written to scene JSON.

## Expected behavior

### Transport stopped

- Note keys play Synth A immediately.
- The most recently pressed held key owns the monophonic voice.
- Releasing a non-active held key does not interrupt the active note.
- Releasing the active key restores the previous held key.
- Releasing the last key releases the voice.
- A missed key-up is recovered from the next keyboard matrix state.

### Transport running

- Starting transport sends All Notes Off and clears the held-note stack.
- Live note input is disabled.
- PatternPlayer exclusively owns Synth A.
- Stopping transport re-enables live input; press a note again to play.

### Pages

Live keyboard is required on:

- PERFORM;
- Synth A parameter page;
- Feel/Texture page.

The active page command has priority over a note. The step sequencer is not required to expose live keyboard input in this stage because its editing shortcuts may conflict.

## Serial validation

During ordinary playing, watch the existing `[PERF]` telemetry.

Confirm:

- `underruns` does not continually increase;
- free internal heap does not trend downward on each note;
- no watchdog, Guru Meditation, reset, or heap-corruption message appears;
- page switching does not cause a long audio pause.

## Troubleshooting

### First key only dismisses the splash

Wait for the splash to close automatically or press a non-musical key once, then retry the note.

### Notes do not play

1. Confirm transport is stopped.
2. Confirm the current page is PERFORM, Synth A parameters, or Feel/Texture.
3. Press `X`, release every key, and try again.
4. Confirm Synth A is not muted.
5. Check Serial output for an audio or engine initialization failure.

### A note remains sounding

1. Press `X` for Panic.
2. Release all physical keys.
3. Start and stop transport once; transport acquisition also performs All Notes Off.
4. Capture Serial output and the exact key sequence if it repeats.

### Octave does not move higher

The default root is already the highest safe runtime octave for all supported scales and all nineteen keys. Two downward shifts are available; `=` returns toward the default.

### A key edits the page instead of playing

This is intentional when the active page owns that key. Page commands have priority over performance input.

## Acceptance checklist

### Screen

- [ ] Firmware starts on PERFORM.
- [ ] Screen shows root C, Natural Minor, octave, held count, and active MIDI note.
- [ ] `Fn + Tab` cycles PERFORM, PATTERN, and ARRANGE.
- [ ] Existing detailed pages remain accessible.

### Sound and key ownership

- [ ] Both physical rows play Synth A while transport is stopped.
- [ ] Last pressed held note becomes active.
- [ ] Releasing an inactive note does not interrupt the active note.
- [ ] Releasing the active note restores the previous held note.
- [ ] Releasing the final note always releases Synth A.
- [ ] Repeated key scans do not retrigger or duplicate held notes.
- [ ] `X` immediately silences the voice.
- [ ] A new note works normally after Panic.

### Transport

- [ ] Starting transport silences live input and clears held notes.
- [ ] PatternPlayer plays Synth A without competition from the keyboard.
- [ ] Live input remains blocked for the entire transport run.
- [ ] Stopping transport allows new live notes again.

### Editors and lifecycle

- [ ] Live notes work on the Synth A parameter page.
- [ ] Filter parameters can be edited while a note is held.
- [ ] Live notes work on the Feel/Texture page.
- [ ] Switching between supported pages does not create a stuck note.
- [ ] Leaving the supported page set releases live notes.
- [ ] Changing Synth A engine releases the current live note.
- [ ] Project reset releases live notes and clears held state.
- [ ] Live playing does not modify Synth A pattern data.

### Regression and stability

- [ ] Existing probabilistic recipe IDs 1–5 remain available.
- [ ] Atlas recipes still apply as seed patterns.
- [ ] Save/Load behavior is unchanged.
- [ ] No reset, watchdog, or heap corruption occurs.
- [ ] Underruns do not continually increase during a 30-minute play/edit/page-switch test.

Do not merge the PR until this checklist has been physically validated on Cardputer-Adv.
