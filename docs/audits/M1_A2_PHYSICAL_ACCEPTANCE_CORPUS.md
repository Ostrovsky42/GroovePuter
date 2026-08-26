# M1-A2 — physical acceptance corpus decision

## Purpose

This checkpoint decides whether M1 phrase-materialization acceptance can use
only current directly admitted production vocabulary.  It does not change
composition policy, `kProfiles`, melodic semantics, or M1 wiring.

## Exact ancestry

Base: `3a68d2c6a22eed244ef42415e321164866509dd5` (M1-O1).

M1-T1F2 established the authoritative domain as 33 direct `kProfiles` pairs
times addresses `0..255` (8,448 production-path rows).  RestHeavy pure-Melodic
4-bar has zero rows; DriftPhrase pure-Melodic has zero rows; CallResponse
multi-onset has 260 rows.

O1 supplies the complete production-built `MelodicMotifRequest` immediately
before realization.  Every replay below copies that request and changes only
`barOrdinal`.

## P1 — space

The authoritative fixture Electro/base/address 19 selects pure Melodic,
SparseCall, Mirror, four bars, archetype 23, and progression 1.  Replay masks
are:

| bar ordinal | onsets | continuations | status |
| --- | --- | --- | --- |
| 0 | `{2}` (`0x2000`) | empty | Ok |
| 1 | empty | empty | ValidButEmpty |
| 2 | `{2}` (`0x2000`) | empty | Ok |
| 3 | empty | empty | ValidButEmpty |

P1 is **STRONG**: two explicit valid-empty bars are generated from one
production-captured request, not inferred from the rhythm name.

## P2 — bar-to-bar semantic differentiation

The proposed RepeatedCell fixture Acid/base/address 0 selects pure Melodic,
RepeatedCell, Pivot, four bars, archetype 7, progression 1.  All four replay
bars are `{0,4,8,12}` (`0x0888`), with no continuations.  It has one distinct
semantic bar and therefore fails P2.

The canonical exhaustive search visited all 8,448 directly admitted
profile/address rows, replaying each of 3,027 pure-Melodic four-bar rows from
its production-captured request.  Five rows have at least two distinct
semantic bars.  In `kProfiles` source order then ascending address order, the
first is Electro/base/address 19, SparseCall/Mirror: the same literal fixture
as P1, with two distinct bars.

The same fixture is retained as the canonical anchor for both independent
properties rather than selecting a later noncanonical row merely to give each
label a different name.  Its two assertions remain separate: P1 requires
space/valid emptiness; P2 requires semantic differentiation.

## P3 — call/response

Electro/base/address 7 remains directly admitted and selects pure Melodic,
DelayedAnswer, CallResponse, four bars.  Every replay yields the multi-onset
mask `{6,14}` (`0x0202`) with no continuations and the CallResponse motif
identity.  P3 passes as the authoritative multi-onset motif-relationship
anchor; it is not required to vary by bar ordinal.

## RestHeavy and DriftPhrase disposition

T3 continues to prove lower-level RestHeavy semantic empty-bar correctness.
RestHeavy is removed as a required physical composition fixture because it is
authoritatively unreachable in the pure-Melodic four-bar domain, while P1 now
proves production-reachable valid empty physical semantic material.

DriftPhrase is removed as a required physical fixture because it is likewise
authoritatively unreachable.  Its intended property, P2 differentiation, is
proven by the canonical SparseCall anchor without a composition-policy change.

## Static/moving harmonic duplication

**RELAXED.** `MelodicMotifRequest` does not contain a progression or harmonic
rhythm input.  M1's contract is stable selection plus four explicit bar
ordinals; duplicating these tests across static/moving harmony would measure
composition reachability rather than the melodic materialization owner.

## Final M1 mandatory corpus

| Property | Direct authoritative fixture | Proof |
| --- | --- | --- |
| P1 SPACE | Electro/base/address 19, SparseCall/Mirror | `{2}, empty, {2}, empty`; valid-empty bars |
| P2 BAR VARIATION | Electro/base/address 19, SparseCall/Mirror | two distinct semantic bars from one captured request |
| P3 CALL RESPONSE | Electro/base/address 7, DelayedAnswer/CallResponse | multi-onset `{6,14}`, CallResponse motif |
| empty-bar semantics | frozen M1-T1 T3 RestHeavy | lower-level `material, empty, empty, empty` |

The compact frozen masks and search counts are in
`tests/data/m1_a2_acceptance_corpus.tsv`.

## Compiler and sanitizer result

The executable replay snapshot is byte-identical across two GCC runs and
Clang; GCC ASan/UBSan is clean.  Normal firmware gates are inherited from the
frozen O1 evidence because A2 changes only tests and documentation.

## Decision and hard stop

**DECISION A — REVISED CORPUS VALID.** M1 implementation is unblocked for the
next checkpoint, `0.9.9-M1-P1 — MULTI-BAR MELODIC PHRASE PRODUCTION WIRING`.

Do not implement M1 in A2.  Do not change `kProfiles`, composition policy, or
melodic semantics.
