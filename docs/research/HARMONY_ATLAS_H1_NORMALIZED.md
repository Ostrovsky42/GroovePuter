# Harmony Atlas H1 Canonical Normalization

**Status:** generated research evidence / H1 checkpoint  
**Source:** `ldrolez/free-midi-chords @ baf0896694de6b09ac00250722f2414202e668ed`  
**Evidence class:** `EDITORIAL_CATALOG_EVIDENCE`  
**Runtime impact:** none

## Gate result

| Item | Count |
|---|---:|
| Logical definitions | 190 |
| Admitted definitions | 190 |
| Quarantined definitions | 0 |
| Normalized chord events | 846 |
| Normalized rest events | 0 |
| Raw token vocabulary | 85 |
| Observed raw suffixes | 20 |

H1 normalizes source notation only. It performs no progression deduplication, functional/cadential classification, runtime weighting, absolute MIDI projection, or firmware vocabulary admission.

## Canonical root representation

Each chord token is separated into:

```text
FunctionalDegree
  diatonic_degree       0..6
  alteration_semitones  -1 | 0 | +1 in the pinned corpus
  notation_case         UPPER | LOWER
  source_accidental     exact source spelling
  source_roman          exact source Roman spelling
```

`bIII`, `III` and `#III` therefore cannot collapse to the same root identity.

## Loss-aware chord quality

Quality is decomposed into triad class, extension class, seventh flavor, fifth alteration and exact raw suffix.

Critical non-equivalence:

```text
I7    -> MAJOR + SEVENTH + UNSPECIFIED seventh flavor
Idom7 -> MAJOR + SEVENTH + DOMINANT
IM7   -> MAJOR + SEVENTH + MAJOR
```

H1 refuses to guess that generic `7` is equivalent to explicit `dom7` or `M7`.

Supported pinned progression suffixes (20): `<empty>`, `5`, `6`, `69`, `7`, `9`, `M`, `M-5`, `M6`, `M7`, `add9`, `dim`, `dom7`, `m`, `m6`, `m7`, `m9`, `madd9`, `sus2`, `sus4`

## Source family / notation boundary

```text
Major -> TRADITIONAL_MAJOR
Minor -> TRADITIONAL_MINOR
Modal -> IONIAN_RELATIVE_MODAL
```

Family remains part of every normalized progression; H1 claims no cross-family semantic equivalence.

## Typed metadata

Known descriptors are typed as mood, structural (`Cadence`) or catalog (`New`). Unknown descriptors quarantine the definition.

## Quarantine

Quarantined definitions: **0**. Quarantine reasons: **0**.

No pinned-source definitions are quarantined.

## Reproducibility / handoff

- exact H0 source pin and critical blob verification are reused;
- every admitted token round-trips to exact source spelling;
- event order and repeated events are preserved;
- explicit rests use `REST` event refs;
- normalized definitions reference a deterministic token vocabulary;
- H1 performs no progression deduplication;
- H2 may consume this representation for explicit F0-F6 fingerprints;

Next stage: **H2 structural fingerprints / dedup**.
