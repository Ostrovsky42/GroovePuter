# 0.9.9-F08 Hardware Listening Acceptance

Date: 2026-08-23

## Purpose

Record the bounded human hardware-listening decision that unblocks the F08 Stage15 tonal golden update.

This record accepts the already-reviewed F08 ownership change and its current `{0,8}` moving bootstrap as a safe baseline. It does **not** accept `{0,8}` as the final HarmonicRhythm musical vocabulary.

## Ownership decision

Accepted and preserved:

- `ChordRhythm` owns physical chord articulation: attacks, continuations, releases.
- `HarmonicRhythm` owns harmonic change timing and harmonic event count.
- `ChordProgression` owns the degree / quality sequence and consumes HarmonicRhythm event cardinality.
- `TonalMaterializer` consumes physical role articulation and the independently owned harmonic clock.

Must not be restored:

```cpp
progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);
```

`HarmonicRhythmRequest` must not regain `ChordRhythmId`, `ChordRhythmPlan`, or chord-onset ownership.

## Frozen hardware A/B corpus

| # | Case | Clock | Preference | Note |
| ---: | --- | --- | --- | --- |
| 1 | DnB 5 B / MINOR FALL | `0000 -> 8080` | NEW | Progression remained recognisable; role coherence remained good; an artificial/predictable step-8 turn was audible. |
| 2 | TripHop 4 A / II-V-I | `0000 -> 8080` | NEW / SAME | — |
| 3 | House 4 A / POP CYCLE | `0000 -> 8080` | NEW | OLD was also musically useful as variation. |
| 4 | House 5 B / POP CYCLE | `4904 -> 8080` | SAME | OLD possibly slightly better. |
| 5 | Outrun 0 B / POP CYCLE | `2448 -> 8080` | NEW | — |
| 6 | UK Garage 1 B / BORROWED LIFT | `0101 -> 8080` | OLD / SAME | Both musically acceptable. |
| 7 | FunkSoul 6 B / BORROWED LIFT | `0802 -> 8080` | NEW | — |
| 8 | TripHop 2 B / PARALLEL SHIFT | `0902 -> 8080` | SAME | — |
| 9 | Acid 2 B / STATIC MODAL | unchanged | SAME | Static control. |
| 10 | Techno 4 B / PEDAL DRONE | unchanged | SAME | Static control. |
| 11 | Reggae 4 B / BORROWED LIFT | `0202 -> 8080`, output fingerprint same | SAME | Moving sensitivity control. |

Changed cases 1-8:

- clear NEW preference: **1, 3, 5, 7**
- NEW/SAME: **2**
- SAME: **8**
- OLD/SAME or OLD possibly better: **4, 6**
- strong NEW rejection: **0**

Controls:

- 9: SAME
- 10: SAME
- 11: SAME

This is a bounded human musical acceptance corpus, not a statistical experiment.

## Accepted interpretation

1. Independent HarmonicRhythm ownership caused no systematic musical regression in this corpus.
2. Current `{0,8}` is adequate as a safe F08 baseline.
3. `{0,8}` is too predictable to be the final HarmonicRhythm vocabulary.
4. Some legacy timing shapes remain musically useful as possible future vocabulary candidates.
5. Those shapes must never regain ownership through chord articulation.
6. Case 1 exposed audible step-8 predictability despite NEW being preferred overall.
7. Case 3 showed that OLD can remain musically useful as variation even where NEW is preferred.

Therefore the accepted F08 statement is:

> replace articulation-derived harmonic timing with independently owned harmonic timing

It is **not**:

> `{0,8}` is the final HarmonicRhythm vocabulary

## Structural and causal evidence re-verified before golden update

The current F08 head was re-run against the frozen Stage15 corpus before finalization.

Re-verified snapshot:

- corpus rows: **256**
- changed rows: **93**
- topology changes: **0**
- articulation changes: **0**
- pitch changes: **93**
- full-fingerprint changes: **93**
- identical old/new clock -> changed: **0**
- same event count / different positions -> changed: **31**
- harmonic activity increased: **46**
- harmonic activity decreased: **16**
- same count / different positions: **31**
- static harmony changed: **0 / 102**
- Reggae moving rows: **2**
- Reggae moving fingerprints changed: **0**
- high-risk `0 -> 2` harmonic-event class: **19** rows

Fresh generated F08 tonal actual SHA-256 before the golden update:

```text
bbc1544bf289c7ef7f062997bde3f0b8dae3a317ace54b0998cef6649872ac3f
```

The 11 hardware-listening coordinates were re-verified against the same progression and old/new harmonic-clock cases before accepting the golden.

## Bootstrap quarantine remains active

Current production bootstrap remains intentionally:

```text
static progression -> {0}
moving progression -> {0,8}
```

The representative moving progression corpus remains:

- POP CYCLE
- II-V-I
- PARALLEL SHIFT
- MINOR FALL
- BORROWED LIFT

Current debt state:

```text
distinctMovingHarmonicClocks = 1
future debt target >= 4
EXPECTED XFAIL / CI remains green
```

The `>= 4` target is only a **debt detector**. It is not a musical-quality acceptance criterion. Four bad clocks are not better than one good clock.

Do not invent additional clocks merely to XPASS the quarantine.

## F08.1 boundary — record only, not implemented here

Future conceptual contract:

```text
Progression vocabulary
Phrase position
Phrase harmonic position
Generation level
possibly articulation characteristics as NON-OWNING context
        |
        v
HarmonicRhythmPolicy
        |
        +-- event count
        |
        +-- harmonic clock selected from a small bounded vocabulary
        |
        v
ChordProgression + TonalMaterializer
```

Research candidates may include shapes such as:

```text
{0}
{0,8}
{0,4,8,12}
{0,12}
{0,6,10}
{0,8,12}
```

They are candidates only. This F08 finalization does not add them to production.

The next research question is which small set of harmonic timing shapes is justified by progression structure and phrase position.

Articulation may later be non-owning selection context, for example to avoid an awkward harmonic change on top of a dense physical chord gesture. It must not become owner of event count or harmonic clock again.

Do not start F08.1 from either:

```text
genre -> mask
```

or:

```text
if BPM > X -> mask
```

## Final F08 acceptance state

```text
F08 OWNERSHIP
PASS

F08 STRUCTURAL SAFETY
PASS

F08 CAUSAL CORPUS
PASS

F08 HARDWARE MUSICAL ACCEPTANCE
PASS

F08 {0,8} BOOTSTRAP
ACCEPTED BASELINE / QUARANTINED AS INCOMPLETE VOCABULARY

F08 {0,8} AS FINAL VOCABULARY
REJECTED / INSUFFICIENT

STAGE15 GOLDEN
APPROVED FOR UPDATE TO THIS HARDWARE-ACCEPTED F08 BASELINE

F08.1 HARMONIC RHYTHM VOCABULARY
NEXT / OUT OF SCOPE
```
