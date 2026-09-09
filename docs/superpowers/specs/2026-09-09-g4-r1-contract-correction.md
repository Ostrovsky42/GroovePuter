# G4-R1 Contract Correction

This note is authoritative for G4-R1 and **supersedes** the parts of
`2026-09-09-g4-reference-genre-idea-contracts-design.md` that treated a
`RhythmFamily` Auto bass candidate list as universal legality, and the matching
implementation-plan steps that proposed rejecting all incompatible explicit
bass requests.

## Evidence that forced the correction

The first RED on `bf3e21fb9aeea8146166ef81d8e6c2fd7e8c5998` proved the expected
DnB and Dub Techno defects, but code inspection exposed a broader invariant:
Acid deliberately uses independent bass motion across multiple rhythm families.
In particular, `SparseAcid` is `RhythmFamily::SparsePulse` while the Acid
composition profile intentionally draws from `kBassDrive`.

Therefore:

> `BassRhythm::candidatesFor(RhythmFamily)` is the default vocabulary for Auto
> role selection. It is not, by itself, a universal legality contract for every
> explicit semantic bass identity.

Turning it into hard global legality would narrow a reference genre that is
already musically strong and would create the wrong owner for genre identity.

## Correct ownership

### Bass role

`bass_rhythm.*` continues to own physical realization of a requested bass
identity and its Auto defaults. G4-R1 does not add a new global rejection rule.

### Genre/profile editorial data

`GenerationProfileView` remains the owner of which weighted bass identities a
genre/recipe may select as its musical vocabulary.

For DnB, the structural prohibition is therefore expressed as a DnB profile
candidate space, not as a universal `RhythmFamily` law.

The first DnB contract is:

```text
KickAnswer
GapFill
HalfTimePocket
SyncopatedHook
```

with more than one identity remaining reachable. This protects the intended
fast-drums / slower-or-answering-bass relationship without changing Acid,
House, Dub, or unrelated explicit role requests.

## Revised Dub Techno contract

The original design text was also too aggressive where it implied that every
Dub Techno candidate should become strict four-floor.

The reference sound is already strong. The first correction must be additive
and conservative:

- keep the existing dub/space candidates 409–412;
- ensure the candidate space contains **more than one** independently selectable
  techno-skeleton statement;
- use an existing archetype rather than create a new genre/archetype;
- do not require 128/128 Dub Techno selections to be four-floor at this stage.

The current materialized structural witness is quarter-note kick anchors at
0/4/8/12. The R1 test requires at least two candidate archetypes satisfying
that witness, preventing Steppers from being the sole techno statement while
preserving non-four-floor dub ideas.

## Revised implementation sequence

1. Characterization RED: DnB profile incoherence + only one Dub Techno
   techno-skeleton candidate. Acid/House reference reachability remains GREEN.
2. DnB GREEN: change only DnB editorial bass candidate data. Do not change the
   general bass realizer.
3. Re-run focused gate and require DnB PASS while Dub remains RED.
4. Dub Techno GREEN: add the smallest justified existing techno/four-floor
   candidate at modest weight while keeping 409–412.
5. Re-run focused gate; then proceed to phrase-law truthfulness as a separate
   RED→GREEN checkpoint.

## Stop condition

If either correction audibly or structurally collapses Acid/House, or if Dub
Techno loses its space/omission identity because the test over-rewards
four-floor, revert the production change and revise the characterization rather
than weakening evidence after the fact.
