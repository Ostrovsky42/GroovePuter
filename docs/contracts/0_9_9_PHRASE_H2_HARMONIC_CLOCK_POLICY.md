# 0.9.9-PHRASE-H2 — Phrase-wide Harmonic Clock Policy

## Purpose
Freeze the cross-bar HarmonicRhythm WHEN policy for semantic phrases. H2 concatenates one accepted F08 one-bar plan per semantic bar into the existing C1 PhraseHarmonicTimeline. It does not wire runtime execution.

## Hardware
No new hardware listening is required for H2. Accepted F08 hardware equivalence is inherited for the one-bar HarmonicRhythm owner only. Multi-bar phrase behavior is not final hardware-accepted behavior until later production execution/listening work.

## Hardware list
None for this semantic host contract.

## Wiring
None.

## Ownership
- W1/F08 HarmonicRhythm owns one-bar WHEN.
- H2 owns cross-bar WHEN projection policy.
- C1 PhraseHarmonicTimeline represents phrase-global event positions.
- H1 ChordProgression remains the single phrase-global WHAT source.
- P1R will own production execution wiring; H2 does not publish runtime state.

## Policy
Each semantic bar realizes exactly one authoritative F08 HarmonicRhythm plan. Local onsets restart at the bar boundary. H2 concatenates those local positions into monotonically increasing phrase-global harmonic event ordinals.

`phraseHarmonicPosition` is the `PhraseHarmonicEventRange.firstOrdinal` base carried into the F08 request for that bar. It does not change the local F08 clock.

## STATIC
Static progressions preserve F08 `{0}` in every semantic bar.

| phrase bars | phrase event positions |
| ---: | ---: |
| 1 | 1 |
| 2 | 2 |
| 4 | 4 |
| 8 | 8 |

The `{0}` position in each independently materializable bar is a state anchor.

## MOVING
The accepted F08 moving bootstrap preserves `{0,8}` in every semantic bar.

| phrase bars | phrase event positions |
| ---: | ---: |
| 1 | 2 |
| 2 | 4 |
| 4 | 8 |
| 8 | 16 |

## Event position vs value transition
**event position != harmonic value transition**

A phrase harmonic event position is a temporal/state anchor. It does not by itself assert that the harmonic value differs from the previous event. In particular, static `{0}` at each bar boundary must not be interpreted as a forced repeated chord change.

## WHEN local / WHAT global
WHEN restarts locally per semantic bar. WHAT does not restart per bar: one H1 frozen progression source is selected for the phrase and phrase-global harmonic ordinals address that source continuously.

Critical boundary:

```text
bar0 step8 -> ordinal 1
bar1 step0 -> ordinal 2
WHAT       -> frozen H1 source ordinal 2
```

## Phrase coordinate
For each bar H2 records `PhraseHarmonicEventRange { firstOrdinal, eventCount }`. `firstOrdinal` is also carried as the F08 `phraseHarmonicPosition`. No second cursor or parallel coordinate exists.

## 32 synthetic firewall
C1 retains fixed capacity for 32 event positions because historical H1 tests use a synthetic eight-bar QuarterCycle-shaped fixture. H2 production policy does not import that clock. Eight-bar F08 moving projection produces 16 positions, not 32.

F08.1 named clocks and genre/feel/BPM clock tables are not imported.

## REST HEAVY
Melodic density and rests are not inputs to H2. A REST-heavy melodic bar therefore cannot suppress or advance harmonic time.

ChordRhythm articulation is also not an H2 input and cannot change phrase harmonic event positions.

## Build / validation
Run:

```bash
bash tests/run_0_9_9_phrase_h2_tests.sh
```

The focused gate runs the source firewall, GCC, deterministic repeat comparison, Clang when available, ASan, and UBSan. Final acceptance also requires the repository normal CI matrix to be terminal green on the exact H2 head.

## Expected behavior
Host output reports STATIC totals `1/2/4/8`, MOVING totals `2/4/8/16`, the bar boundary ordinals `1 -> 2`, one H1 source selection in the integration proof, eight moving bars producing 16 positions, and `F08.1 imported=NO`.

## Troubleshooting
- If a bar uses anything other than accepted F08 `{0}` or `{0,8}`, stop: H2 has imported a new clock policy.
- If bar 1 restarts WHAT at ordinal 0, stop: phrase-global H1 continuity is broken.
- If a protected W1/H1/C1 owner differs from frozen W1, stop: H2 scope has expanded.
- If 8-bar moving produces 32 positions, stop: the synthetic capacity fixture leaked into production policy.
- If any required CI job is queued, running, skipped unexpectedly, or failed, Decision A is not available.

## Memory
H2 uses fixed-size arrays only and introduces no heap allocation. Normal Cardputer fixed-DRAM CI remains an acceptance gate. Runtime largest-free-block must only be reported if directly measured; linker/static DRAM figures are not a substitute.

## Provenance
- PHRASE-H1: `74456bcfec0fc74138ec0d8c652dde642c7e16b6`
- PHRASE-W1 technical freeze: `34912cd050c04727c13533575b2cf999816e0549`
- Accepted F08 authority: `cfb1f9a8e214cfcb823a5e75445f26356b55bed6`
- #388 remains frozen blocker evidence and is not modified by H2.

## Acceptance checklist
- [ ] H2 branch is based exactly on frozen W1.
- [ ] Protected W1/H1/C1 owners are unchanged.
- [ ] One F08 HarmonicRhythm realization occurs per semantic bar.
- [ ] STATIC 1/2/4/8 bars -> 1/2/4/8 event positions.
- [ ] MOVING `{0,8}` 1/2/4/8 bars -> 2/4/8/16 event positions.
- [ ] bar0 step8 is ordinal 1; bar1 step0 is ordinal 2.
- [ ] H1 source selection count is one and ordinal 2 does not reset to ordinal 0.
- [ ] `event position != harmonic value transition` remains explicit.
- [ ] C1 32-position capacity remains synthetic-only for H2 production.
- [ ] F08.1 is not imported.
- [ ] No P1 runtime wiring, Song/Bank/transport/UI/MIDI/synth, or lifetime behavior is added.
- [ ] Focused GCC/repeat/Clang/ASan/UBSan is terminal green.
- [ ] Required normal CI is terminal green on exact head.

## Decision
`DECISION_A_CANDIDATE` until every required exact-head gate is terminal green. A failure that requires broadening H2 ownership is a hard stop and must become Decision B or C rather than a policy workaround.
