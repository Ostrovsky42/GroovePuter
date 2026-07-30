# Performance Workflow — Cardputer ADV

## Purpose

Validate the first `PERFORM / PATTERN / ARRANGE` vertical slice on real Cardputer-Adv hardware.

This test covers only the internal live-keyboard path:

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

Open the Serial monitor:

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Controls

### Workflow

- Startup: `PERFORM`.
- `Fn + Tab`: cycle `PERFORM → PATTERN → ARRANGE`.
- `Fn + Shift + Tab`: cycle backward.
- On PERFORM: `1` = PERFORM, `2` = PATTERN, `3` = ARRANGE.

### NOTE mode

- NOTE mode is ON after every reboot.
- `N` on PERFORM toggles NOTE mode ON/OFF.
- The screen always shows `NOTE MODE: ON` or `NOTE MODE: OFF`.
- NOTE mode is runtime-only and is not stored in scene/project JSON.

When NOTE mode is ON, all nineteen musical keys belong to the performance layer. This remains true while transport is running: NoteOn is blocked, but the key is consumed and cannot reach legacy randomize or BPM shortcuts.

When NOTE mode is OFF, the letters are available to the existing page/global commands.

### Musical keyboard

```text
Q W E R T Y U I O P   upper manual, one octave higher
A S D F G H J K L     lower manual
```

Both rows use the same scale-degree layout. The upper row is exactly one octave above the lower row. Synth A is monophonic and uses last-note priority.

### Performance settings

- `[` / `]`: previous / next scale.
- `-` / `=`: octave down / up.
- `X`: live Synth A Panic / All Notes Off.
- `Space` or the Cardputer action button: transport start/stop.

Defaults after every reboot:

```text
NOTE mode: ON
Root: C
Lower manual: C2
Upper manual: C3
Scale: Natural Minor
Octave shift: 0
```

NOTE mode, scale and octave are runtime-only in this stage.

## Expected behavior

### Transport stopped, NOTE mode ON

- Note keys play Synth A immediately.
- The most recently pressed held key owns the monophonic voice.
- Releasing a non-active held key does not interrupt the active note.
- Releasing the active key restores the previous held key.
- Releasing the last key releases the voice.
- A missed key-up is recovered from the next keyboard matrix state.

### Transport running, NOTE mode ON

- Starting transport sends target-scoped All Notes Off and clears the held-note stack.
- Live NoteOn is disabled.
- PatternPlayer exclusively owns Synth A.
- Musical keys are consumed and do not invoke legacy shortcuts.
- Stopping transport permits new live notes; press the key again to play.

### Target-scoped Panic

A `PerformanceKeyboard` AllNotesOff event targets Synth A only.

- It releases Synth A only when Synth A currently has a live-owned note.
- It does not release PatternPlayer-owned Synth A while transport is running.
- It never releases Synth B.
- A future global project/transport panic must remain a separate operation.

### Pages

Live keyboard is required on:

- PERFORM;
- Synth A parameter page;
- Feel/Texture page.

The active page command has priority over a note. The step sequencer is not required to expose live keyboard input in this stage because its editing shortcuts may conflict.

## Mandatory P0 regression procedure

Perform this section before the general playability checks.

### P0.1 — blocked note keys must not reach legacy commands

1. Open PERFORM.
2. Confirm `NOTE MODE: ON`.
3. Record the current BPM and keep a recognizable Synth A, Synth B and drum pattern.
4. Start transport.
5. Press `I`, `O`, `P`, `K` and `L` several times.
6. Stop transport.

Expected:

- BPM is unchanged;
- Synth A pattern is unchanged;
- Synth B pattern is unchanged;
- drum pattern is unchanged;
- no randomize or BPM toast/action occurs.

Then press `N` to set NOTE mode OFF and verify that legacy commands are available again. Press `N` once more before continuing.

### P0.2 — performance lifecycle must not release PatternPlayer

1. Confirm NOTE mode is ON.
2. Start a pattern containing sustained or clearly audible Synth A and Synth B notes.
3. While transport runs, navigate:

```text
PERFORM → Genre → Synth A parameters → Feel/Texture
```

4. Return to PERFORM.
5. While transport continues, change scale with `[` / `]`.
6. Change octave with `-` / `=`.
7. Toggle NOTE mode OFF and ON with `N`.

Expected:

- PatternPlayer continues without a note release caused by page navigation;
- scale/octave changes do not cut Synth A or Synth B;
- NOTE-mode toggling does not cut PatternPlayer;
- no extra click attributable to a forced voice `release()` is heard;
- Synth B is never affected by a Synth A performance panic.

## Serial validation

During ordinary playing, watch the existing `[PERF]` telemetry.

Confirm:

- `underruns` does not continually increase;
- free internal heap does not trend downward on each note;
- no watchdog, Guru Meditation, reset, or heap-corruption message appears;
- page switching does not cause a long audio pause.

## Troubleshooting

### First key only dismisses the splash

Wait for the splash to close automatically or press a non-musical key once, then retry.

### Notes do not play

1. Confirm `NOTE MODE: ON` on PERFORM.
2. Confirm transport is stopped.
3. Confirm the current page is PERFORM, Synth A parameters, or Feel/Texture.
4. Press `X`, release every key, and try again.
5. Confirm Synth A is not muted.
6. Check Serial output for an audio or engine initialization failure.

### Letters run old commands instead of notes

Return to PERFORM and press `N` until the screen shows `NOTE MODE: ON`.

### A note remains sounding

1. Press `X` for live Synth A Panic.
2. Release all physical keys.
3. Start and stop transport once.
4. Capture Serial output and the exact key sequence if it repeats.

### Octave does not move higher

The default two-manual range is already the highest safe runtime position for all supported scales and all nineteen keys. One downward shift is available; `=` returns to the default position.

### A key edits the page instead of playing

This is intentional when the active page owns that key. Page commands have priority over performance input.

## Acceptance checklist

### Screen and mode

- [ ] Firmware starts on PERFORM.
- [ ] Screen shows `NOTE MODE: ON` after reboot.
- [ ] `N` toggles NOTE mode and the displayed state.
- [ ] Screen shows root C, Natural Minor, octave, held count, and active MIDI note.
- [ ] `Fn + Tab` cycles PERFORM, PATTERN, and ARRANGE.
- [ ] Existing detailed pages remain accessible.

### P0 regressions

- [ ] With transport running and NOTE mode ON, `I/O/P` do not randomize patterns.
- [ ] With transport running and NOTE mode ON, `K/L` do not change BPM.
- [ ] Leaving PERFORM for a page without live input does not release PatternPlayer.
- [ ] Scale change during transport does not release PatternPlayer.
- [ ] Octave change during transport does not release PatternPlayer.
- [ ] NOTE-mode OFF/ON during transport does not release PatternPlayer.
- [ ] Performance AllNotesOff affects only live-owned Synth A.
- [ ] Performance AllNotesOff never releases Synth B.

### Sound and key ownership

- [ ] Both physical rows play Synth A while transport is stopped and NOTE mode is ON.
- [ ] Upper-row notes sound one octave above matching lower-row scale degrees.
- [ ] Last pressed held note becomes active.
- [ ] Releasing an inactive note does not interrupt the active note.
- [ ] Releasing the active note restores the previous held note.
- [ ] Releasing the final note always releases Synth A.
- [ ] Repeated key scans do not retrigger or duplicate held notes.
- [ ] `X` immediately silences a live-owned Synth A note.
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
- [ ] Leaving the supported page set releases live notes without touching PatternPlayer.
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
