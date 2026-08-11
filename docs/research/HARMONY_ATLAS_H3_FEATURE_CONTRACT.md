# Harmony Atlas H3 Feature Contract

**Status:** normative research contract for H3 derived evidence  
**Runtime impact:** none

## Fingerprint status after H3

```text
F0 SourceIdentity              FROZEN_FROM_H2
F1 TranspositionInvariant      FROZEN_FROM_H2
F2 RootSequence                FROZEN_FROM_H2
F3 RootQualitySequence         FROZEN_FROM_H2
F4 FunctionalClassSequence     COMPUTED_H3
F5 ChordRhythmIdentity         DEFERRED_H4_CHORD_RHYTHM
F6 CombinedIdentity            DEFERRED_UNTIL_F5
```

H3 must recompute and verify F0-F3 against frozen H2 before deriving F4. F4 is a high-level analytic equivalence only and has **no destructive-dedup authority**.

## Derived feature definitions

`chromatic_root_count`
: Number of CHORD events whose normalized `alteration_semitones != 0`.

`borrowed_candidate_count`
: Number of CHORD events matching H3's explicit conservative borrowed-candidate rules. This is candidate evidence only and must never be reported as confirmed historical/compositional borrowing.

`dominant_resolution_count`
: Number of adjacent non-REST functional transitions `DOMINANT -> TONIC`.

`tonic_return_distance`
: If the final CHORD is `TONIC` and an earlier `TONIC` exists, the number of CHORD-index steps from the immediately preceding tonic to the final tonic. Otherwise `null`.

`unique_degree_count`
: Number of unique `(diatonic_degree, alteration_semitones)` pairs among CHORD events.

`quality_entropy_bits`
: Shannon entropy, base 2, over H1 semantic quality signatures within one progression. The quality signature is triad class + extension class + seventh flavor + fifth alteration.

`repetition_period_f3_events`
: Smallest exact event period `p` that divides the ordered F3 sequence and repeats at least twice. Otherwise `null`. This feature does not collapse the longer source form.

`functional_motion_histogram`
: Histogram of adjacent non-REST `functional_role` transitions.

`cadence_class`
: Terminal two-CHORD heuristic. H3 may emit authentic/plagal/half/deceptive **candidates** under explicit reason codes. Modal source definitions remain `MODAL_AMBIGUOUS`; common-practice cadence semantics are not forced.

`closure_class`
: Deterministic priority: modal ambiguity -> terminal cadence candidate -> dominant ending -> tonic start/end -> turnaround heuristic -> open loop. Every result carries a reason code.

## Classification safety

Major/Minor functional roles use a deliberately bounded root-degree heuristic:

```text
I      -> TONIC
II, IV -> PREDOMINANT
V      -> DOMINANT
other unaltered roots -> UNALTERED_OTHER
altered roots         -> CHROMATIC_COLOR
```

`UNALTERED_OTHER` intentionally does **not** claim the resulting chord quality is diatonic.

Modal policy is deliberately weaker:

```text
unaltered tonic -> TONIC
all other modal events -> MODAL_COLOR
```

Low-confidence / modal ambiguity is a valid H3 result and must not be upgraded silently.

## Source metadata boundary

Source structural tag `Cadence` is independent evidence. It does not force a derived common-practice cadence class. In particular, a source-tagged Modal progression may remain `TAGGED_MODAL_UNRESOLVED`.

## Deferred features

Timing-based harmonic event density is not a valid H3 claim because H3 has no ChordRhythm timing evidence. It remains deferred to H4.

No H3 feature may become a runtime selection weight, runtime progression ID, absolute MIDI projection, or Scene/Song state owner.
