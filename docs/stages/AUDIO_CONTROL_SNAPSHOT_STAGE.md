# Audio Control Snapshot Stage

## Purpose

Move routine, high-frequency Synth A/B parameter edits off `AudioMutationGate` pauses.

The control/UI thread now publishes a complete immutable synth-parameter snapshot into a fixed double buffer. `AudioTask` consumes pending snapshots exactly once at the existing renderer block boundary through `AudioMutationGate::waitAtAudioBoundary()`.

This stage deliberately keeps `AudioMutationGate` for structural mutations such as engine replacement, Scene load/reset, pattern-bank replacement, topology/object-lifetime changes and other operations that cannot be represented as scalar control state.

Scope of this stage:

- generic Synth A/B normalized parameters exposed through `SwappableSynthVoice`;
- coherent double-buffer publication;
- block-boundary apply;
- structural-state recapture after guarded mutations;
- no per-sample snapshot polling;
- no allocation on the snapshot path.

Explicitly out of scope:

- unified note ownership;
- event looper / performance capture;
- DSP musical validator;
- Scene schema changes;
- MIDI routing changes;
- engine/topology changes through snapshots;
- converting every drum/FX control in one PR.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3)
- USB-C data cable
- built-in speaker or 3.5 mm audio output
- optional headphones for listening for short dropouts while turning controls

## Wiring

No external wiring is required.

Cardputer-Adv internal audio is used. PORT.A is untouched; its project I2C contract remains GPIO2 SDA / GPIO1 SCL and is unrelated to this test.

## Runtime contract

Routine parameter path:

```text
UI / control thread
    -> update control-side Parameter mirror
    -> copy complete SynthParameterControlSnapshot
    -> publish inactive double-buffer slot
    -> atomic active-slot flip
    -> AudioTask block boundary
    -> apply complete snapshot to active synth voice
```

Structural path remains:

```text
UI
    -> AudioMutationGate lock
    -> renderer acknowledges at block boundary
    -> direct structural mutation
    -> capture final structural parameter state into snapshots
    -> renderer resumes
```

The block-boundary consumer is invoked before structural-pause acknowledgement. Therefore an immediate guarded Save after a knob edit sees the latest queued parameter state before the control thread proceeds.

## Build / flash

From the repository root:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
```

Run the SDL build/smoke according to the repository's normal host workflow, then flash the Cardputer-Adv using the existing Arduino CLI release workflow.

No new board flags, partitions, libraries or wiring are required.

## Expected behavior

### Screen

1. Open Synth A or Synth B parameter page.
2. Hold or repeatedly press the cutoff/resonance/envelope adjustment keys, or drag a knob in SDL.
3. The displayed value should advance continuously; rapid repeated edits must not collapse to the same stale value while waiting for an audio block.
4. Engine/type selection, Scene load and other structural operations retain their existing behavior.

### Audio

- cutoff/resonance/envelope changes become audible on the next audio block;
- no deliberate renderer pause is required for those routine normalized parameter changes;
- no clicks, broadband bursts or stuck sound should appear solely from parameter adjustment;
- existing engine-switch crossfade behavior is preserved;
- Stop/Play, Pattern, Song, SMF and MIDI timing behavior is unchanged.

### Serial / diagnostics

No new routine per-knob logging is added. Existing audio underrun/CPU diagnostics remain the source of truth.

During a five-minute parameter-twiddling test, compare underrun count against the dirty-region base branch under the same musical load.

## Troubleshooting

### Knob moves on screen but sound does not change

Check that `AudioMutationGate::setAudioTaskActive(true)` has been reached. Snapshot clients queue routine changes only while the audio task is marked active; boot-time changes are applied directly.

### Saved Scene contains the previous knob value

This indicates a structural Save path bypassed `AudioMutationGate`. Guarded mutations flush pending routine snapshots before the pause acknowledgement, then recapture final structural state before resume.

### Engine switch or Scene load is unstable

Do not convert topology/object-lifetime changes into routine snapshots. They remain guarded direct mutations in this stage.

### Host snapshot stress test hangs

The snapshot contract assumes one control writer and one audio reader. Do not add a second writer without changing the publication protocol.

## Acceptance checklist

### Automated

- [ ] `tests/test_audio_control_snapshot.cpp` passes with `-pthread`.
- [ ] concurrent snapshot stress never observes a torn correlated value.
- [ ] source regression confirms the snapshot is double-buffered and allocation-free.
- [ ] source regression confirms snapshot apply is outside `SwappableSynthVoice::process()`.
- [ ] snapshot apply occurs once through the block-boundary gate, before structural pause acknowledgement.
- [ ] guarded structural mutation recaptures the final synth parameter state.
- [ ] complete host suite passes.
- [ ] SDL build passes.
- [ ] Cardputer-Adv build with warnings enabled passes.

### Hardware

- [ ] Synth A cutoff can be moved rapidly for 30 seconds with continuous audio.
- [ ] Synth A resonance/env controls behave the same way.
- [ ] Repeat for Synth B.
- [ ] rapid repeated key/knob changes do not lose increments visually.
- [ ] no new audio underruns appear during the control stress test.
- [ ] immediately Save after a fast knob move, reboot/load, and verify the final value is retained.
- [ ] switch TB303 -> SID -> AY -> OPL2 under the existing structural guard path and verify controls remain usable.
- [ ] Scene load/reset still restores the expected parameter values.
- [ ] Pattern/Song/SMF playback and USB MIDI behavior are unchanged.

## Next stage

The next independent runtime-foundation stage is the unified note ownership registry. It must stack on this branch only after this snapshot contract is accepted; it must not be folded into this PR.
