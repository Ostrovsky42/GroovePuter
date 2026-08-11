# Harmony Atlas H4 Rhythm Contract

**Status:** normative research contract for H4 ChordRhythm evidence  
**Runtime impact:** none

## Fingerprint status after H4

```text
F0 SourceIdentity              FROZEN_FROM_H2
F1 TranspositionInvariant      FROZEN_FROM_H2
F2 RootSequence                FROZEN_FROM_H2
F3 RootQualitySequence         FROZEN_FROM_H2
F4 FunctionalClassSequence     FROZEN_FROM_H3
F5 ChordRhythmIdentity         COMPUTED_H4_CHORD_RHYTHM
F6 CombinedIdentity            COMPUTED_H4_F3_PLUS_F5
```

H4 must verify the exact frozen H1, H2 and H3 artifact digests and recompute F0-F4 before producing F5/F6. H4 may not retroactively rewrite any earlier fingerprint identity.

## Source rhythm provenance

The pinned progression generator uses:

```text
gen.py
  styles = ['', 'pop', 'pop2', 'hiphop2', 'soul']

c2mpatterns.py
  N = next source progression event
  S = same current chord
  X = rest
  numeric prefix = duration multiplier in beats
```

H4 pins and verifies the exact Git blobs for `gen.py`, `c2mpatterns.py` and `c2m.py`. Extraction is static: upstream Python is not imported or executed and generated MIDI files are not decoded.

Unknown pattern syntax is rejected. H4 does not inherit `c2m.py`'s permissive legacy fallback for malformed pattern tokens.

## `S` means retrigger, not continuation

The pinned renderer calls `addNote(...)` for every non-rest expanded segment. Therefore an `S` segment creates another MIDI-note onset for the same current chord.

Normative H4 representation:

```text
N -> CHORD_ONSET
S -> CHORD_RETRIGGER
X -> REST_PATTERN
source X consumed by N/S -> REST_SOURCE
continuation -> false
```

`continuation_mask` remains part of the research shape because future rhythm sources may contain true continuation semantics, but the pinned H4 source produces zero continuation segments.

## F5 ChordRhythm identity

F5 is independent of absolute pitch, chord quality, source family, source style label and key projection.

F5 fingerprints the ordered realized sequence of:

```text
CHORD_ONSET
CHORD_RETRIGGER
REST
```

plus exact rational segment duration.

Rest provenance is not identity-bearing:

```text
REST_SOURCE  == REST_PATTERN  -> canonical F5 REST
```

The source of the rest remains available as a diagnostic statistic.

`CHORD_ONSET` and `CHORD_RETRIGGER` are not equivalent because they differ in whether the harmonic source advances.

Each F5 rhythm shape exposes:

```text
segment durations
note-onset coordinates
rest mask
same-chord-retrigger mask
continuation mask
source-advance mask
phrase length
```

## F6 Combined identity

F6 is exactly:

```text
F6 = hash(F3 RootQualitySequence + F5 ChordRhythmIdentity)
```

F4 is deliberately excluded. F6 must not replace F3 or F5 as the independent harmonic/rhythm evidence dimensions.

## Support accounting

The pinned source contains:

```text
190 logical progression definitions
5 generator style materializations per definition
12 key projections
```

Thus:

```text
logical harmonic support              190
rhythm-style observations             950
physical progression MIDI outputs   11400
```

Only logical definitions count as harmonic support. Neither style multiplicity nor key transposition can increase support or become a runtime probability.

## Realized vocabulary boundary

The pinned H4 source realizes 30 F5 identities because five generator masks are applied to six source form lengths (3, 4, 5, 6, 8 and 9 events).

This does **not** mean there are 30 independently authored rhythm masks. Reports must distinguish:

```text
source pattern masks used       5
source form-length classes      6
realized F5 identities         30
```

## Current pinned-source observations

The pinned source contains no explicit source-rest progression definitions. Therefore all 866 measured rest segments come from the `hiphop2` pattern and `source_rest_segments=0`.

Measured same-chord retriggers:

```text
hiphop2  846
soul     223
total   1069
```

Measured true continuations: **0**.

No F5 class is shared across multiple source styles in the pinned corpus. Every one of the 189 F3 harmonic classes has exactly five distinct F5 identities available across the five generator styles.

The five F6 duplicate groups are not new harmonic duplicates: each is the already-known `Modal:053` / `Modal:079` F3 duplicate pair under one of the five rhythm styles.

## Architecture boundary

H4 is R1 research evidence only.

H4 may not:

- add or modify production `ChordRhythm` vocabulary;
- change `ChordProgression`;
- change Tonal Projector;
- bind rhythms to Scene/Song/page/bank/slot state;
- create Genre/Mood-specific runtime switches;
- turn style incidence into weights;
- choose runtime candidates;
- remove source definitions.

The next stage is **H5 Stage 15 representability**. H5 may measure how much F3/F5/F6 evidence current production contracts can represent, but production expansion remains a later separate decision.
