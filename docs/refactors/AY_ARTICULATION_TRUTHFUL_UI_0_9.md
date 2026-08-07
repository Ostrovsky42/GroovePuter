# GroovePuter 0.9 — AY articulation and truthful controls

## Purpose

Finish the remaining AY-specific articulation defects without touching the AY pitch policy already repaired in PR #125, Scene persistence, genre ownership, transport, Song, pattern paging, or cross-engine gain architecture.

This is a release-stabilization change, not a full AY emulator rewrite.

## Base

- repository: `Ostrovsky42/GroovePuter`
- branch: `agent/ay-articulation-truthful-ui`
- base: `dev_0.9`
- keep the PR draft until automated gates and Cardputer ADV listening smoke pass

## Existing behavior that must remain

- AY uses the dedicated 1.7734 MHz PSG clock and the current `ChipTuning::quantizeAyToneFrequency()` policy from #125;
- C1..B4 remains chromatic/monotonic under the existing host tuning regression;
- engine id remains exactly `AY`;
- parameter count remains 4;
- parameter indices remain:
  - 0 Noise
  - 1 Decay
  - 2 current Chorus/Spread control
  - 3 Env
- existing parameter persistence semantics must not change in this PR;
- GrooveboxMode and LoFi hooks remain compatible.

## Confirmed defects on current dev_0.9

`AySynthVoice::startNote()` currently ignores `accent` completely.

`slideFlag` only prevents `ampSlew_` from being reset. The pitch itself jumps directly to the new quantized frequency, so there is no portamento.

`velocity = 0` is clamped to a non-zero gain because velocity gain is currently bounded to at least `0.05f`.

`Env=Gate` is not a true held gate. While the key is held, `env_` still decays with a roughly 4 second coefficient.

The parameter named `Chorus` controls detune/spread of channels B/C, while channel C remains an always-present sub-octave voice even when the value is zero. The current label over-promises a conventional chorus control.

## Required implementation

### 1. Velocity contract

`velocity == 0` must produce silence.

For velocity 1..127, preserve the current normalized response shape as closely as possible. Do not introduce a new velocity curve in this PR.

### 2. Accent

Use the existing `accent` argument.

Requirements:

- accent must be audible but bounded;
- target: approximately +15% amplitude before the final engine output scaling;
- accent must not alter tuning, envelope mode, noise amount, or saved parameters;
- no clipping/NaN/Inf may be introduced;
- accent applies to the triggered note and naturally follows its envelope.

Do not add a new persisted Accent parameter.

### 3. Real slide / legato

Implement a sample-rate-independent portamento for an already-active AY voice.

Requirements:

- first note with `slideFlag=true` behaves like a normal trigger;
- slide only becomes legato when a previous AY note is still active;
- active slide must preserve oscillator phase rather than hard-resetting channels A/B/C;
- target frequency must still pass through the existing AY pitch quantization contract from #125;
- use a bounded, sample-rate-independent glide time around 40–50 ms;
- 22.05 kHz and 44.1 kHz host runs must produce approximately the same glide time;
- non-slide note starts at the requested quantized pitch immediately;
- release during glide must still terminate normally;
- no heap allocation in `process()`.

Do not change `ChipTuning` or the AY master-clock constants.

### 4. Truthful Gate envelope

Make `Env=Gate` match its label.

Required behavior:

- while gate is held, the envelope stays at sustain/full held level rather than decaying away;
- after `release()`, use the existing short gate-style release region, approximately 35 ms;
- release must be bounded and reach silence;
- Hold, Decay and Pluck retain their current broad musical roles;
- do not add new envelope parameters.

### 5. Truthful parameter label

Rename parameter index 2 from:

`Chorus`

to:

`Spread`

Reason: the control changes detune/spread while the fixed sub-octave channel remains present at zero. Renaming the label is lower-risk and more truthful than redesigning the oscillator topology before 0.9.

Do not change the parameter index, range, normalization, or persisted value.

Keep `Noise` unchanged in this PR even though it currently affects both level and clock; splitting it would require a new persisted parameter and is outside this release scope.

## Reset requirements

`reset()` must clear all transient articulation state introduced by this PR:

- current/target glide frequency state;
- gate/active state;
- accent gain state;
- envelope state;
- amplitude smoothing state.

It must preserve the existing patch parameters exactly as before.

## Focused host regression

Add a focused test, preferably:

`tests/test_ay_articulation.cpp`

Compile the real AY voice implementation and real `ChipTuning` helpers used by production code.

Minimum assertions:

1. velocity 0 produces silence;
2. normal velocity produces finite non-zero output;
3. accent RMS/peak is measurably above non-accent but remains bounded;
4. first note with `slide=true` does not glide from stale/default frequency;
5. active slide reaches the new target over a bounded ~40–50 ms interval;
6. slide duration at 22.05 kHz and 44.1 kHz is approximately equivalent in milliseconds;
7. non-slide retarget is immediate;
8. `Env=Gate` remains held while the note is held;
9. Gate release reaches silence after a bounded tail;
10. reset silences the voice and clears glide state;
11. AY C1..B4 tuning regression from #125 remains green;
12. parameter count is still 4;
13. parameter 2 label is `Spread`;
14. engine name is still exactly `AY`.

A small dedicated workflow may be added if needed, but do not weaken or replace existing Core regressions.

## Explicit boundary

Do not modify:

- `scenes.h` / `scenes.cpp` persistence schema;
- synth parameter count or indices;
- `TextureMode` migration (#134);
- `GenreManager` ownership work (#132);
- TB303/DST stabilization (#131);
- SID implementation (#139);
- AY pitch clock/divider policy from #125;
- Song or Generation behavior;
- Phrase Core;
- MIDI routing, clock or transport;
- pattern paging/storage;
- broad loudness normalization;
- PolyBLEP/oversampling/aliasing architecture;
- AY noise control split;
- logarithmic 4-bit amplitude redesign.

## Hardware assumptions

- M5Stack Cardputer ADV;
- built-in audio path at the project sample rate;
- built-in keyboard or normal MIDI/performance input;
- no external wiring required;
- PORT.A is not used by this test.

## Build / flash

From the exact PR head run the repository's normal release matrix, including:

- host regressions;
- focused AY articulation regression;
- Four-axis UI;
- Phrase Core;
- SDL build;
- Cardputer ADV firmware build;
- fixed DRAM budget;
- Cardputer ADV SEQTRAK MIDI-only build.

Flash the exact same head to Cardputer ADV before listening acceptance.

## Expected behavior

- AY pitch from #125 is unchanged;
- ordinary note starts remain immediate and stable;
- accented notes are slightly louder, not radically different;
- held Gate notes remain held;
- NoteOff in Gate mode has a short clean release;
- active slide gives a short audible portamento without oscillator restart;
- a first note marked slide starts normally;
- zero-velocity notes are silent;
- UI shows `Spread` instead of `Chorus`.

## Troubleshooting

If pitch regressions appear, stop and compare the change against `ChipTuning::quantizeAyToneFrequency()`; this PR must not alter the #125 tuning policy.

If slide duration changes with sample rate, replace any per-sample fixed coefficient with a coefficient derived from sample rate and target glide time.

If Gate still fades while held, check for the generic decay branch applying to Env option 3 before release.

If accent clips, reduce the bounded accent multiplier; do not compensate by changing the global AY output gain.

If persistence tests change, revert any Scene-format edits; this PR must not require persistence migration.

## Acceptance checklist

Automated:

- [ ] focused AY articulation test passes with real production sources;
- [ ] #125 AY tuning regression remains green;
- [ ] full host regressions green;
- [ ] Four-axis UI green;
- [ ] Phrase Core green;
- [ ] SDL build green;
- [ ] Cardputer ADV build green;
- [ ] fixed DRAM gate green;
- [ ] SEQTRAK MIDI-only build green.

Cardputer ADV listening:

- [ ] normal AY note has no new click or pitch error;
- [ ] velocity 0 is silent;
- [ ] accent is audible but modest;
- [ ] first slide-marked note starts normally;
- [ ] active slide produces a short smooth/stepped-PSG portamento rather than an instant pitch jump;
- [ ] Gate holds for at least 2 seconds without fading away;
- [ ] Gate NoteOff releases cleanly and reaches silence;
- [ ] Decay and Pluck still sound recognizably as before;
- [ ] UI displays `Spread`;
- [ ] no stuck note, reset, watchdog, NaN/Inf, or growing underrun behavior.

## Merge policy

Keep draft until the automated matrix is green and the physical Cardputer ADV listening checklist is recorded. Rebase/synchronize with `dev_0.9` after the other parallel release PRs merge, then repeat the relevant gates before final merge.
