# 0.9.9-PHRASE-C1 — Semantic Contract Convergence

## Purpose

Converge the frozen M2-A1, M3-A1, and M4-A1 representation gaps into one bounded semantic phrase contract without wiring runtime playback, storage, Song transport, UI, or MIDI/internal-synth execution.

Frozen evidence only:

- M2-A1 PR #381 — cross-bar note lifetime representation gap.
- M3-A1 PR #384 — phrase harmonic timeline representation gap.
- M4-A1 PR #385 — explicit phrase-length owner/representation boundary gap.

The audit branches are not merged or stacked. PHRASE-C1 starts from exact frozen M1/M1L SHA `5ad44bb9400ea38d349b7f815f84f833fb18ce6a`.

## Ownership contract

Dependency remains:

`Phrase Request -> phraseGenerationIdentity -> requested phrase length -> phraseBarOrdinal -> semantic rhythm/harmony/melody -> note lifetime -> materialization -> patternAddress -> Song/transport/backend`.

`patternAddress`, Song row, Bank slot, transport page, and UI state are downstream physical owners only.

### C1-0 temporal axes

Supported requested lengths are exactly `1/2/4/8` bars.

- global `phraseBarOrdinal = 0..N-1`
- `phraseVocabularyBarOrdinal(bar) = bar % 4`
- `evolutionOrdinal = bar / 4`

For eight bars:

- global: `0 1 2 3 4 5 6 7`
- vocabulary: `0 1 2 3 0 1 2 3`
- evolution: `0 0 0 0 1 1 1 1`

Existing `uint16_t phraseGenerationIdentity` and its unspecified sentinel remain unchanged.

### C1-M2 note lifetime

`MelodicCrossBarLifetime` carries only semantic boundary truth:

- enters from previous logical phrase bar;
- continues into next logical phrase bar.

The derived observable states distinguish started-here, continues-in, continues-out, and ends-here. A normal intra-phrase physical bar change may continue. `Stop` and transition outside the executing logical phrase release. Internal synth and MIDI must eventually consume the same semantic decision; C1 does not wire either backend.

Legacy MiniAcid `note == -2` is not generation truth and is not stored by this contract.

### C1-M4 requested phrase length

`requestedPhraseBars` is caller-owned and separate from profile/composition metadata. Exact requested length succeeds only when an existing production phrase law with that exact length is admissible; otherwise the request returns a typed rejection.

Current rejection reasons include:

- `InvalidPhraseLengthDomain`
- `NoAdmissibleLawForRequestedLength`
- `CompositionResolutionFailed`

No nearest-length or `8 -> 4` fallback exists. Admissibility is derived from the same production `GenerationProfileView::phraseLaws` and accepts the same `GenerationContext` as execution.

### C1-M3 harmonic time

`PhraseHarmonicTimeline` owns WHEN only.

Capacity domains remain separate:

1. semantic phrase harmonic time: `8 bars x 4 positions = 32 event positions`;
2. `ChordProgressionPlan` WHAT values: existing `kMaxHarmonicEvents = 8`;
3. physical phrase storage: separate downstream owner.

The timeline stores eight bounded `StepMask` values using the repository `stepBit()` convention. It maps `(phraseBarOrdinal, localHarmonicEventOrdinal)` to a global `phraseHarmonicEventOrdinal` without storing chord values.

An eight-bar QuarterCycle maps bars to `0..3`, `4..7`, ..., `28..31`. REST HEAVY melodic emptiness does not pause, collapse, or renumber harmonic time.

Current M3 observed baseline remains: a continuation crossing a harmonic change retains its onset source (`ONSET_SOURCE_STABLE`). A harmonic change alone does not imply retune, retrigger, or termination.

## Remaining policy gap / Decision

**DECISION B — REMAINING POLICY GAP.**

The timeline can represent `phraseHarmonicEventOrdinal = 17` while the frozen progression value carrier still owns only eight HarmonicEvent values. Existing policy does not define which WHAT-source, if any, event position 17 consumes.

Therefore:

`TIMELINE REPRESENTATION SUFFICIENT`

`PROGRESSION SOURCE RESOLUTION GAP`

PHRASE-C1 deliberately does not add modulo cycling, repeat-last, clamping, nearest-chord lookup, progression regeneration, or any other source-extension policy.

## Integrated semantic result

`PhraseSemanticResult` exposes without decoding physical pattern slots:

- stable `phraseGenerationIdentity`;
- requested and effective phrase bars;
- typed request rejection reason;
- global temporal coordinates and existing vocabulary/evolution projection;
- per-bar harmonic event ranges and phrase-wide timeline;
- cross-bar melodic lifetime in/out;
- authoritative `MelodicMotifStatus`, including `ValidButEmpty`.

No duplicate semantic-empty boolean is introduced.

## Future UI information mapping

No UI is implemented here.

- SETUP: semantic request context -> admissible lengths -> requested exact length -> typed rejection. Transport span and future storage preflight remain separate owners.
- FORM: `PhraseSemanticResult` -> phrase identity, bar count, global/evolution/vocabulary coordinates, `MelodicMotifStatus`, lifetime in/out, harmonic ranges.
- ADVANCED: read-only observability only. No held-note sustain/retrigger control and no reinterpretation of `gateLengthMultiplier` as cross-bar ownership.

## Future PHRASE-I2 storage requirement

Out of scope in C1. Eight-bar generation may replace up to `8 bars x 3 tracks`. PHRASE-I2 must preflight storage and own atomic snapshot/rollback, preferably by reusing existing Song-generation rollback machinery if a later audit proves it suitable.

## Memory / embedded constraints

All C1 semantic carriers are fixed-size and trivially copyable. No heap or unbounded containers are introduced. The focused test prints exact `sizeof()` values, largest new fixed array, and the worst-case integrated eight-bar semantic result size on the CI compiler.

The existing rhythm-vocabulary `kMaxPhraseBars = 4` is intentionally not changed; C1 introduces the separately named semantic capacity `kMaxSemanticPhraseBars = 8`.

## Hardware list

- Host Linux runner for the focused semantic contract.
- Cardputer ADV remains a regression target through the normal CI matrix.
- No hardware listening is required or permitted for C1.

## Wiring

None. This checkpoint has no physical I/O or hardware-listening path.

## Build / validation

Focused host gate:

```bash
python3 tests/test_0_9_9_phrase_c1_source_contract.py
bash tests/run_0_9_9_phrase_c1_tests.sh
```

The runner executes GCC, deterministic GCC repeat, Clang parity when available, ASan, and UBSan.

Normal repository regression matrix remains required on the Draft PR:

- Core host
- SDL
- Cardputer ADV
- fixed DRAM
- SEQTRAK MIDI-only
- M1-P1 / M1L software regressions
- frozen M2-A1 / M3-A1 / M4-A1 compatibility

## Expected behavior

Focused output must expose concrete states, not only a generic PASS:

- 1/2/4/8 temporal axes and eight-bar projections;
- exact-length success/rejection/admissibility agreement;
- 32-position eight-bar QuarterCycle timeline;
- REST HEAVY harmonic timeline continuity;
- cross-bar lifetime in/out and hard Stop/external barriers;
- backend-neutral lifetime decision;
- `ONSET_SOURCE_STABLE` baseline;
- `MelodicMotifStatus::ValidButEmpty` reuse;
- physical `patternAddress` invariance;
- type sizes and no-heap report;
- explicit `PROGRESSION SOURCE RESOLUTION GAP`.

## Troubleshooting

- If the focused build reports a `kMaxPhraseBars` collision, the semantic capacity has leaked into the existing four-bar rhythm-vocabulary owner; use `kMaxSemanticPhraseBars` only.
- If local harmonic step ordering is reversed, use `stepBit(step)` rather than raw `1 << step`.
- If an exact request succeeds with a different effective count, fail the contract; do not clamp or choose the nearest length.
- If ordinal >7 is resolved by cycling/clamping progression values, remove that policy; WHAT-source resolution is intentionally unresolved in C1.
- If `patternAddress` appears in a semantic carrier, the physical boundary has leaked upstream.

## Acceptance checklist

- [ ] Focused GCC PASS.
- [ ] GCC deterministic repeat PASS.
- [ ] Clang parity PASS.
- [ ] ASan PASS.
- [ ] UBSan PASS.
- [ ] Exact `1/2/4/8` request contract and typed rejection PASS.
- [ ] 8-bar QuarterCycle exposes 32 harmonic event positions.
- [ ] REST HEAVY empty bars do not collapse harmonic time.
- [ ] Cross-bar note lifetime is represented without `note == -2` generation ownership.
- [ ] Internal synth/MIDI lifetime decision representation is backend-neutral.
- [ ] `ONSET_SOURCE_STABLE` remains unchanged.
- [ ] `ValidButEmpty` is reused as semantic-empty authority.
- [ ] Integrated semantic result is invariant to physical destination `patternAddress`.
- [ ] No heap/unbounded storage introduced.
- [ ] Normal Core/SDL/Cardputer/fixed-DRAM/SEQTRAK regressions PASS.
- [ ] Decision remains B until a separate checkpoint defines progression WHAT-source resolution beyond current capacity.
