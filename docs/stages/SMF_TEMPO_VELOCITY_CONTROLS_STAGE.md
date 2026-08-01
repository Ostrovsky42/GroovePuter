# SMF Tempo + Velocity Controls Stage

## Purpose

Extend the accepted realtime SMF player with small performance controls without changing the accepted MIDI dispatcher, SD streaming model, SEQTRAK routing, PatternPlayer transport, or internal GroovePuter audio path.

This stage adds:

```text
Up / Down   effective playback BPM -/+ 1
O           restore ORIGINAL SMF tempo map
V           velocity boost +0 -> +8 -> +16 -> +0
```

Tempo adjustment preserves the relative timing encoded by the SMF tempo map. It scales the playback timeline; it does not quantize notes, triplets, note lengths, or bar positions.

For lifecycle safety, changing tempo while the SMF is playing first pauses at the current musical position and invalidates already-scheduled events. Press `Space` to resume with the new tempo. This avoids mixing events scheduled with two different tempo scales.

Velocity boost applies only to NoteOn velocity and saturates at MIDI velocity 127. NoteOff semantics and note ownership are unchanged.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3, PSRAM disabled.
- Yamaha SEQTRAK.
- Data-capable USB-C connection between Cardputer-Adv and SEQTRAK.
- SD card containing known `.mid` files under `/midi`.

Toolchain assumptions:

```text
M5Stack ESP32 Arduino core: 3.2.2 pinned
USBMode=default
CDCOnBoot=cdc
UploadMode=cdc
```

## Wiring

```text
Cardputer-Adv USB-C
        |
        | data-capable USB
        v
Yamaha SEQTRAK USB
```

No PORT.A wiring is required. PORT.A remains GPIO2 SDA / GPIO1 SCL and is not touched by this stage.

## Build / Flash

```bash
bash tests/run_host_tests.sh
bash scripts/build_sdl.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Use the repository-pinned dependencies. Do not update the ESP32/M5Stack core as part of this stage.

## Expected behavior

Open the accepted MIDI Player and start a known file:

```text
Alt+P
-> select /midi/*.mid
-> Enter
```

### Tempo

The Now Playing page displays the effective BPM. At original speed it also identifies the tempo as `ORIGINAL`.

Pressing `Up` or `Down` changes the current effective tempo by one BPM, bounded to the supported 40.0-250.0 BPM adjustment range at the current musical position.

If playback was running, the player safely pauses at the current position before applying the new scale. `Space` resumes from that same musical position.

`O` restores the original SMF tempo map scale.

Expected musical result:

- triplets remain triplets;
- syncopation is not flattened;
- note durations scale proportionally;
- tempo-map changes remain present and are scaled proportionally;
- seek/restart still use musical ticks/bars rather than a re-quantized grid.

### Velocity

`V` cycles:

```text
+0 -> +8 -> +16 -> +0
```

The boost is applied to NoteOn velocity only:

```text
velocity 64 + 8  -> 72
velocity 120 +16 -> 127
```

Velocity zero remains zero so NoteOn-with-zero velocity cannot be turned into an accidental sounding note.

### Existing controls remain

```text
Space       Play / Pause
R           Restart from MUSIC START
Left/Right  seek -/+ 1 bar
X           player-scoped Panic / Pause
B           files
M           RAW / SEQTRAK routing
D           performance diagnostics
```

## Troubleshooting

### Tempo key pauses playback

Expected in this stage. The scheduler already contains future events. Tempo changes invalidate those events and pause at the current musical position so old-scale and new-scale deadlines cannot be mixed. Press `Space` to resume.

### Tempo changes pitch

That is a regression. This feature changes MIDI event timing only; it must not transpose notes or alter pitch data.

### Notes become 1/16 quantized

That is a regression. Realtime SMF playback must continue to use exact SMF ticks and the accepted sample/block scheduler.

### Velocity boost causes missing NoteOff

That is a regression. Velocity adjustment is NoteOn-only. NoteOff routing, generation invalidation, panic, and wire ownership must remain unchanged.

### Generic GM file sounds like the wrong instruments

Check routing mode. `RAW` preserves source channels; `SEQTRAK` applies the accepted SEQTRAK mapping. Instrument choice/routing is separate from tempo and velocity scaling.

## Acceptance checklist

### Regression

- [ ] BOOT reaches the normal ready state.
- [ ] Internal GroovePuter audio works.
- [ ] PERFORM A/B/DX/DRUMS still works.
- [ ] Pattern MIDI and MIDI Clock/Start/Stop remain unchanged outside SMF playback.
- [ ] MIDI Player loads and plays the same files accepted before this PR.

### Tempo

- [ ] Original-speed playback sounds unchanged before touching BPM controls.
- [ ] `Up` increases effective tempo by 1 BPM.
- [ ] `Down` decreases effective tempo by 1 BPM.
- [ ] changing tempo while playing pauses cleanly at approximately the same musical position.
- [ ] `Space` resumes with the new tempo.
- [ ] `O` restores original tempo.
- [ ] triplets and syncopation remain recognizable.
- [ ] long notes and chords keep proportional duration.
- [ ] tempo-map files do not burst or collapse at tempo-change points.
- [ ] seek and restart remain correct after tempo adjustment.

### Velocity

- [ ] `V` cycles +0 / +8 / +16 / +0.
- [ ] velocity boost is audibly stronger on a velocity-sensitive SEQTRAK sound.
- [ ] high velocities saturate without wraparound.
- [ ] NoteOn velocity zero remains silent.
- [ ] changing velocity boost leaves no stuck notes.

### Stability

- [ ] repeated tempo changes do not reset/watchdog.
- [ ] repeated velocity changes do not grow queue failures.
- [ ] Panic clears SMF-owned notes.
- [ ] leaving/re-entering MIDI Player does not corrupt current settings/state.
- [ ] no new sustained audio underrun regression.

Hardware pass criterion:

```text
same faithful MIDI timing semantics
+ controllable tempo scale
+ bounded velocity boost
+ no stuck notes
+ no regression to GroovePuter standalone behavior
```
