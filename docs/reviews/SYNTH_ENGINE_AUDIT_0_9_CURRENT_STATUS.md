# Synth Engine Audit 0.9 — Current Status Addendum

## Baseline

```text
release branch: dev_0.9
base SHA: 538ae24a1c88253eb0cfc1a9a671e16091e449bf
stabilization PR: #131
hardware listening: PENDING
```

The original audit remains the evidence source. This file records disposition only.

## Fixed before PR #131

PR #125 is merged into `dev_0.9` and fixes:

- AY PSG clock and chromatic mapping;
- SN76489 playable-register octave folding and `Oct+`;
- matching the original logical NoteOff after NoteOn clamp;
- path-stable include guards required by the affected Arduino build.

Hardware listening remains pending.

## Fixed in PR #131

### TB303 amplitude lifecycle

- amplitude attack/decay/sustain/release is separate from the filter envelope;
- ordinary NoteOn retriggers;
- active legato slide does not perform a full retrigger;
- NoteOff starts a bounded release and reaches silence;
- reset/Panic clears the active voice;
- the sample-processing path adds no new allocation.

### TB303 output ownership

- visible `Volume` is applied once;
- optional sub is mixed once;
- tests cover finite output, monotonic Volume RMS and bounded sub level.

### Distortion enable

- an invalid low drive is restored to a safe audible default;
- a valid user drive is preserved;
- disabling does not alter drive;
- Synth A and B retain independent instances.

Focused coverage:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_tb303_release_contract.cpp \
  src/dsp/mini_tb303.cpp src/dsp/filter.cpp \
  src/dsp/audio_wavetables.cpp src/dsp/tube_distortion.cpp \
  -o build/test_tb303_release_contract
./build/test_tb303_release_contract
```

## P0 still open

### Versioned synth persistence

The Scene codec still needs independent Synth A/B TYPE plus normalized parameters `0..5`, explicit version dispatch, deterministic engine-specific legacy defaults and failed-decode rollback.

Legacy TB303 raw values must not become normalized values for another engine. Legacy OPL2 remains decode-only and maps to TB303.

### Loaded patch ownership

Normal Scene Load must guarantee:

```text
stored TYPE + stored parameters win
```

Genre timbre must not run as hidden post-load replacement. It remains valid only for explicit sound-changing actions.

## Remaining P1

- engine-aware neutral defaults for AY/SH101/SN/WAVEMORPH;
- explicit Save/Load revision success wiring;
- TB303/SID DC protection, realtime filter allocation, broad loudness and aliasing work remain deferred unless separately proven low-risk;
- parameter-range design remains only partially mitigated by held acceleration.

## Hardware checklist

- [ ] exact candidate SHA recorded;
- [ ] AY/SN pitch and logical NoteOff accepted on Cardputer ADV;
- [ ] TYPE and parameters `0..5` survive Save/reboot/Load for every engine on A and B;
- [ ] normal Load does not apply genre timbre over the stored patch;
- [ ] TB303 trigger/accent/slide/release/Volume/sub/Panic accepted;
- [ ] neutral patches do not start with unintended maximum noise;
- [ ] no allocation failure, watchdog reset or growing underrun count.

PR #131 remains draft until the open P0 items are implemented and hardware acceptance is recorded.
