# GF2 G4-CC1A-P1 — Acid Articulation Authority Ratification

## Status

`AUTHORITY_RATIFIED / CAPABILITY_REVIEW_REQUIRED`

This document serves as the canonical authority report for checkpoint `G4-CC1A-P1` on branch `feature/20260911-01-g4-cc1a-p1-acid-articulation-authority`.

It establishes that authoritative, two-sided bass articulation capability (independent re-articulation vs. connected/extended motion) has been restored and proven reachable across all three shipped Acid owners (`Acid / BASE`, `Acid / Chicago Jack`, `Acid / Rolling Acid`) across realization levels P1, P2, and P3, without altering shared profile definitions, existing candidate weights, or unowned production subsystems.

The candidate contract `G4-ACID-BASS-ARTICULATION-CAPABILITY` remains `REVIEW_REQUIRED` as designed; this ratification ratifies the authority and reachability evidence without prematurely promoting capability to `PROVEN` in the global registry.

## Authoritative Lineage & Verification Boundary

```text
base_sha=8331735080f25acbc809ef97b9881fd9e17c86a2
branch=feature/20260911-01-g4-cc1a-p1-acid-articulation-authority
red_sha=c92a1d459eb7e72251aa30df9313ea79b3543d46
production_fix_sha=fe2697cf55323c735f794c0251e17d817ca3c0cd
c1b_lifetime_sha=77815605d54023770425a80a221f7c8ec17dd948
i6_r1_refresh_sha=b41c24242f68cf0bbb62ebf776729a88b4167a80
workflow=.github/workflows/gf2-g4-cc1a-p1-acid-articulation-authority.yml
runner=tests/run_gf2_g4_cc1a_p1_ratification.sh
permissions=Contents: read, Metadata: read
```

## Production Scope & Blast Radius Audit

Diff against base `8331735080f25acbc809ef97b9881fd9e17c86a2` across `src/**`:

```text
src/generation/composition/generation_profile.cpp
```

- Total production files changed: **1**
- Non-production changes: tests, CI workflows, and documentation evidence.
- Shared candidate safety: `kBassDrive` remains untouched for House, Techno, Darksynth, Rave, and Broken genres.
- Acid candidate safety: all existing candidate weights (`KickLock`: 70, `OffbeatPush`: 90, `RollingDrive`: 110, `SyncopatedHook`: 75) are strictly preserved in the new Acid-specific `kBassAcid` candidate table.
- Added candidate: existing `BassRhythmId::SustainAndDrop` (weight 100).

## Defect Characterization (RED)

Prior to P1, `G4-CC1A` proved that while the upstream `LaneGrammar` metadata declared detached and held gates, downstream strong materialization collapsed into detached-only patterns:

- Admitted Acid rhythm candidates: detached-only `kBassDrive` identities.
- Historical 1152-row corpus: `all_detached=1152`, `mixed=0`, `continuation=0`.
- Connected witness reachability: 0/3 Acid owners could select or materialize connected motion.

RED commit `c92a1d45` established this failure with explicit two-sided reachability requirements on the unmodified production tree.

## Causal Correction (GREEN)

In `src/generation/composition/generation_profile.cpp`:

1. Defined `kBassAcid[]` preserving all four `kBassDrive` entries and introducing `BassRhythmId::SustainAndDrop` (weight 100).
2. Bound `kBassAcid` specifically to Acid profiles:
   - `Acid / BASE` (recipe 0)
   - `Acid / Chicago Jack` (recipe 6)
   - `Acid / Rolling Acid` (recipe 7)
3. Preserved downstream realization pipeline:
   `resolveStrongRhythmFrozenSelection()` -> `BassRhythmPlan` -> `BassPitchBehaviorPlan` -> `adaptTonalPlanToSynthPattern()`.

## Two-Sided Reachability & Witnesses

Each shipped Acid owner now possesses deterministic detached and connected witnesses across all realization levels (P1, P2, P3):

| Owner | Recipe | Detached Witness | Connected Witness | Selected Connected Archetype & Masks |
| :--- | :---: | :---: | :---: | :--- |
| `Acid / BASE` | 0 | id=1 | id=2 | Archetype 405, `SustainAndDrop` (id=10), Onsets `0x8008`, Continuations `0x7F07` |
| `Acid / Chicago Jack` | 6 | id=1 | id=15 | Archetype 405, `SustainAndDrop` (id=10), Onsets `0x8008`, Continuations `0x7F07` |
| `Acid / Rolling Acid` | 7 | id=1 | id=15 | Archetype 405, `SustainAndDrop` (id=10), Onsets `0x8008`, Continuations `0x7F07` |

Every connected witness verifies:
- `onset_mask` MSB-first step addressing (e.g. `0x8008` = steps 0, 12).
- `continuation_mask` addressing (e.g. `0x7F07` = steps 1..7, 13..15).
- Tonal materializer copies active predecessor note into continuation cells with `slide=true`.
- No independent onset is triggered on continuation cells.

## Corpus Measurement (1152 Rows)

Corpus parameters: 3 owners × 3 levels × 128 identities = 1152 materializations.

```text
attempted: 1152
successful: 1152
failed: 0
all_detached: 918 (79.7%)
all_connected: 0
mixed: 234 (20.3%)
continuation_rows: 234
continuation_events: 2340
slide_rows: 0
```

### Breakdown by Owner and Level

| Owner | Level | Rows | All Detached | All Connected | Mixed | Continuation Rows | Continuation Events | Slide Rows |
| :--- | :---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `BASE` | P1 | 128 | 97 | 0 | 31 | 31 | 310 | 0 |
| `BASE` | P2 | 128 | 97 | 0 | 31 | 31 | 310 | 0 |
| `BASE` | P3 | 128 | 97 | 0 | 31 | 31 | 310 | 0 |
| `CHICAGO_JACK` | P1 | 128 | 105 | 0 | 23 | 23 | 230 | 0 |
| `CHICAGO_JACK` | P2 | 128 | 105 | 0 | 23 | 23 | 230 | 0 |
| `CHICAGO_JACK` | P3 | 128 | 105 | 0 | 23 | 23 | 230 | 0 |
| `ROLLING_ACID` | P1 | 128 | 104 | 0 | 24 | 24 | 240 | 0 |
| `ROLLING_ACID` | P2 | 128 | 104 | 0 | 24 | 24 | 240 | 0 |
| `ROLLING_ACID` | P3 | 128 | 104 | 0 | 24 | 24 | 240 | 0 |

Deterministic repeat check passed byte-for-byte on both TSV census output and summary log across repeated executions.

## Monophony & Connected Integrity

- Monophony assertion: **PASS** across all 234 continuation rows.
- Every continuation step carries active predecessor continuity without re-triggering NoteOn.
- Strict monophonic voice ownership: step transitions do not cause dual-held notes or voice allocation leaks.

## Lifetime & Source Boundary Regressions (C1B)

The historical boundary test `tests/test_pattern_phrase_p2_lifecycle_barriers.cpp` was extended in C1B to execute the connected witness (`Acid / BASE / P1 / identity=2`) directly through `MiniAcid::processSequencerEvents()`:

- **Targeted NoteOff**: Proved exactly one NoteOff event upon sequenced source transition (`PATTERN -> PHRASE`).
- **No Stuck Notes**: Note release verified at physical audio engine seam.
- **Stale Continuation Invalidation**: Subsequent step advancing proves old pattern continuation cannot reassert ownership or generate phantom sound.
- **Panic Immunity**: Transition executes cleanly without triggering emergency panic reset.

## Adversarial & False Positive Controls

All negative and adversarial controls pass:

1. **False Positive Defense**:
   - `fresh_onset`: PASS (independent attack on continuation step rejected)
   - `slide_onset_without_predecessor`: PASS (slide-only decoration without causal note rejected)
   - `stale_previous`: PASS (continuation after release rejected)
   - `unrelated_held`: PASS (unrelated voice hold rejected)
   - `ghost_note`: PASS (ghost-marked onset rejected as continuation)
   - `ownership_leak`: PASS (cross-pattern voice leak rejected)
   - `positive_control`: PASS (canonical connected pair accepted)
2. **Adversarial Mutators**:
   - `flatten_articulation`: PASS (artificial collapse detected and failed)
   - `remove_connected_path`: PASS (omission of connected identity fails contract)
   - `remove_detached_path`: PASS (omission of detached identity fails contract)
   - `label_independence`: PASS (mutating genre label does not bypass predicate)
   - `weight_independence`: PASS (candidate weight alterations do not alter capability truth)
   - `kick_independence`: PASS (removing kick response does not break articulation model)
   - `grid_independence`: PASS (independent of arbitrary grid constant windows)

## Unified Ratification Matrix (12/12 PASS)

The complete suite executed under unified runner `tests/run_gf2_g4_cc1a_p1_ratification.sh`:

| Gate | Result | Focus |
| :--- | :---: | :--- |
| `production_scope` | **PASS** | Strict blast-radius audit: only `generation_profile.cpp` modified |
| `p1_authority` | **PASS** | Acid articulation authority test: witnesses, determinism, corpus |
| `c1b_lifetime` | **PASS** | Realtime connected witness playback & source transfer barrier |
| `p2` | **PASS** | ASan/UBSan, GCC/Clang parity, lifecycle barriers |
| `p3` | **PASS** | Phrase-relative onset addressing, SDL MiniAcid integration |
| `g4_r1` | **PASS** | Reference genre contracts (DnB, Dub Techno, Acid, House) |
| `g4_i3` | **PASS** | Phrase-law causal truthfulness across all genre pilots |
| `g4_i4` | **PASS** | Dub Techno structural ownership & admission invariants |
| `g4_i5` | **PASS** | House structural ownership & admission invariants |
| `g4_i6` | **PASS** | Global ownership census (12,672/12,672, reachability 122/122, freshness UP_TO_DATE) |
| `g4_cc1` | **PASS** | Acid, Techno, Funk contract wave |
| `g4_cc1a` | **PASS** | Retained mode: `PARTIAL_SUCCESSOR_P1` (historical collapsed assertions bypassed) |
| **overall** | **PASS** | Full suite green |

## Retained Contract Specification

```text
contract_id:
G4-ACID-BASS-ARTICULATION-CAPABILITY

version:
1

musical_statement:
Within each shipped Acid owner, the authoritative bass idea space preserves
a genuine musical choice between independent re-articulation and connected/extended
motion. Individual materializations are not required to mix both classes.

domains:
BASS
ARTICULATION

scope:
owner-space capability for Acid / BASE, Acid / Chicago Jack, Acid / Rolling Acid

quantifier:
forall shipped Acid owners,
  exists admissible authoritative detached/re-articulated possibility
  AND
  exists admissible authoritative connected/extended possibility

predicate_kind:
CAPABILITY

origin:
G4-CC1A

restoration:
G4-CC1A-P1

status:
REVIEW_REQUIRED
```

## Conclusion

`G4-CC1A-P1` is formally ratified. Authoritative two-sided bass articulation capability is fully proven, deterministic, lifetime-safe, and monophonically sound across all shipped Acid owners.
