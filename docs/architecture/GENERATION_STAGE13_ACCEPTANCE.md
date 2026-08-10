# Generation Stage 13 — Composition Matrix Acceptance

## Scope delivered

Stage 13 composes the currently persisted production catalog through one
read-only profile matrix:

- 9 base `GenerativeMode` profiles;
- 11 admitted Genre/Variant pairs;
- the Stage 7C Rhythm compatibility owner by reference;
- weighted Feel, BassRhythm, ChordRhythm, MelodicRhythm and Motif edges;
- weighted 1/2/4/8-bar phrase-law choices;
- BPM, grid and density corridors;
- an explicit Chord/Melodic semantic secondary role.

The matrix contains stable vocabulary IDs and weights only. It owns no event
masks, pitches, synth TYPE, physical binding or Scene destination. Weighted
selection canonicalizes stable IDs, so declaration order cannot change a
result. Unknown or mismatched Variant intent falls back to the base profile in
the data resolver; the conservative production allow-list still routes unknown
recipes to legacy generation.

## Production reachability

The normal materialization bridge now resolves one composition plan and passes
its explicit Bass, Chord, Melodic and Motif IDs into the Stage 9–11 realizers.
Rhythm AUTO/manual remains the persisted Stage 7C owner.

The selected Feel is advisory because `Scene::feel.timingProfile` is explicit
user intent and therefore has higher precedence. The bridge continues to apply
that persisted profile. Phrase-law and 2/4/8-bar choices are also exposed in the
transient result but are not executed by production until the Stage 6.1
physical ESP32-S3 measurements permit a production BarEvolution caller.

## Host acceptance

- every current production profile has non-empty valid adjacent edges;
- same profile + seed + phrase ordinal reproduces the same composition;
- manual Rhythm identity stays fixed;
- weighted choice is invariant under candidate reordering and duplicates;
- every matrix-selected role can be realized against every compatible Rhythm;
- Trip-Hop exercises low-BPM, laid-back, sparse, long-hold and rest-heavy
  primitives without a bespoke pattern generator;
- the Drum & Bass profile contains no four-floor Rhythm family;
- production projection remains transactional;
- GCC, Clang, ASan/UBSan, `-Werror` and `-Wvla` pass.

## Deferred completion gates

The roadmap's full Stage 7–13 program Definition of Done is not claimable in
this environment:

- no new persisted top-level Lo-Fi/House/Funk genre IDs are assigned by this
  stage; the roadmap explicitly treats that list as a curation target rather
  than a frozen enum;
- no physical Cardputer ADV listening, DRAM or worst-case execution evidence is
  available;
- the Stage 6.1 hardware gate blocks production multi-bar evolution;
- SEQTRAK MIDI-only hardware/build validation is unavailable here.

```text
STAGE13_HOST_MATRIX = PASS
STAGE13_ROLE_PRODUCTION_REACHABILITY = PASS
STAGE13_PHRASE_PRODUCTION_REACHABILITY = BLOCKED_BY_STAGE_6_1_HARDWARE_GATE
FULL_PROGRAM_HARDWARE = HARDWARE_PENDING
```
