# SID articulation / truthful UI — GroovePuter 0.9

## Purpose

Close the remaining SID-specific articulation and misleading-control findings from `docs/reviews/SYNTH_ENGINE_AUDIT_0_9.md` without overlapping the active TextureMode migration (#134), final TB303/DST stabilization (#131), synth persistence work, or broad gain/aliasing work.

This PR is intentionally narrow. Keep persisted engine id/name `SID` unchanged.

## Current defects to close

Current `SidSynthVoice::startNote()` receives `accent` and `slideFlag` but discards both. `SidSynth::startNote()` resets phase on every trigger and `stopNote()` immediately sets the voice inactive, so release is a hard cut. Velocity 0 is clamped to an audible minimum. The current `Reso` control does not create resonance, and the `BP` filter mode is not a true band-pass response.

Relevant files:

- `src/dsp/sid_synth.h`
- `src/dsp/sid_synth.cpp`
- `src/dsp/sid_synth_voice.h`
- `src/dsp/sid_synth_voice.cpp`
- focused SID host tests

Do not modify `scenes.h`, `scenes.cpp`, `genre_manager.*`, pattern paging, MIDI transport, Phrase Core, or TextureMode code in this PR.

## Required behavior

### 1. Bounded amplitude lifecycle

Add an allocation-free amplitude contour to SID.

Requirements:

- normal NoteOn uses a short click-safe attack;
- NoteOff starts a bounded release instead of an immediate cut;
- release reaches silence and then deactivates the voice;
- velocity 0 produces silence;
- reset/Panic leaves the voice inactive and silent;
- repeated normal triggers remain deterministic;
- no heap allocation from `process()`.

Use the existing SH101 articulation as the behavioral reference, not as a code-copy requirement. A short attack around 1–2 ms and release around 70 ms are acceptable defaults for 0.9 if represented in sample-rate-independent time constants.

### 2. Accent

`accent=true` must produce an audible but bounded articulation change.

Minimum contract:

- accent raises amplitude modestly, bounded below clipping;
- accent may also open the filter slightly if implemented without changing the persisted parameter model;
- accent must not change pitch or parameter storage.

### 3. Slide / legato

`slideFlag=true` on an already-active voice must perform real portamento instead of retriggering from a phase reset.

Requirements:

- keep current oscillator phase for an active legato slide;
- glide from current frequency to target frequency;
- use sample-rate-independent glide timing;
- first note of a phrase with slide set must still start normally rather than glide from stale/default state;
- non-slide NoteOn may retrigger the phase as today if click-safe attack prevents a hard discontinuity.

Do not add polyphony.

### 4. DC safety

The pulse-width extremes can create large DC before per-voice DST/DLY. Add a cheap, stable per-voice DC blocker in the SID signal path.

Requirements:

- no allocation;
- stable at 22.05 kHz and desktop sample rates;
- reset clears DC-blocker history;
- neutral center pulse remains audibly unchanged apart from negligible DC removal.

Do not introduce oversampling or a new oscillator architecture.

### 5. Truthful parameter labels

Keep parameter indices, normalized values, engine id and persistence semantics unchanged.

The existing implementation is not a true SID filter. Therefore make the visible labels describe the actual DSP rather than claiming features that do not exist.

Required UI-facing naming:

- parameter 1: replace `Reso` with a truthful damping/filter-character label such as `Damp`;
- filter option index 1: replace `BP` with a truthful label such as `EDGE`;
- filter option index 3: replace `OFF` with `RAW`.

Do not rename persisted engine `SID` to `SID-LITE` in this PR because engine names are part of scene compatibility.

Do not change the normalized mapping/range of Cutoff or P-Width here; persistence owns that decision.

## Explicitly deferred

Do not include:

- versioned synth persistence or parameter 5 storage;
- TYPE ownership on scene load;
- neutral defaults for other synth engines;
- broad inter-engine loudness normalization;
- true SID chip emulation;
- new resonant multimode filter architecture;
- PolyBLEP/oversampling/aliasing pass;
- generic knob acceleration/range scaling;
- TextureMode or GenreManager refactors;
- Song/Generation changes.

## Host regression requirements

Add focused tests that prove:

1. velocity 0 is silent;
2. NoteOn attack does not jump immediately to full steady-state amplitude;
3. NoteOff produces a non-zero tail and reaches silence within a bounded interval;
4. reset immediately returns to silence;
5. accent RMS/peak is measurably above the same normal note without exceeding safe bounds;
6. active slide does not hard-reset phase and converges toward the new pitch;
7. first-note slide behaves as a normal first trigger;
8. long pulse-width-extreme rendering has near-zero DC after the blocker;
9. parameter labels are `Damp`, `EDGE`, and `RAW` while parameter indices stay 0..3.

The tests must compile the real SID sources, not a duplicated model.

## Required validation

Before ready-for-review, obtain green:

- `bash tests/run_host_tests.sh`;
- focused SID regression test;
- Four-axis UI;
- Phrase Core;
- SDL build;
- Cardputer ADV firmware build;
- fixed DRAM budget;
- Cardputer ADV SEQTRAK MIDI-only build.

Do not weaken an existing assertion merely to make CI green.

## Hardware assumptions

Target hardware: M5Stack Cardputer ADV, ESP32-S3, project sample rate 22.05 kHz, mono GroovePuter audio path. No extra wiring is required; use the normal audio/headphone output. FX must be disabled for articulation comparison unless a step explicitly tests DST/DLY interaction.

## Hardware acceptance

For SID on both Synth A and Synth B:

1. FX OFF, track volume unchanged.
2. Play C2, A2, A3, A4 at velocity 64 and 100.
3. Confirm NoteOn has no obvious hard click.
4. Hold for about one second, release, and confirm a short audible release tail rather than an immediate cut.
5. Repeat normal note vs accent and confirm accent is clearly stronger without a large level jump.
6. Play a held note and trigger an adjacent note with slide; confirm continuous portamento rather than a fresh attack from silence.
7. Set P-Width near both extremes and confirm there is no large DC thump or apparent disappearance caused by DC offset.
8. Confirm the UI shows `Damp`, `EDGE`, and `RAW` for the existing parameter/mode slots.
9. Confirm no watchdog reset, Guru Meditation, increasing underruns, or stuck note.

## Acceptance checklist

- [ ] SID normal NoteOn has click-safe attack.
- [ ] SID NoteOff has bounded release and reaches silence.
- [ ] velocity 0 is silent.
- [ ] accent is implemented and bounded.
- [ ] active slide is real legato portamento.
- [ ] first-note slide does not use stale frequency.
- [ ] per-voice DC blocker is active and reset-safe.
- [ ] parameter indices and persisted engine name remain unchanged.
- [ ] misleading `Reso/BP/OFF` labels are replaced with truthful names.
- [ ] no changes to scene persistence or TextureMode migration.
- [ ] host/SDL/Cardputer/fixed-DRAM/SEQTRAK gates are green.
- [ ] hardware listening smoke is recorded before merge.

## Merge boundary

Keep this PR draft until automated gates pass. Do not merge based only on host tests; SID articulation and click/DC behavior need a short Cardputer ADV listening check.
