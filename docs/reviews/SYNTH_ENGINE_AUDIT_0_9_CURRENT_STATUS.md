# Synth Engine Audit 0.9 — Current Status Addendum

## Baseline

```text
release branch: dev_0.9
base SHA: 0afd24fe5f4b0b2d549214abe2ed1a7eb7f3448c
stabilization PR: #131
hardware listening: PENDING
```

The original audit remains the evidence source. This file records disposition only.

## Fixed before/folded into the final stabilization baseline

### PR #125 — AY/SN pitch and note lifecycle

- AY PSG clock and chromatic mapping;
- SN76489 playable-register octave folding and `Oct+`;
- matching the original logical NoteOff after NoteOn clamp;
- path-stable include guards required by the affected Arduino build.

### PR #143 — synth persistence and Scene Load ownership

- independent Synth A/B stable TYPE plus normalized parameters `0..5` are persisted in versioned `synthState` schema v1;
- legacy TB303-shaped `synthParams` remain decode-only input and are not emitted by new saves;
- malformed or unknown versioned synth state rejects the Scene transaction without half-loading one voice;
- legacy TB303 raw units keep their historical meaning;
- legacy non-TB scenes without patch data keep engine-native runtime defaults rather than inheriting TB303 raw defaults;
- normal Scene Load restores the saved patch and does not apply Genre timbre/texture projection over it;
- quick Save during a synth engine crossfade snapshots the pending public voice consistently;
- explicit Save/Load/recovery revision state follows operation success rather than function entry.

The exact #143 head passed focused persistence tests, host regressions, Phrase Core, SDL, Cardputer ADV + fixed DRAM and SEQTRAK MIDI-only before merge.

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

### FEEL/Genre revision ownership

- browsing remains preview-only;
- committed FEEL/Genre actions produce one logical Scene mutation;
- explicit Save/Load success semantics are supplied by the #143 baseline.

Focused coverage includes the TB303/DST release contracts, scene revision contracts, Scene codec round-trip, real swappable synth persistence and source ownership checks.

## Release disposition

There are no remaining P0/P1 synth-persistence blockers owned by #131.

Critical-path policy:

- #132 GenreManager ownership refactor is deferred until after 0.9;
- #142 AY articulation follow-up is deferred until after 0.9;
- #139 SID articulation is optional only if a Cardputer ADV listening smoke passes cleanly;
- #134 TextureMode runtime removal is allowed before RC only if integration with the #143 Scene migration/load contract is demonstrably low-risk; otherwise it is deferred.

Realtime filter allocation, additional per-voice DC protection, broad loudness and aliasing work remain deferred unless a focused release blocker is demonstrated.

## Hardware checklist

- [ ] exact candidate SHA recorded;
- [ ] AY/SN pitch and logical NoteOff accepted on Cardputer ADV;
- [ ] TYPE and parameters `0..5` survive Save/reboot/Load for every engine on A and B;
- [ ] normal Load does not apply genre timbre over the stored patch;
- [ ] legacy non-TB missing-patch defaults are sane;
- [ ] failed Save/Load and recovery revision behavior is correct;
- [ ] TB303 trigger/accent/slide/release/Volume/sub/Panic accepted;
- [ ] no allocation failure, watchdog reset or growing underrun count.

PR #131 remains the final release-candidate PR until exact-head automated gates and required hardware acceptance are recorded.
