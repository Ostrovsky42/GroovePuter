# Realtime SMF Player Stage

## Purpose

Add a faithful Standard MIDI File player alongside the existing destructive `MidiImporter`.

The product contract is:

```text
PROJECT -> MIDI browser -> selected .mid
                          |- PLAY
                          |- IMPORT TO GROOVEBOX
                          `- INFO / ROUTING
```

`PLAY` must not rewrite GroovePuter patterns or scenes. `IMPORT TO GROOVEBOX` keeps the existing quantized importer workflow.

The accepted design remains one USB owner:

```text
SMF timeline -------------------\
PatternPlayer -> scheduled notes +--> MidiDispatchTask -> TinyUSB
transport sync -> F8/FA/FC -----/
```

No UI, parser, DSP callback, or Arduino `loop()` may write TinyUSB directly.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3;
- microSD card containing `.mid` files under `/midi`;
- data-capable USB-C cable;
- Yamaha SEQTRAK or another class-compliant USB MIDI receiver;
- optional Linux MIDI monitor for timestamp capture.

## Wiring

Cardputer-Adv USB-C data connection -> Yamaha SEQTRAK USB MIDI path.

PORT.A is not used by this test. GPIO2 SDA / GPIO1 SCL, `Wire`, Scroll Unit and display initialization order are unchanged.

## UX contract

### File action

Selecting a MIDI file must offer:

```text
PLAY
IMPORT TO GROOVEBOX
INFO / ROUTING
```

The existing `/midi` browser and file scan are reused; do not add a second browser.

### Now Playing

The player page prioritizes musical state over diagnostics:

```text
MIDI PLAYER                 USB READY
Future_Club.mid
BAR 13 : BEAT 2 / 48        118 BPM
[progress]
TEMPO  ORIGINAL
ROUTE  SEQTRAK
LOOP   OFF
```

Only changing regions should redraw during playback.

### Controls

Use only keys that physically exist on Cardputer:

```text
Space         play / pause from current position
Shift+Space   restart and immediately play
Left/Right    seek -1 / +1 bar
Shift+Left/Right seek -4 / +4 bars
L             loop on/off
Shift+L       A-B loop setup
X             player panic
```

Do not assign plain number keys; `1..0` remain GroovePuter mute controls.

`Shift+Space` is the primary "play song from the beginning" gesture. Restart is one lifecycle operation:

```text
release player-owned notes
-> invalidate old scheduled SMF events
-> seek restart origin
-> restore timeline/tempo state
-> PLAY
```

Restart origin is user-selectable:

```text
MUSIC START  (default; first NoteOn)
FILE START   (tick 0, including setup/silence)
```

At end-of-file, a subsequent Play starts again from `MUSIC START` by default.

## Playback model

### Tempo

Two independent tempo policies are planned:

- `ORIGINAL`: follow the SMF tempo map exactly;
- `PROJECT`: preserve ticks, durations and tuplets while scaling the timeline to GroovePuter BPM.

`PROJECT` is retiming, not quantization.

### Routing

Routing is independent of tempo:

- `RAW`: preserve source channels;
- `SEQTRAK`: map musical roles to the SEQTRAK profile;
- `CUSTOM`: later per-track destination editing.

SEQTRAK profile:

```text
CH1  KICK
CH2  SNARE
CH3  CLAP
CH4  HAT1
CH5  HAT2
CH6  PERC1
CH7  PERC2
CH8  SYNTH1
CH9  SYNTH2
CH10 DX
CH11 SAMPLER
```

A GM drum track on source CH10 must eventually split by drum note into SEQTRAK drum tracks 1..7; it must not be forwarded as a single "drums CH10" lane.

### v1 event support

Required for faithful v1 playback:

- SMF Type 0 and Type 1;
- PPQN time division;
- running status;
- NoteOn and NoteOff including NoteOn velocity zero;
- velocity;
- simultaneous notes / external polyphony;
- tempo meta events;
- time-signature meta events for bar display;
- Program Change parsed for routing/inspection.

Deferred from v1 runtime output:

- SMPTE division;
- CC automation;
- Pitch Bend;
- Aftertouch;
- SysEx;
- BLE MIDI;
- MIDI input/slave mode.

## Foundation implemented in this stage

The platform-neutral foundation does not touch TinyUSB or audio state.

It provides:

- bounds-checked SMF Type 0/1 parsing;
- PPQN validation;
- running-status parsing;
- ordered note/tempo/time-signature/program timeline;
- exact NoteOff preservation;
- first-NoteOn `musicStartTick`;
- deterministic timeline cursor;
- `FILE START` / `MUSIC START` restart semantics;
- EOF -> Play restart from `MUSIC START`;
- bounded streaming track cursors for Cardputer runtime;
- tick-to-time and bar/beat timing helpers;
- a bounded scheduled SMF note queue with generation invalidation and panic recovery.

The runtime increment must translate due SMF events into the accepted dispatcher. It must not create a second USB task or wall-clock MIDI scheduler.

## Build / Flash

Foundation host validation:

```bash
bash tests/run_host_tests.sh
```

After runtime/UI integration:

```bash
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Use the repository-pinned M5Stack ESP32 core and current TinyUSB settings.

## Expected behavior

Foundation host tests prove:

- Type 1 conductor + note tracks merge into one ordered timeline;
- tempo and time-signature metadata survive parsing;
- notes starting after setup silence produce a non-zero `musicStartTick`;
- running-status NoteOn is parsed;
- NoteOn velocity zero becomes NoteOff;
- simultaneous notes stay simultaneous;
- restart from FILE START exposes tick-zero setup events;
- restart from MUSIC START lands on the first musical NoteOn;
- EOF Play restarts from MUSIC START;
- SMPTE and truncated files fail safely;
- long files can be streamed with bounded cursor/cache state instead of retaining every event;
- scheduled NoteOn cannot consume cleanup reserve;
- seek/restart/stop can invalidate stale queued SMF events and request panic cleanup.

## Troubleshooting

- **MIDI still sounds quantized:** that is the old `MidiImporter`; realtime PLAY is a separate path and must not call `MidiImporter::importFile()`.
- **No notes after selecting a file:** check whether PLAY is implemented for the current stage; the foundation alone does not dispatch USB MIDI.
- **Unexpected silence at restart:** compare `MUSIC START` vs `FILE START`.
- **Stuck notes after seek/restart:** runtime integration is invalid unless it releases player ownership and invalidates old scheduled events before changing position.
- **Clock bursts:** never derive SMF scheduling from `millis()` or a second timer task; use the accepted sample-timed dispatcher.

## Acceptance checklist

### Foundation

- [ ] host parser tests pass with `-Wall -Wextra -Werror`.
- [ ] Type 0/1 PPQN parsing works.
- [ ] running status works.
- [ ] NoteOff duration information is preserved.
- [ ] tempo map events are preserved.
- [ ] simultaneous notes keep identical ticks.
- [ ] `MUSIC START` is the first NoteOn tick.
- [ ] `FILE START` is tick zero.
- [ ] EOF Play restarts from `MUSIC START`.
- [ ] malformed/truncated input fails without out-of-bounds access.
- [ ] production stream state is bounded and does not retain the whole MIDI event list.
- [ ] scheduled queue reserves NoteOff cleanup capacity and supports generation invalidation.

### Runtime / hardware gate before merge

- [ ] `PLAY` does not mutate project patterns/scenes.
- [ ] `IMPORT TO GROOVEBOX` remains available and unchanged.
- [ ] `Space` plays/pauses current position.
- [ ] `Shift+Space` restarts audibly from the configured beginning.
- [ ] original note lengths are recognizable by ear.
- [ ] chords remain polyphonic on USB output.
- [ ] triplets are not flattened to 1/16.
- [ ] tempo changes reproduce without restart.
- [ ] seek/restart/stop leave no stuck notes.
- [ ] no stale pre-seek event is emitted after generation invalidation.
- [ ] player may continue while leaving the Now Playing page.
- [ ] PERFORM can later coexist with external SMF playback under separate ownership.
- [ ] Cardputer-Adv -> SEQTRAK timing is stable under UI navigation.
- [ ] no internal audio underrun/watchdog regression.
