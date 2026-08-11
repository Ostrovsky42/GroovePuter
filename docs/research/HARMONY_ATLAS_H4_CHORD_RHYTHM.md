# Harmony Atlas H4 ChordRhythm Extraction

**Status:** generated research evidence / H4 checkpoint  
**Source:** `ldrolez/free-midi-chords @ baf0896694de6b09ac00250722f2414202e668ed`  
**Evidence class:** `EDITORIAL_CATALOG_EVIDENCE`  
**Runtime impact:** none

## Identity boundary

```text
F0 SourceIdentity              FROZEN_FROM_H2
F1 TranspositionInvariant      FROZEN_FROM_H2
F2 RootSequence                FROZEN_FROM_H2
F3 RootQualitySequence         FROZEN_FROM_H2
F4 FunctionalClassSequence     FROZEN_FROM_H3
F5 ChordRhythmIdentity         COMPUTED_H4_CHORD_RHYTHM
F6 CombinedIdentity            COMPUTED_H4_F3_PLUS_F5
```

H4 recomputes and verifies F0-F4 before creating F5/F6.

## Source rhythm semantics

Pinned `gen.py`, `c2mpatterns.py` and `c2m.py` are parsed as source evidence. H4 executes no upstream renderer and decodes no generated MIDI.

```text
N  next progression event / new chord onset
S  same chord / new note onset (retrigger)
X  rest
numeric prefix  duration multiplier in beats
```

`S` is not a tie/hold continuation: pinned `c2m.py` emits `addNote(...)` for every non-rest segment. H4 therefore records retriggers separately and continuation=false.

`REST_SOURCE` and `REST_PATTERN` remain separate diagnostics, but both canonicalize to `REST` inside F5 because rest origin is provenance, not rhythm identity.

## Support accounting

Logical harmonic definitions: **190**.
Rhythm-style observations: **950**.
Generator styles per definition: **5**.
Key projections: **12**.
Physical progression MIDI materializations implied by source generation: **11400**.

Only the 190 logical definitions count as harmonic support. Style and key materialization multiplicity cannot inflate support or runtime weights.

## Realized rhythm vocabulary

Source generator pattern masks used: **5**.
Source event-count classes: **6**.
Realized F5 identities: **30**.

The realized F5 count is **not** a claim that the source contains that many independently authored rhythm patterns: it is the result of source masks conditioned by progression form length.

Source-event-count distribution:

| Events | Definitions |
|---:|---:|
| 3 | 14 |
| 4 | 144 |
| 5 | 5 |
| 6 | 7 |
| 8 | 19 |
| 9 | 1 |

## F5 / F6

| Level | Observations | Unique classes | Duplicate groups | Surplus | Cross-style groups | Largest class |
|---|---:|---:|---:|---:|---:|---:|
| F5 | 950 | 30 | 25 | 920 | 0 | 144 |
| F6 | 950 | 945 | 5 | 5 | 0 | 2 |

F5 fingerprints realized onset/retrigger/rest structure plus exact rational durations, independent of pitch, quality, family and style name. F6 combines exact F3 harmonic identity with F5.

## Segment statistics

- total segments: **6165**
- note-onset segments: **5299**
- same-chord retrigger segments: **1069**
- rest segments: **866**
- pattern-rest segments: **866**
- source-rest segments: **0**
- continuation segments: **0**

## Source style inventory

| Style | Observations | Unique F5 | Patterns | Onsets | Retriggers | Rests | Continuations |
|---|---:|---:|---|---:|---:|---:|---:|
| `default` | 190 | 6 | long | 846 | 0 | 0 | 0 |
| `hiphop2` | 190 | 6 | hiphop2 | 1692 | 846 | 866 | 0 |
| `pop` | 190 | 6 | pop | 846 | 0 | 0 | 0 |
| `pop2` | 190 | 6 | pop2 | 846 | 0 | 0 | 0 |
| `soul` | 190 | 6 | soul | 1069 | 223 | 0 | 0 |

## Harmonic identity × rhythm identity

F3 harmonic classes: **189**.

| Unique F5 per F3 | F3 classes |
|---:|---:|
| 5 | 189 |

## Cross-style F5 equivalence

F5 classes shared by multiple source styles: **0**.
- none in pinned source

## H4 contract

- exact H1/H2/H3 artifact digests are mandatory;
- F0-F4 are recomputed and verified unchanged;
- source rhythm masks are parsed without executing upstream code;
- unknown renderer pattern tokens are rejected rather than accepted through legacy fallback;
- style/key multiplicity never increases harmonic support;
- same-chord `S` means retrigger, not continuation;
- rest provenance is excluded from F5 identity;
- F5 is pitch-independent ChordRhythm identity;
- F6 is exact F3 + F5 combined identity;
- zero source definitions are removed and no runtime candidate is selected;
- source incidence never becomes runtime probability.

Next stage: **H5 Stage 15 representability report**. H5 may compare F3/F5/F6 evidence to current runtime contracts but must not change production code.
