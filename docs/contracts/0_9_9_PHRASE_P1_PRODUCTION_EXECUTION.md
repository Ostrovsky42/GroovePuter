# 0.9.9-PHRASE-P1 — Production Phrase Execution

Status: **PRODUCTION CHECKPOINT — DECISION B**  
Exact base: `74456bcfec0fc74138ec0d8c652dde642c7e16b6`  
Base branch: `agent/20260826-11-0.9.9-phrase-h1-progression-source-contract`  
Branch: `agent/20260826-12-0.9.9-phrase-p1-production-execution`

## Purpose

Attempt the first production phrase semantic execution path after frozen C1 and
H1. P1 must join exact phrase length, one frozen composition, one frozen
progression WHAT source, phrase-wide harmonic WHEN coordinates, and deterministic
bar-local materialization without Song/Bank publication.

This checkpoint stops at a new production ownership blocker before changing
production code.

## Ownership

Frozen ownership remains:

```text
ChordRhythm       = physical articulation
HarmonicRhythm    = WHEN harmony advances
ChordProgression  = WHAT harmony advances to
Phrase timeline   = bounded WHEN coordinates, not a selector
patternAddress    = physical destination only
```

C1 supplies a bounded `PhraseHarmonicTimeline`. H1 supplies the authoritative
progression WHAT grammar/source semantics. Neither checkpoint defines which
harmonic WHEN clock current production should select for a logical phrase.

The current `GenerationProfileView` has no `HarmonicRhythm` or `HarmonicClock`
field. Current StrongRhythm still uses `chord.plan.onsets` as
`harmonicEventOnsets` and still realizes its local progression with
`phraseBars = 1`.

Promoting those chord articulation onsets to the phrase-wide harmonic clock
would collapse two explicitly separate owners. Adding a new clock/profile table
would be new harmonic policy/vocabulary, which P1 is forbidden to invent.

## Decision

**PHRASE-P1**  
**DECISION B — PRODUCTION HARMONIC WHEN OWNER MISSING**

The blocker is not the M2 lifetime producer. It is the absence of an
authoritative production owner that selects/populates phrase-wide harmonic WHEN
for a logical phrase.

H1 WHAT is ready; C1 WHEN representation is ready; production WHEN selection is
not.

No production implementation is added in this Decision-B checkpoint.

## H1 32-position reachability classification

Frozen H1 classification is preserved exactly:

**B — SYNTHETIC BOUNDED-CAPACITY FIXTURE ONLY.**

The semantic/source contract supports:

```text
8 bars × 4 positions/bar = 32 phrase harmonic event positions
```

and ordinal `17` resolves deterministically from one frozen progression source.
No current production profile admits the exact 8-bar + QuarterCycle combination.
P1 does not add vocabulary to make it reachable.

## Exact-length status

The blocker is not phrase-length admission. Existing production profiles already
contain genuine domains including:

```text
kPhraseCompact = 1 / 2 / 4
kPhraseDrive   = 2 / 4
kPhraseBroken  = 2 / 4
kPhraseSlow    = 4 / 8
```

C1 exact request resolution remains available and typed rejection remains
unchanged. P1 stops before a valid phrase-wide harmonic execution state can be
prepared, so it does not claim the requested 1/2/4/8 execution matrix as passed.

## M2 lifetime status

C1 `MelodicCrossBarLifetime` remains the frozen backend-neutral carrier. The
known non-trivial lifetime producer is still absent.

Because P1 cannot legally prepare the phrase-wide harmonic execution state, the
carrier is not yet wired through a new production P1 result type in this branch.
No lifetime policy is invented and the absence of a producer is **not** the P1
blocker.

Required follow-up remains:

**0.9.9-PHRASE-C2 — CROSS-BAR LIFETIME PRODUCER POLICY**

C2 must not start from this Decision-B checkpoint unless P1 is later unblocked
and reaches Decision A.

## I2 replay guarantee status

C1/H1 lower contracts are deterministic, but P1-T10 requires independently
replayable **production bar materialization from one valid frozen phrase
execution state**. Such a state cannot be constructed without choosing the
missing phrase-wide harmonic WHEN owner.

Therefore P1 does not falsely claim the I2 replay guarantee. I2 remains blocked
until a later P1 rerun reaches Decision A.

## A1 supersession / provenance

| Frozen A1 claim | Status in P1 | New owner | Why |
| --- | --- | --- | --- |
| M3-A1: no phrase harmonic carrier | SUPERSEDED | C1 `PhraseHarmonicTimeline` | C1 added bounded phrase-wide WHEN coordinates. |
| M3-A1: `phraseBars=1` current executable behavior | PRESERVED for legacy one-bar path | existing StrongRhythm | P1 does not change legacy behavior; phrase-wide path remains blocked. |
| M3-A1: `TonalMaterializationRequest` has no phrase carrier | STILL TRUE | TonalMaterializer remains bar-local | This is intentional; phrase execution should project a finite local view later. |
| M2-A1: no phrase-wide lifetime representation | SUPERSEDED | C1 `MelodicCrossBarLifetime` | C1 added the semantic carrier. |
| M2-A1: no authoritative cross-bar producer | STILL TRUE | future C2 | P1 is forbidden to invent lifetime musical policy. |
| M4-A1: no explicit requested length owner | SUPERSEDED | C1 exact phrase-length request | Exact 1/2/4/8 admission and typed rejection now exist. |
| M3-A1 observed harmonic timing derives from chord articulation | STILL TRUE in legacy path; NOT VALID as new phrase owner | unresolved production HarmonicRhythm owner | Frozen ownership now requires ChordRhythm articulation and HarmonicRhythm WHEN to stay distinct. |

Frozen A1 documents are not modified and their obsolete absence-oriented guards
are not used as P1 gates.

## Hardware

No hardware listening is required or permitted for this Decision-B checkpoint.
Normal Cardputer-Adv compilation/memory CI remains a regression gate because the
branch must not perturb embedded firmware.

## Wiring

No external hardware wiring.

Production wiring added by P1: **none**, because doing so would require choosing
new harmonic WHEN policy.

## Build / validation

From repository root:

```bash
bash tests/run_0_9_9_phrase_p1_tests.sh
```

The focused gate runs:

- P1-specific syntax-aware ownership/source guard;
- GCC;
- GCC deterministic repeat;
- Clang parity when available;
- ASan;
- UBSan.

Normal repository regression CI must also remain green.

## Expected behavior

Focused output must include:

```text
P1 source guard: production src delta=ZERO
P1 source guard: exact_8bar_phrase_admission=YES
P1 source guard: phrase_wide_harmonic_when_owner=ABSENT
P1 source guard: chord_articulation_cannot_be_promoted_to_when_owner
P1 source guard: forbidden_publication_runtime_policy=ABSENT
c1_when_representation=READY positions=32
h1_what_source=READY ordinal17=DETERMINISTIC
h1_32_position_reachability=SYNTHETIC_ONLY
production_phrase_wide_harmonic_when_owner=ABSENT
lifetime_blocker=NO
PHRASE-P1 DECISION_B
0.9.9-PHRASE-P1 production execution blocker gate: OK
```

## Troubleshooting

If a current production `HarmonicRhythm`/`HarmonicClock` owner is found in this
exact ancestry, this Decision B is invalid: characterize that owner and rerun P1
rather than adding another one.

If the only available timing source is `ChordRhythm` articulation, do not reuse
it as phrase-wide harmonic WHEN merely to pass P1. That would violate the frozen
ownership split.

If an implementation requires adding a genre/profile clock lookup, new harmonic
vocabulary, or accepting arbitrary caller-supplied phrase harmonic masks as the
production policy, stop: that is policy work outside P1.

## Memory

This Decision-B checkpoint adds no production types and no `src/` state.
Therefore incremental production state is:

```text
new P1 production types = none
incremental production fixed arrays = 0 B
incremental production heap = NO
incremental firmware semantic state = 0 B
```

Cardputer fixed-DRAM and largest-internal-block values are reported from normal
CI rather than estimated here.

## Acceptance checklist

- [x] C1 bookkeeping is formally closed at `f63db9b...`;
- [x] H1 is frozen at `74456bcf...` with Decision A;
- [x] H1 32-position reachability is synthetic-only;
- [x] H1 proves ordinal 17 deterministic;
- [x] P1 branch starts from exact frozen H1 SHA;
- [x] exact 8-bar phrase admission exists in current vocabulary;
- [x] C1 WHEN representation exists;
- [x] H1 WHAT source exists;
- [x] current production has no separate phrase-wide HarmonicRhythm/Clock owner;
- [x] current legacy path still derives harmonic timing from chord articulation;
- [x] no new harmonic vocabulary/policy introduced;
- [x] no lifetime producer invented;
- [x] no Song/Bank publication added;
- [x] no UI/MIDI/runtime lifetime ownership added;
- [x] P1 production `src/` delta is zero;
- [ ] final focused CI passes;
- [ ] final normal Core/SDL/Cardputer/fixed-DRAM/SEQTRAK CI passes;
- [ ] after terminal validation, freeze **Decision B** and STOP.

## What unblocks P1

A separate upstream contract must identify the authoritative production source
of phrase-wide harmonic WHEN without deriving it from physical chord
articulation and without P1 inventing new musical policy. After that contract is
frozen, P1 can be rerun from an explicit new base and attempt the required
production execution path.
