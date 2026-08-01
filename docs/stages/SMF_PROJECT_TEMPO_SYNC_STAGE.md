# SMF PROJECT TEMPO SYNC STAGE

## Purpose

Synchronize Standard MIDI File playback with GroovePuter transport so Yamaha SEQTRAK receives MIDI Clock and SMF notes from one musical timeline.

Two tempo sources are available:

```text
ORIGINAL
SMF tempo map -> realtime playback

PROJECT
SMF tick positions -> GroovePuter BPM / project phase
```

`PROJECT` changes absolute speed without flattening native tick spacing, note lengths, triplets or syncopation.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3, no PSRAM assumed)
- Yamaha SEQTRAK
- data-capable USB-C cable
- microSD card with `.mid` files under `/midi`

## Wiring

```text
Cardputer-Adv USB-C
        |
        | USB MIDI
        v
Yamaha SEQTRAK USB
```

PORT.A is not required. If unrelated I2C hardware is connected:

```text
SDA GPIO2
SCL GPIO1
```

## Timing model

```text
GroovePuter sequencer: 96 PPQN / Q32.32 phase
USB MIDI Clock:       24 PPQN
SMF:                  native PPQN division
```

The AudioTask publishes the same live project phase/block snapshot used by MIDI Clock. Future PROJECT SMF events are scheduled from that current snapshot, not projected indefinitely from one launch-time BPM/block anchor.

This distinction matters during longer SEQTRAK recordings: Clock and NoteOn/NoteOff must continue to reference the same evolving project phase.

No independent wall-clock scheduler, transport task or second TinyUSB owner is introduced.

## SEQTRAK routing assumptions

```text
CH1  KICK
CH2  SNARE
CH3  CLAP
CH4  HAT1
CH5  HAT2
CH6  PERC1
CH7  PERC2
CH8  SYNTH 1
CH9  SYNTH 2
CH10 DX
CH11 SAMPLER
```

Safe SMF mapping:

```text
source CH1       -> SYNTH 1 / CH8
source CH2       -> SYNTH 2 / CH9
source CH3       -> DX / CH10
source GM CH10   -> native drums / CH1..7
source CH4+      -> unmapped CH16 sink
```

Extra melodic channels must not trigger the user's recorded SAMPLER pads. Use RAW when faithful whole-file channel preservation is more important than reducing the arrangement to SEQTRAK targets.

## Controls

```text
T           ORIGINAL / PROJECT tempo source
Enter       load selected file; never starts playback
G           GroovePuter transport only
Space       SMF Play / Pause only
Up / Down   ORIGINAL: change SMF tempo
            PROJECT:  change GroovePuter BPM / Clock
R           restart from MUSIC START
Left/Right  seek -/+ 1 bar
M           RAW / SEQTRAK SAFE routing
V           velocity boost
O           original tempo reset
X           player panic / pause
B           browser
D           diagnostics
```

PROJECT has explicit transport ownership. Start GroovePuter with `G`, then arm SMF with `Space`; the SMF begins on NEXT BAR. `Space` cannot create a hidden arm while the project master is stopped. Stopping GroovePuter with `G` pauses PROJECT-SMF at its last published tick and requests immediate player-scoped note cleanup.

Switching `T` while playing preserves playback intent only when the destination clock is already running. Switching an active file to PROJECT while GroovePuter is stopped leaves SMF paused until `G`, then `Space`. `T` is not a sound-off command.

## Build / flash

Run:

```bash
./tests/run_host_tests.sh
```

Then use the repository's existing pinned Cardputer-Adv Arduino build and flash procedure.

## Test procedure

### 1. Baseline

1. Boot normally.
2. Verify internal GroovePuter audio.
3. Verify PERFORM A/B/DX/DRUMS.
4. Verify Start/Clock/Stop reaches SEQTRAK.

### 2. ORIGINAL regression

1. Select ORIGINAL.
2. Load a known MIDI.
3. Verify normal full-file playback, seek, restart, velocity and RAW routing.

### 3. PROJECT launch

1. Use a short constant-tempo MIDI for the first test.
2. Set GroovePuter to 90 BPM.
3. Start transport and a simple SEQTRAK drum pattern/metronome.
4. Select PROJECT with `T`.
5. Observe `ARMED`.
6. Verify entry on a bar boundary without manually catching beat 1.

### 4. Long recording stability

1. Record the incoming MIDI into SEQTRAK for at least 32 bars.
2. Compare the beginning, middle and end against SEQTRAK drums/metronome.
3. Listen for accumulating early/late movement, alternating jitter or changed note lengths.
4. Stop and verify no hanging notes.

A complete song containing internal tempo changes is not a clean PROJECT drift test: PROJECT intentionally follows one GroovePuter BPM rather than the file's tempo map.

### 5. Tempo change

1. While PROJECT plays, change 90 -> 110 BPM.
2. Verify SEQTRAK Clock and SMF notes change together.
3. Verify no catch-up burst or stuck notes.
4. Hold a long note across the change and verify it is not cut by CC123.
5. Repeat several Up/Down changes and verify the musical position is not skipped.

## Scheduler hardening contract

- One coherent PROJECT timeline snapshot is captured for each scheduling pass.
- The timeline carries a transport epoch and Q16.16 BPM. Events from an older
  transport session are rejected by `MidiDispatchTask`.
- A transient seqlock read miss leaves the player state unchanged. It is not a
  synthetic Stop.
- A timeline older than two audio blocks causes a controlled PROJECT pause and
  SMF-scoped cleanup.
- Tempo re-anchor preserves the exact fractional SMF tick. It invalidates only
  future scheduled deadlines and does not request panic.
- A PROJECT NoteOn already behind the live phase is dropped and counted. A late
  NoteOff remains cleanup-eligible and is scheduled on the current block.
- Dispatcher NoteOn lateness is limited to one audio block. Sent-late NoteOn,
  maximum NoteOn lateness, late NoteOff and epoch drops are separate metrics.

### 6. Routing

1. In SEQTRAK SAFE, verify extra melodic lanes do not fire SAMPLER pads.
2. Compare RAW for full-arrangement fidelity.
3. Verify CH1/CH2/CH3 and GM drums reach expected targets.

## Expected behavior

- ORIGINAL remains faithful to the MIDI tempo map.
- PROJECT displays ARMED before quantized entry.
- MIDI Clock and PROJECT notes remain phase-aligned over a long recording.
- `T` changes tempo source without unexplained silence.
- Extra channels do not trigger recorded sampler sounds.
- RAW remains available for faithful channel preservation.
- Pause/Stop/Restart/Panic leave no stuck notes.

## Troubleshooting

### T changes mode but playback is silent

Check the displayed state. Active playback should move through PROJECT `ARMED` to `PLAYING`, or resume ORIGINAL. If it remains PAUSED, record the exact state/message and serial output.

### PROJECT waits on `WAIT PROJECT PLAY`

GroovePuter transport is stopped or the AudioTask has not published a valid project block. Start project transport and confirm MIDI Clock reaches SEQTRAK.

### Phrase starts one bar later

The nearest bar boundary was too close for bounded SD/parser prefill. The scheduler deliberately selects the following bar instead of sending a late burst.

### Playback gradually moves against SEQTRAK

Treat this as a failure. Reproduce with a short constant-tempo MIDI, fixed BPM, no tempo changes and at least 32 recorded bars. Note whether the error accumulates continuously or alternates around the beat.

### RAW sounds better

This is expected for many complete arrangements. RAW preserves original MIDI channels. SEQTRAK SAFE deliberately maps only three melodic source channels plus drums and silences additional channels until custom routing exists.

### Hanging notes

Press `X`, capture serial diagnostics and record whether the issue followed Stop, BPM change, route change or mode change.

## Acceptance checklist

```text
[ ] normal boot succeeds
[ ] internal GroovePuter audio unchanged
[ ] PERFORM A/B/DX/DRUMS unchanged
[ ] PatternPlayer unchanged
[ ] MIDI Start/Clock/Stop reaches SEQTRAK
[ ] ORIGINAL playback unchanged
[ ] T visibly selects ORIGINAL / PROJECT
[ ] active T switch does not end in unexplained silence
[ ] PROJECT shows ARMED
[ ] first notes enter on a bar boundary
[ ] 32-bar recording shows no accumulating drift
[ ] 90 -> 110 BPM changes Clock and SMF together
[ ] 90 -> 110 BPM does not panic or cut an active long note
[ ] repeated BPM changes do not skip an SMF fragment
[ ] no catch-up burst
[ ] projectLateDrop and smfLateDrop remain explainable and bounded
[ ] timelineStale remains zero during normal playback
[ ] triplets and syncopation remain recognizable
[ ] RAW routing unchanged
[ ] source CH1 -> CH8
[ ] source CH2 -> CH9
[ ] source CH3 -> DX CH10
[ ] source CH4+ does not trigger SAMPLER
[ ] GM source CH10 drums -> native CH1..7
[ ] Stop/Pause/Restart/Panic leave no stuck notes
[ ] no watchdog/reset
[ ] no sustained underrun regression
[ ] host-tests green
[ ] SDL build green
[ ] Cardputer-Adv build green
```
