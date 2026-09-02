# 0.9.9 PHRASE-H2R — H2 replay on finalized H1-F1/W1R

## Purpose
Replay the frozen PHRASE-H2 harmonic clock projection on finalized H1-F1/W1R ancestry. This checkpoint does not choose new harmonic policy and does not start P1R.

## Frozen predecessors
- H1-F1 FINAL: `eae498dc5b6377ddc4a45c2e62a7c33afab92e6c`
- W1R FINAL base: `329dcb91e40feb734182f437a8a50f2b61b40fd2`
- H1-F1 owns the intrinsic `ChordProgressionSource` WHAT grammar and arbitrary global-ordinal accessor `chordProgressionEventAt()`.
- W1R owns one-bar accepted F08 `HarmonicRhythm` WHEN semantics.

## Old H2 reference
- Old W1 base: `34912cd050c04727c13533575b2cf999816e0549`
- Old H2 final: `c9c0dc852dfb96b191c5d7066c81af99e3df189a`
- Decision A: PHRASE-WIDE HARMONIC CLOCK POLICY READY.
- Policy: BAR-LOCAL HARMONIC CLOCK CONCATENATION.

## Ownership
For every semantic phrase bar H2 realizes exactly one accepted F08 `HarmonicRhythmPlan`. The local one-bar clock restarts per bar. H2 concatenates local event positions into `PhraseHarmonicTimeline` with monotonically increasing phrase-global event ordinals.

`ChordProgressionSource` is not an H2 production owner or dependency. H2 produces WHEN positions/ordinals only. P1R will later consume the single phrase-global H1 WHAT source.

Mandatory semantic distinction: **event position != harmonic value transition**. Repeated static step-0 anchors do not assert repeated chord changes.

## Replay delta
Production replay is exactly one file:

`src/generation/composition/phrase_harmonic_clock_projection.h`

Its blob is byte-identical to old H2 final. No frozen H1-F1/W1R owner is changed.

The total H2R replay delta is limited to that production file plus focused tests, source guard, workflow, and this contract document.

## H1-F1 / W1R firewall
H2R does not modify production semantics from:
- `chord_progression.*`
- `harmonic_rhythm.*`
- `strong_rhythm_migration.*`
- `phrase_harmonic_timeline.*`
- `phrase_semantic_result.*`

H2 production neither selects `ChordProgressionSource` nor calls `chordProgressionEventAt()`.

Focused compatibility only proves that an H2-produced global ordinal can be resolved by finalized H1-F1. For `TwoFiveOne`, intrinsic period remains 3 and ordinal 8 resolves to intrinsic event index 2 through `chordProgressionEventAt()`.

## F08 compatibility
Accepted local clocks remain:
- StaticModal `{0}` count 1
- PedalDrone `{0}` count 1
- accepted moving progressions `{0,8}` count 2

Static totals for phrase lengths 1/2/4/8 are 1/2/4/8 positions. Moving totals are 2/4/8/16 positions.

Critical boundary:
- bar0 step0 -> ordinal 0
- bar0 step8 -> ordinal 1
- bar1 step0 -> ordinal 2
- bar1 step8 -> ordinal 3

## Capacity
`PhraseHarmonicTimeline` capacity remains 32 positions. Current production H2 reachability is at most 16 positions for 8 bars using accepted moving `{0,8}` F08. QuarterCycle and F08.1 are not imported.

## Fixture roles
H2R does not regenerate or reapprove tonal fixtures. Existing W1R separation remains authoritative: HISTORICAL PRE-F13, FROZEN F13, CURRENT ACCEPTED F08 GOLDEN.

## Hardware list
Focused H2R validation is host-only and requires no hardware. Normal validation compiles the existing Cardputer ADV firmware and SEQTRAK MIDI-only target; no new wiring or hardware behavior is introduced.

## Wiring
None for focused validation. Existing Cardputer ADV hardware wiring remains unchanged.

## Build / validation
Focused:
```sh
bash tests/run_0_9_9_phrase_h2r_tests.sh
```
This runs source guards, GCC, deterministic repeat comparison, Clang when available, ASan and UBSan.

Normal exact-head validation must additionally pass Core host, SDL, Cardputer ADV, fixed DRAM static/linker budget, SEQTRAK MIDI-only, E0a/E1a/M1 applicable gates, and relevant Stage15 tonal workflows.

## Expected behavior
The same request yields the same timeline and per-bar event ranges without a hidden cross-bar cursor. Empty melodic content does not suppress harmonic projection. ChordRhythm articulation cannot change the H2 timeline.

## Troubleshooting
- Any production difference from old H2 projection: STOP; replay drift.
- Any change to protected H1-F1/W1R owners: STOP; ancestry conflict.
- Any F08.1, QuarterCycle, Song, transport, MIDI execution, storage, UI, heap, C2/R1/I2/P1R production dependency: reject the replay.
- Any production call to `chordProgressionEventAt()` or `realizeChordProgressionSource()`: reject the replay; WHAT consumption belongs to P1R, not H2R.
- Cardputer fixed-DRAM CI evidence is static/linker evidence only; it is not runtime largest-free-block telemetry.

## Memory
Projection remains fixed-capacity and trivially copyable. No heap allocation is introduced. The synthetic 32-position timeline capacity is not expanded and production reachability remains 16 positions maximum under accepted F08 moving vocabulary.

## Provenance
H2R branch base is exact W1R final `329dcb91e40feb734182f437a8a50f2b61b40fd2`. Production projection is replayed byte-for-byte from old H2 final `c9c0dc852dfb96b191c5d7066c81af99e3df189a`.

## Acceptance checklist
- [ ] H2R branch merge-base equals W1R FINAL `329dcb91e40feb734182f437a8a50f2b61b40fd2`.
- [ ] Exactly one H2R commit above W1R FINAL.
- [ ] Production delta is exactly `src/generation/composition/phrase_harmonic_clock_projection.h`.
- [ ] Production projection byte-identical to old H2 final.
- [ ] H1-F1/W1R protected owners byte-identical to W1R FINAL.
- [ ] H2 production consumes neither `ChordProgressionSource` nor `chordProgressionEventAt()`.
- [ ] STATIC 1/2/4/8 -> 1/2/4/8.
- [ ] MOVING 1/2/4/8 -> 2/4/8/16.
- [ ] bar0 step8 ordinal 1; bar1 step0 ordinal 2.
- [ ] 8-bar axes: phrase 0..7; vocabulary 0,1,2,3,0,1,2,3; evolution 0,0,0,0,1,1,1,1.
- [ ] TwoFiveOne source period 3 and test-only `chordProgressionEventAt(..., 8)` resolves intrinsic index 2.
- [ ] Deterministic repeat passes.
- [ ] GCC / Clang / ASan / UBSan pass.
- [ ] Exact-head normal and Stage15 validation terminal green.

## Decision
On terminal-green validation, reproduce frozen Decision A:

**PHRASE-WIDE HARMONIC CLOCK POLICY READY**

**BAR-LOCAL WHEN + PHRASE-GLOBAL ORDINAL CONCATENATION**

Then hard stop. Fresh PHRASE-P1R is the next legitimate checkpoint; it is not part of H2R.
