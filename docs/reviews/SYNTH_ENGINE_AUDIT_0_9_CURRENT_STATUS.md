# Synth Engine Audit 0.9 — Current Status Addendum

## Purpose

Record the disposition of findings after PR #125 and during draft stabilization PR #131 without rewriting the original source audit or claiming Cardputer ADV acceptance.

## Baselines

```text
release branch: dev_0.9
release base SHA: b28c63801660c9d024e4aad57716d534744fa324
stabilization PR: #131
hardware listening: PENDING
```

The exact candidate SHA is the final frozen head of PR #131 and must be copied into `docs/releases/0_9_FINAL_ACCEPTANCE.md` before flashing.

## Fixed before this PR

PR #125 is merged into `dev_0.9` and closes the code paths for:

- AY PSG clock and chromatic C1–B4 mapping;
- SN76489 playable-register octave folding while preserving pitch class;
- SN `Oct+` behavior;
- matching the original logical NoteOff after NoteOn clamp;
- include guards required by the affected aliased Arduino paths.

These items retain hardware-listening status `PENDING`.

## Fixed in draft PR #131

### TB303 amplitude lifecycle

- amplitude attack/decay/sustain/release is separate from the filter envelope;
- ordinary NoteOn retriggers the amplitude envelope;
- active legato slide does not perform a full retrigger;
- NoteOff starts a bounded release and reaches silence;
- reset/Panic clears the active voice;
- the sample-processing path adds no new allocation.

### TB303 output ownership

- visible `Volume` is applied once;
- optional sub is mixed once;
- numeric tests cover finite output, monotonic Volume RMS and a bounded sub-level difference.

### Distortion enable policy

- enabling with drive below the working threshold restores one safe default;
- a valid user drive is preserved;
- disabling does not change drive;
- Synth A and Synth B retain independent instances.

Focused host coverage is in `tests/test_tb303_release_contract.cpp` and the `Release 0.9 stabilization` workflow.

## P0 still open

### Versioned synth persistence

The Scene JSON contract still needs a versioned engine-aware payload that stores, independently for Synth A and B:

- TYPE;
- normalized parameters `0..5`;
- explicit parameter count/version;
- deterministic engine-specific defaults for fields absent from legacy Scenes.

Legacy TB303 raw fields must not be reinterpreted as normalized values for another engine. Legacy OPL2 remains decode-only and maps to TB303.

### Loaded patch ownership

Normal Scene Load must guarantee:

```text
stored TYPE + stored parameters win
```

Genre timbre application must not run as hidden post-load replacement. It remains valid only for explicit Apply/new-default/regeneration operations whose UI contract promises a sound change.

Until both P0 items have round-trip, legacy and failed-load tests, PR #131 remains draft.

## P1 status

- non-TB engine-aware neutral defaults: open; must be completed with legacy version dispatch;
- TB303/SID DC before per-voice effects: deferred unless separately proven low-risk;
- realtime filter allocation: deferred under the release stop condition;
- cross-engine loudness and aliasing: hardware-led deferred work;
- parameter range design: held acceleration is only a partial mitigation;
- FEEL/TEXTURE/GENRE revision evidence: open in the final mutation matrix.

## Hardware assumptions

- M5Stack Cardputer ADV;
- built-in mono audio path at the repository sample rate;
- headphones recommended;
- no external GPIO wiring required.

## Build and flash

```bash
git fetch origin
git switch release/0.9-final-stabilization
git reset --hard origin/release/0.9-final-stabilization
rm -rf build .pio .pioenvs .piolibdeps
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
```

Flash using the normal Cardputer ADV upload command and monitor serial at `115200` baud.

## Exact verification commands

```bash
# Focused TB303/DST contract
g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_tb303_release_contract.cpp \
  src/dsp/mini_tb303.cpp \
  src/dsp/filter.cpp \
  src/dsp/audio_wavetables.cpp \
  src/dsp/tube_distortion.cpp \
  -o build/test_tb303_release_contract
./build/test_tb303_release_contract

# Existing suite
bash tests/run_host_tests.sh
```

## Acceptance checklist

- [x] AY/SN and logical NoteOff code fixes are present from #125.
- [x] TB303 numeric lifecycle/DST contract passes in host CI.
- [ ] Cardputer ADV confirms AY/SN pitch and NoteOff cleanup.
- [ ] TYPE and parameters `0..5` survive Save, reboot and Load for all engines on A and B.
- [ ] normal Load does not apply genre timbre over the stored patch.
- [ ] legacy Scenes receive correct engine-specific param-5 defaults.
- [ ] malformed or unknown persistence versions leave the active Scene unchanged.
- [ ] TB303 trigger, accent, slide, sustain, release, Volume, sub and Panic pass listening acceptance.
- [ ] no engine starts with unintended maximum noise.
- [ ] serial shows no allocation failure, watchdog reset or growing underrun count.

The original audit remains the evidence source; this addendum is the current disposition record.
