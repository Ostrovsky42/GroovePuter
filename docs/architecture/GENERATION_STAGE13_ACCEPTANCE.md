# Generation Stage 13 — Composition Matrix Acceptance

Status: `HOST-COMPLETE` for composition selection; the full Stage 7C–13 program
is not complete.

## Scope delivered

Stage 13 composes the currently persisted production catalog through one
read-only profile matrix:

- 9 base `GenerativeMode` profiles;
- 11 admitted Genre/Variant pairs;
- the Stage 7C Rhythm compatibility owner by reference;
- weighted Feel, BassRhythm, ChordRhythm, MelodicRhythm and Motif edges;
- weighted phrase-law and phrase-length intent, kept transient until shipped
  multi-bar execution is reachable;
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

The shipped `ReferenceVocabulary` adds a second independent blocker: all 24
archetypes allow one bar only and reference the single one-bar Statement
trajectory. Stage 12 therefore remains fixture-tested API, not a shipped
1/2/4/8-bar user capability.

## Physical secondary-role limit

The production binder has two monophonic synth lanes:

```text
Synth A = Bass
Synth B = Chord XOR Melodic
```

The matrix carries both Chord and Melodic candidate IDs for compatibility
planning, but `CompositionSecondaryRole` chooses only one physical Synth B role
per materialization. Stages 7C–13 cannot render an independent held chord and
melody simultaneously.

## Lo-Fi target verdict

The roadmap's first practical Lo-Fi / Chill-Hop target is **not met by this
stage**:

- no Lo-Fi / Chill-Hop profile exists among the 9 base modes and 11 admitted
  Variant pairs in this PR;
- Trip-Hop is only a proxy and selects Chord for Synth B;
- the target combination of held Chord plus independent rest-heavy Melody is
  outside the two-lane physical binding described above.

A later Genre-expansion PR may add Lo-Fi composition rows using the generic
primitives, but it cannot claim the full simultaneous Chord+Melody target
without a separate voice-allocation decision.

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

## Reviewed stage status

| Stage | Honest status in this PR | Remaining gate |
|---|---|---|
| 7C | host/production path implemented | routing hardware smoke |
| 8 | host/production path implemented | LaidBack hardware listening |
| 9 | `PARTIAL` | BassPitchContour, BassArticulation, Acid audition |
| 10 | host/production path implemented | held-note hardware listening |
| 11 | host-complete for `Chord XOR Melodic` | explicit voice expansion for simultaneous roles |
| 12 | `API-ONLY` | shipped vocabulary expansion plus Stage 6.1 physical gate |
| 13 | host-complete composition selection | Lo-Fi target and full hardware curation |

```text
STAGE13_HOST_MATRIX = PASS
STAGE13_ROLE_PRODUCTION_REACHABILITY = PASS
STAGE9_COMPLETION = PARTIAL
STAGE12_COMPLETION = API_ONLY
STAGE12_PHRASE_PRODUCTION_REACHABILITY = BLOCKED_BY_VOCABULARY_AND_STAGE_6_1
LOFI_TARGET = NOT_MET
FULL_PROGRAM_HARDWARE = HARDWARE_PENDING
```
