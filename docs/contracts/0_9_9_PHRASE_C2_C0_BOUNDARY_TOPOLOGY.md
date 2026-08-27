# 0.9.9 PHRASE-C2-C0 — Cross-Bar Melodic Boundary Topology

## Purpose

Characterize the frozen P1R production-reachable post-admission melodic boundary topology without changing production musical semantics.

## Frozen P1R base

`016bcd6ba514b3a57f8803c63c869f1b2a8953a7`

P1R decision: `DECISION A — PRODUCTION PHRASE EXECUTION READY`.

## Question

Does the existing production corpus contain a natural pure-melodic boundary with semantic occupancy at step 15, an initial gap in the next bar, and a later admitted onset of the same melodic voice, without adding duration, articulation, hybrid arbitration, or vocabulary policy?

Answer: **YES**.

The demonstrated production-reachable subclass is **A-ONSET**. No production-reachable **A-CONTINUATION** or **A-OVERLAP** exists in the frozen attempt-0 corpus.

## Non-goals

No lifetime producer, Continue/Release policy, gate change, transport/MIDI/internal-synth runtime change, Song/Bank publication, new melodic vocabulary, hardware listening, or production probe.

## Production firewall

`src/` remains byte-identical to the frozen P1R base. The focused runner executes both an opening and final firewall:

```sh
git diff --exit-code 016bcd6ba514b3a57f8803c63c869f1b2a8953a7 -- src/
```

## Authoritative corpus domain

The final corpus fixes:

```text
generationAttemptOrdinal = 0
rhythmSelectionMode       = AUTO
generative modes          = 16
recipe IDs                = 18
phraseGenerationIdentity  = 0..65534
0xFFFF                    = kUnspecifiedPhraseGenerationIdentity sentinel
candidate lengths         = 1 / 2 / 4 / 8
```

The complete Cartesian request domain is:

```text
16 * 18 * 65535 * 4 = 75,496,320 request tuples
```

Every tuple goes through the exact P1R phrase-length resolver. Only admitted exact lengths contribute phrases and boundaries.

Measured totals:

```text
request tuples                  75,496,320
active settings                        288
legacy settings                          0
admitted phrases                41,418,120
adjacent intra-phrase boundaries 96,729,660
unique boundary signatures         294,725
pure-melodic boundaries         52,296,930
pure-melodic boundaries with
non-empty incoming bar          41,306,411
terminal N2 phrase controls     41,418,120
unique terminal N2 signatures     104,104
```

This is exhaustive for the frozen attempt-0 production semantics over the current finite `uint16_t` identity domain excluding only the explicit sentinel. It does **not** make claims about nonzero `generationAttemptOrdinal` rerolls/retries.

## Corpus construction

The authoritative observation path is test-side deterministic replay of the same public production owners used by strong-rhythm materialization:

1. exact frozen phrase selection;
2. production rhythm realization;
3. BassRhythm admission;
4. ChordRhythm admission;
5. MelodicMotif admission;
6. MelodicPitchIntent admission;
7. hybrid-only monophonic chord masking when applicable;
8. full `preparePhraseExecution()` plus `materializePreparedPhraseBar()` replay for representative witnesses.

Raw melodic rhythm names or vocabulary masks are never used as the final classifier.

## Production dependency inventory

Topology dependencies are the selected composition/profile, rhythm archetype/family, exact phrase length admission, frozen selection and realization generation coordinates, realized kick topology, role-specific protected space, BassRhythm result, ChordRhythm result, semantic Synth-B role, MelodicMotif result, bar/vocabulary coordinate, sparse-bar legality, and MelodicPitchIntent result.

Current production tonal profiles allow only `MelodicRhythmOperationId::Preserve`; contour and motif pitch operations do not move onset/continuation coordinates. The source proof also enumerates every frozen melodic rhythm/bar and finds no continuation occupancy on logical step 15.

## Step-mask convention

GroovePuter uses reversed 16-step mask bits:

```text
logical step 0  = bit15
logical step 15 = bit0
```

Therefore:

```text
0x0009 = logical step12 + logical step15
```

## Signature definition

A boundary signature is a **post-observation deduplication key**, not a pre-realization shortcut.

It contains:

```text
phraseBars
boundary ordinal
evolution-seam flag
rhythm archetype ID

for outgoing and incoming bars:
  allowSparse
  rhythm family
  semantic Synth-B role
  BassRhythm status
  ChordRhythm status
  melodic rhythm ID
  motif shape
  MelodicMotif status
  MelodicPitchIntent status
  kick onsets
  bass onsets
  chord onsets
  chord continuations
  protected melodic space
  admitted melodic onsets
  admitted melodic continuations
```

The final classifier depends only on information represented in that observed signature plus terminal-vs-adjacent context.

## Class precedence

Adjacent boundaries are classified exactly once with this precedence:

1. any pure Chord side -> `OTHER`;
2. incoming melodic bar empty / `ValidButEmpty` -> `N1`;
3. incoming admitted melodic onset at logical step 0 -> `N0`;
4. any `ChordWithMelodicFill` side:
   - outgoing occupancy + later incoming onset -> `H`;
   - otherwise no outgoing step15 -> `N3`;
   - otherwise -> `OTHER`;
5. pure Melodic on both sides, later incoming onset, outgoing step15:
   - onset15 + continuation15 -> `A_OVERLAP`;
   - onset15 -> `A_ONSET`;
   - continuation15 -> `A_CONTINUATION`;
6. pure Melodic, later incoming onset, outgoing occupancy but no step15 -> `B`;
7. no outgoing step15 -> `N3`;
8. otherwise -> `OTHER`.

`N2` is not an adjacent-boundary class. Every admitted phrase contributes one separate terminal phrase control.

The adjacent raw classes sum exactly to `96,729,660`; the unique class counts sum exactly to `294,725`.

## Signature sufficiency proof

The signature is sufficient for the frozen classifier because it retains every observed field that can affect semantic role, melodic validity/emptiness, admitted onset/continuation topology, final-step occupancy, incoming step-0 occupancy, and the 3->4 evolution-seam distinction.

This claim is deliberately narrow: equal signatures mean equal C2-C0 boundary classification under the frozen observation pipeline. They are not claimed to be globally musically interchangeable phrases.

## Signature collision validation

Distinct production origins sharing one signature are replayed through production owners and must reproduce the exact same signature and class.

Measured validation:

```text
collision groups                 290,961
replays                          729,398
profile-diverse collision groups 147,476
result                           PASS
```

For every repeated signature the first distinct production origin is replayed; when a profile-distinct origin exists it is replayed as well. No classification/signature collision was observed.

## Raw and unique class counts

| Class | Raw | Unique signatures | Final status |
|---|---:|---:|---|
| `A_ONSET` | 17,530,610 | 30,408 | REACHABLE |
| `A_CONTINUATION` | 0 | 0 | UNREACHABLE under frozen attempt-0 semantics |
| `A_OVERLAP` | 0 | 0 | ZERO / no ownership ambiguity observed |
| `B` | 22,115,006 | 33,632 | characterized |
| `H` | 9,276,932 | 80,113 | characterized / excluded from bootstrap C2 |
| `N0` | 1,644,348 | 1,408 | negative control |
| `N1` | 21,660,687 | 87,948 | negative control |
| `N3` | 909,477 | 6,091 | negative control |
| `OTHER` | 23,592,600 | 55,125 | excluded |
| `N2` terminal | 41,418,120 | 104,104 | terminal control; separate denominator |

## Class A overview

**Decision A — NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE.**

The corpus contains natural post-admission pure-melodic boundaries satisfying the bootstrap crossing shape without any new duration policy, articulation extension, hybrid arbitration, or vocabulary.

The only reachable Class A subclass is **A-ONSET**.

## A-onset

Definition:

```text
pure SemanticSynthBRole::Melodic on both sides
incoming not empty
incoming has no onset at step0
incoming has a later melodic onset
outgoing admitted melodic occupancy reaches step15
step15 occupancy is a NEW onset
```

Measured frequency:

```text
A_ONSET / all admitted adjacent boundaries
17,530,610 / 96,729,660 = 18.123303649%

A_ONSET / pure-melodic adjacent boundaries
17,530,610 / 52,296,930 = 33.521298478%

A_ONSET / pure-melodic boundaries with non-empty incoming bar
17,530,610 / 41,306,411 = 42.440409553%
```

Frequency does not determine Decision A; it is input to the later R1 complexity/value decision.

## A-continuation

Final status:

```text
raw    = 0
unique = 0
UNREACHABLE UNDER FROZEN ATTEMPT-0 PRODUCTION SEMANTICS
```

Two independent forms of evidence agree:

1. source dependency proof: all frozen melodic continuation vocabularies/bars have no logical step15 continuation, and every current production tonal profile uses `MelodicRhythmOperationId::Preserve`;
2. exhaustive corpus confirmation: zero observed `A_CONTINUATION` across the entire attempt-0 identity domain.

Therefore C2 must not generalize from A-onset to continuation-driven crossing.

## A-overlap

Final status:

```text
raw    = 0
unique = 0
```

No boundary contains simultaneous admitted onset and continuation occupancy on logical step15. No semantic ownership ambiguity was observed.

## Class B

`B` is a pure-melodic boundary with outgoing melodic occupancy and a later same-role incoming onset, but the outgoing occupancy stops before logical step15. Making it cross would require a new pre-boundary extension policy, so bootstrap C2 excludes it.

Measured:

```text
raw    = 22,115,006
unique = 33,632
```

## Class H

`H` contains `ChordWithMelodicFill` on at least one side and therefore shares the monophonic Synth B between chord and melodic-fill ownership. Same physical Synth B does not prove same logical melodic voice.

Measured:

```text
raw    = 9,276,932
unique = 80,113
```

Hybrid arbitration remains a separate future problem and is excluded from bootstrap C2.

## Negative controls

- `N0`: incoming same-voice onset at logical step 0.
- `N1`: incoming `ValidButEmpty` / empty admitted melodic material.
- `N2`: phrase end / loop wrap; never bootstrap continuation.
- `N3`: outgoing semantic occupancy ends at or before step 14; crossing would require pre-boundary extension.

## Known M1L hardware fixtures

The historical controls now map deterministically:

```text
SPARSE {2}/empty/{2}/empty
0->1 = N1
1->2 = N3
2->3 = N1

CALL-style {6,14} repeated
0->1 = B
1->2 = B
2->3 = B
```

They remain negative controls and were not manipulated to manufacture Class A.

## A-onset distribution by generative mode

| Mode | Raw A-onset |
|---:|---:|
| 0 | 1,069,100 |
| 1 | 1,068,860 |
| 2 | 1,070,660 |
| 3 | 2,356,252 |
| 4 | 1,069,800 |
| 5 | 0 |
| 6 | 0 |
| 7 | 2,700,322 |
| 8 | 1,073,864 |
| 9 | 1,062,952 |
| 10 | 1,070,940 |
| 11 | 0 |
| 12 | 0 |
| 13 | 2,494,580 |
| 14 | 2,493,280 |
| 15 | 0 |

## A-onset distribution by requested recipe ID

| Recipe ID | Raw A-onset |
|---:|---:|
| 0 | 964,604 |
| 1 | 962,028 |
| 2 | 963,036 |
| 3 | 962,884 |
| 4 | 964,744 |
| 5 | 962,172 |
| 6 | 961,672 |
| 7 | 959,900 |
| 8 | 958,896 |
| 9 | 1,171,730 |
| 10 | 963,292 |
| 11 | 963,192 |
| 12 | 961,804 |
| 13 | 961,580 |
| 14 | 962,828 |
| 15 | 960,280 |
| 16 | 963,752 |
| 17 | 962,216 |

## A-onset distribution by resolved tonal profile

Only nonzero `(generativeMode, profileRecipe)` pairs are listed; all omitted pairs are zero.

| Mode | Profile recipe | Raw A-onset |
|---:|---:|---:|
| 0 | 0 | 951,048 |
| 0 | 6 | 59,688 |
| 0 | 7 | 58,364 |
| 1 | 0 | 1,068,860 |
| 2 | 0 | 1,070,660 |
| 3 | 0 | 2,356,252 |
| 4 | 0 | 1,009,768 |
| 4 | 4 | 60,032 |
| 7 | 0 | 1,798,336 |
| 7 | 1 | 138,392 |
| 7 | 2 | 139,352 |
| 7 | 3 | 138,916 |
| 7 | 8 | 137,036 |
| 7 | 9 | 348,290 |
| 8 | 0 | 1,073,864 |
| 9 | 0 | 1,062,952 |
| 10 | 0 | 1,070,940 |
| 13 | 0 | 2,494,580 |
| 14 | 0 | 2,493,280 |

The profile counts sum exactly to the global `A_ONSET` raw count.

## A-onset distribution by phrase length

| Exact admitted phrase length | Raw A-onset |
|---:|---:|
| 1 | 0 |
| 2 | 4,295,580 |
| 4 | 12,991,227 |
| 8 | 243,803 |

Length 1 has no adjacent intra-phrase boundary and therefore cannot contribute Class A.

## Production-default reachability

`GenreSettings{}` resolves to:

```text
mode             = 0 / Acid
recipe           = 0 / base
rhythm selection = AUTO
```

The production-default path contains:

```text
A_ONSET raw = 59,684
reachable   = YES
```

The minimal frozen default witness remains:

```text
mode=0
recipe=0
profile_recipe=0
bars=2
identity=2
boundary=0->1
archetype=405
progression=1
rhythm=PICKUP PHRASE
motif=PIVOT
out_on=0x0009
out_cont=0x0000
in_on=0x0009
in_cont=0x0000
out_last=15
in_first=12
```

The raw PickupPhrase vocabulary is `{12,14,15}`, but real production admission removes step14 for this witness, leaving `{12,15}`. The witness is therefore post-admission evidence, not name-based classification.

## Intra-segment vs 3->4 evolution seam

Global A-onset location counts:

```text
ordinary intra-segment boundaries = 17,495,781
8-bar 3->4 evolution seam          =     34,829
```

Only `0.198675346%` of all A-onset occurrences are on the 3->4 seam.

Within admitted 8-bar A-onset occurrences, every adjacent boundary has the same count:

| 8-bar boundary | Raw A-onset |
|---|---:|
| 0->1 | 34,829 |
| 1->2 | 34,829 |
| 2->3 | 34,829 |
| 3->4 | 34,829 |
| 4->5 | 34,829 |
| 5->6 | 34,829 |
| 6->7 | 34,829 |

Thus A-onset is **not concentrated at the 3->4 evolution seam**. The seam contributes exactly one seventh of 8-bar A-onset boundaries.

## Representative witnesses

Length 2 / production-default:

```text
class=A_ONSET mode=0 recipe=0 profile_recipe=0 bars=2 identity=2
boundary=0->1 archetype=405 progression=1 rhythm=PICKUP PHRASE motif=PIVOT
out_on=0x0009 out_cont=0x0000 in_on=0x0009 in_cont=0x0000
out_last=15 in_first=12
```

Length 4:

```text
class=A_ONSET mode=0 recipe=0 profile_recipe=0 bars=4 identity=2
boundary=0->1 archetype=405 progression=1 rhythm=PICKUP PHRASE motif=PIVOT
out_on=0x0009 out_cont=0x0000 in_on=0x0009 in_cont=0x0000
out_last=15 in_first=12
```

Length 8:

```text
class=A_ONSET mode=7 recipe=9 profile_recipe=9 bars=8 identity=0
boundary=0->1 archetype=418 progression=5 rhythm=PICKUP PHRASE motif=MIRROR
out_on=0x000b out_cont=0x0000 in_on=0x000b in_cont=0x0000
out_last=15 in_first=12
```

8-bar evolution seam:

```text
class=A_ONSET mode=7 recipe=9 profile_recipe=9 bars=8 identity=0
boundary=3->4 archetype=418 progression=5 rhythm=PICKUP PHRASE motif=MIRROR
out_on=0x000b out_cont=0x0000 in_on=0x000b in_cont=0x0000
out_last=15 in_first=12
```

A non-default case also exists where the incoming first onset is itself step15:

```text
class=A_ONSET mode=0 recipe=1 profile_recipe=0 bars=2 identity=2
boundary=0->1 archetype=408 progression=1 rhythm=PICKUP PHRASE motif=SOURCE ORDER
out_on=0x0001 out_cont=0x0000 in_on=0x0001 in_cont=0x0000
out_last=15 in_first=15
```

## Semantic occupancy vs physical gate

Semantic step occupancy is the C2-C0 classifier. Physical gate lifetime is characterized separately and is not allowed to change the semantic class.

For the production-default Acid A-onset witness:

```text
preset gateLengthMultiplier = 0.8
runtime Synth-B multiplier  = 1.05
effective gate              = 0.84 step
```

The existing countdown release therefore occurs before the actual bar boundary.

This is expected C2-C0 evidence:

```text
C2 owns the logical lifetime decision.
R1 must later make an authorized Continue physically real.
```

C2-C0 does not alter gate multipliers, NoteOff scheduling, AllNotesOff, transport, MIDI, or internal synth lifetime.

## Optional H1-F1/H2 witness

No combined optional H1-F1/H2/TwoFiveOne representative was reachable in this characterized corpus.

Status:

```text
UNREACHABLE IN CHARACTERIZED CORPUS
```

This is informational only and does not affect Decision A.

## Validation evidence

Focused workflow on characterization candidate `b899ee32beac332a73a8b81ca79eb1e5f38c2bd0`:

```text
GitHub Actions run 33102335333
boundary-topology-characterization
SUCCESS
```

The successful runner included:

```text
opening src/ firewall
known A witness GCC + deterministic repeat + Clang
full exhaustive corpus GCC -O2, repeated twice
full corpus Clang equivalence
signature collision replay validation
ASan smoke
UBSan smoke
physical-gate frozen-source contract
full P1R compatibility runner
final src/ firewall
```

The final documentation-only freeze commit must receive the same exact-head terminal-green workflow before the checkpoint is frozen. The authoritative frozen SHA is recorded in PR #395 after that terminal validation rather than self-embedded in this file, because embedding a commit's own SHA would change that SHA.

## Limitations

- C2-C0 is host characterization only; no subjective hardware listening is performed.
- Primary statistics are frozen at `generationAttemptOrdinal=0`.
- Reroll/retry topology for nonzero attempt ordinals is a separate future axis.
- Signature equality is only a statement about this frozen topology classifier, not full musical/material equivalence.
- Hybrid Synth-B arbitration remains outside bootstrap C2.
- Physical voice lifetime remains unimplemented.

## Provenance

Branch:

`research/20260827-05-0.9.9-phrase-c2-c0-boundary-topology`

Frozen base:

`016bcd6ba514b3a57f8803c63c869f1b2a8953a7`

Draft PR:

`#395`

## Hardware

Host characterization only. No Cardputer ADV, SEQTRAK, external display, MIDI cable, or audio output is required for this checkpoint.

## Wiring

None.

## Build / run

```sh
bash tests/run_0_9_9_phrase_c2_c0_tests.sh
```

## Expected behavior

The runner must print the deterministic exhaustive attempt-0 corpus report, zero `A_CONTINUATION`, zero `A_OVERLAP`, a reachable `A_ONSET`, signature collision `PASS`, P1R compatibility `OK`, and final Decision A while leaving `src/` unchanged.

## Troubleshooting

- A raw rhythm with step15 is not evidence; inspect admitted masks after Bass/Chord/protected-space handling and the final semantic Synth-B role.
- `0x0009` means logical steps 12 and 15, not 0 and 3.
- A terminal phrase boundary is `N2`, not an adjacent Class A candidate.
- An incoming step0 onset is `N0` by precedence even if the outgoing bar reaches step15.
- `ChordWithMelodicFill` is `H`/hybrid territory, not pure melodic continuity.
- A zero A-continuation result is valid evidence and must not be converted into a positive fixture.

## Acceptance checklist

- exact frozen P1R base `016bcd6...`;
- separate research branch/worktree lineage;
- zero `src/` delta;
- complete attempt-0 `uint16_t` identity domain excluding only `0xFFFF` sentinel;
- exact 1/2/4/8 resolver used for every request tuple;
- post-admission production observation;
- pure vs hybrid role separation;
- step15 onset/continuation/overlap separated;
- explicit one-class precedence;
- incoming step0, later onset, `ValidButEmpty` and phrase end explicit;
- raw occurrences and unique signatures reported;
- mode/recipe/profile/length/default-path distributions reported;
- intra-segment and 3->4 seam distributions reported;
- source dependency proof plus exhaustive zero for A-continuation;
- zero A-overlap;
- signature collision replay validation passed;
- M1L historical controls mapped without mutation;
- representative full P1R replay;
- physical gate observed, never changed;
- P1R compatibility passed;
- GCC/Clang/determinism/ASan/UBSan gates passed;
- final documentation-only exact head terminal GREEN before freeze;
- no C2 producer/runtime/hardware changes.

## Decision

**DECISION A — NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE.**

Frozen attempt-0 subclass status:

```text
A_ONSET        REACHABLE
A_CONTINUATION UNREACHABLE
A_OVERLAP      ZERO
```

Therefore the next production checkpoint, after this research head receives terminal exact-head GREEN and is frozen, is authorized to investigate only:

```text
PHRASE-C2
MINIMAL CROSS-BAR LIFETIME PRODUCER
scope: A-ONSET ONLY
```

Do not generalize bootstrap C2 to continuation-driven, hybrid, step14-extension, empty-bar, phrase-end, loop-wrap, transport, gate, MIDI, or runtime execution policy.

**HARD STOP after C2-C0 freeze.**
