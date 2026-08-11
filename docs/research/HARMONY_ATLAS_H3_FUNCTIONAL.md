# Harmony Atlas H3 Functional Analysis

**Status:** generated research evidence / H3 checkpoint  
**Source:** `ldrolez/free-midi-chords @ baf0896694de6b09ac00250722f2414202e668ed`  
**Evidence class:** `EDITORIAL_CATALOG_EVIDENCE`  
**Runtime impact:** none

## Dependency / identity boundary

H1 JSON SHA-256: `4783be1f77784a978f2239d8527214b3912eaf262e92430bff755679fec0734e`.

H2 JSON SHA-256: `453287577707e676128f3f83d1215922e22049ab0d880492f95af20ea7e68b0b`.

H3 recomputes and verifies every F0-F3 fingerprint before deriving functional evidence. H3 cannot rewrite normalized source identity.

## F4 FunctionalClassSequence

| Item | Count |
|---|---:|
| Definitions | 190 |
| Unique F4 classes | 101 |
| Duplicate F4 groups | 27 |
| Surplus F4 duplicates | 89 |
| Largest F4 class | 29 |
| Cross-family F4 duplicate groups | 14 |

F4 fingerprints only the ordered **functional role + color class** sequence. Reason codes and confidence remain diagnostics and do not silently redefine identity.

F4 groups spanning multiple distinct F3 identities: **27**; largest such F4 class contains **29** distinct F3 identities.

**F4 is an analytic abstraction, not a destructive-dedup authority.**

## Functional classification boundary

```text
Major / Minor
  I      -> TONIC
  II, IV -> PREDOMINANT
  V      -> DOMINANT
  other unaltered roots -> UNALTERED_OTHER
  altered roots         -> CHROMATIC_COLOR

Modal
  tonic -> TONIC
  every other event -> MODAL_COLOR
```

Modal harmony is deliberately not forced into common-practice tonic/predominant/dominant roles.

Borrowing is reported only as `BORROWED_CANDIDATE`; H3 does not claim historical/compositional borrowing as fact.

## Closure-class distribution

| Class | Definitions |
|---|---:|
| `CADENTIAL` | 17 |
| `CLOSED_TONIC_LOOP` | 4 |
| `DOMINANT_LOOP` | 27 |
| `MODAL_AMBIGUOUS_LOOP` | 82 |
| `OPEN_LOOP` | 48 |
| `TURNAROUND` | 12 |

## Cadence-class distribution

| Class | Definitions |
|---|---:|
| `AUTHENTIC_CADENCE_CANDIDATE` | 10 |
| `DECEPTIVE_CADENCE_CANDIDATE` | 3 |
| `HALF_CADENCE_CANDIDATE` | 27 |
| `MODAL_AMBIGUOUS` | 82 |
| `NO_CADENCE` | 64 |
| `PLAGAL_CADENCE_CANDIDATE` | 4 |

Source `Cadence` metadata and derived cadence candidates are kept separate:

| Comparison | Definitions |
|---|---:|
| `DERIVED_ONLY` | 44 |
| `NEITHER` | 140 |
| `TAGGED_MODAL_UNRESOLVED` | 6 |

## Chromatic / borrowed candidate report

Definitions with altered roots: **71**; altered-root events: **123**.

Altered degree classes: `#IV`=2, `bI`=2, `bII`=31, `bIII`=26, `bVI`=27, `bVII`=35.

Altered-root events by source family: `Modal`=123.

Definitions with conservative borrowed candidates: **0**; candidate events: **0**.

Borrowed counts are candidate evidence only, not a measured statement about compositional origin.

## Quality-diversity report

Mean semantic quality entropy: **0.89766 bits**.

Range: **0.0 .. 2.155639 bits**.

Unique-quality-count distribution:

| Unique semantic qualities | Definitions |
|---|---:|
| 1 | 26 |
| 2 | 129 |
| 3 | 25 |
| 4 | 8 |
| 5 | 2 |

## Functional-motion report

Adjacent DOMINANT→TONIC resolutions: **17**.

Functional role event counts:

| Role | Events |
|---|---:|
| `DOMINANT` | 80 |
| `MODAL_COLOR` | 256 |
| `PREDOMINANT` | 108 |
| `TONIC` | 241 |
| `UNALTERED_OTHER` | 161 |

Top functional motions:

- `MODAL_COLOR->MODAL_COLOR` — 156
- `TONIC->MODAL_COLOR` — 74
- `UNALTERED_OTHER->UNALTERED_OTHER` — 52
- `MODAL_COLOR->TONIC` — 41
- `TONIC->UNALTERED_OTHER` — 39
- `UNALTERED_OTHER->PREDOMINANT` — 34
- `PREDOMINANT->DOMINANT` — 32
- `TONIC->PREDOMINANT` — 32
- `PREDOMINANT->UNALTERED_OTHER` — 30
- `UNALTERED_OTHER->TONIC` — 27
- `TONIC->DOMINANT` — 24
- `TONIC->TONIC` — 24
- `DOMINANT->UNALTERED_OTHER` — 23
- `DOMINANT->TONIC` — 17
- `PREDOMINANT->TONIC` — 15
- `UNALTERED_OTHER->DOMINANT` — 15
- `DOMINANT->PREDOMINANT` — 11
- `PREDOMINANT->PREDOMINANT` — 8
- `DOMINANT->DOMINANT` — 2

## Uncertain / unknown report

Definitions carrying LOW-confidence common-practice ambiguity or UNKNOWN cadence: **82**.

This is intentional. Modal/common-practice ambiguity stays visible rather than being forced into a stronger function class.

## H3 contract

- F0-F3 are verified unchanged before any functional derivation;
- F4 is now COMPUTED from functional role + color class;
- F4 is high-level analytic equivalence and has no destructive-dedup authority;
- F5 remains deferred to H4 ChordRhythm extraction;
- F6 remains deferred until F5 exists;
- cadence and closure classes are deterministic heuristics with reason/confidence;
- source `Cadence` tags do not overwrite derived cadence classes;
- borrowed labels are candidates only;
- no absolute MIDI projection is performed;
- no runtime candidate is selected;
- no source-incidence ratio becomes runtime weight;
- timing-based harmonic density remains deferred to H4.

Next stage: **H4 ChordRhythm extraction**. H4 may make F5 computable; H3 must not be retroactively rewritten to fit rhythm results.
