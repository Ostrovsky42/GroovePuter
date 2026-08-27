# 0.9.9-PHRASE-P1R — Production Phrase Execution

Status: **DECISION A — PRODUCTION PHRASE EXECUTION READY**

## Purpose

Wire the already-frozen phrase semantic owners into one bounded production execution path:

```text
exact phrase length
-> one composition / phrase identity
-> one StrongRhythmFrozenSelection
-> one H1 ChordProgressionSource
-> one H2 harmonic clock projection
-> one PhraseSemanticResult
-> random-access physical materialization by phraseBarOrdinal
```

P1R does not publish patterns, schedule transport, or implement non-trivial cross-bar note lifetime.

## Hardware

Production target remains M5Stack Cardputer ADV (ESP32-S3). Focused P1R semantics are host-testable and use no external peripheral. Final acceptance also requires the repository Cardputer ADV, fixed-DRAM and SEQTRAK MIDI-only jobs.

## Wiring

No external wiring is required for P1R. There are no PORT.A, I2C, SPI-display, MIDI-routing, or storage-topology changes in this checkpoint.

## Frozen ancestry

Branch:

`agent/20260827-04-0.9.9-phrase-p1r-production-execution`

Frozen P1R base:

`0f694187fa65e51c08468ce8b28ed88bb6bb8699`

That SHA is H2R FINAL. P1R must not change the frozen H1/H2/phrase-length/semantic owners.

The historical P1R-T0 blocker at `d760dfb8623b9cdad00b5a2d9d60c24ef451f738` remains immutable evidence that a finite `ChordProgressionPlan` cannot represent phrase-global WHAT for every intrinsic source period.

## Ownership

P1R adds only execution plumbing.

Frozen owners remain authoritative:

- `resolveGenerationCompositionForPhraseBars(...)` — exact 1/2/4/8 admission and typed reject.
- `ChordProgressionSource` / `chordProgressionSourceEventAt(...)` — phrase-global WHAT.
- `projectPhraseHarmonicClock(...)` — H2 phrase-wide projection of accepted one-bar WHEN.
- `PhraseSemanticResult` — authoritative semantic phrase carrier.
- `MelodicCrossBarLifetime` — lifetime carrier.

`PreparedPhraseExecution` owns no new musical policy.

## Prepared execution

`preparePhraseExecution(...)` produces one caller-owned `PreparedPhraseExecution` containing bounded immutable state only:

- exact `PhraseLengthRequestResult`,
- one `StrongRhythmFrozenSelection`,
- one phrase generation identity,
- one intrinsic `ChordProgressionSource`,
- one H2 `PhraseHarmonicClockProjection`,
- one existing `PhraseSemanticResult`,
- scalar materialization settings.

It stores no `DrumPatternSet[]`, `SynthPattern[]`, Song, Bank, Scene publication state, mutable bar cursor, or heap-owned object.

## Semantic preparation scan

`PhraseSemanticResult.bars[].melodicStatus` is observed through the actual strong production materializer.

Preparation reuses one caller-owned `PhraseExecutionScratch` physical bar. Before every semantic bar the scratch is reset to the same deterministic valid synth pitch source. The bar is materialized with the prepared H1/H2 override, semantic status is captured, and the physical result is discarded. The scratch is reset again before return.

Therefore preparation is bounded and deterministic while retaining no N-bar physical copy.

## Exact length

P1R calls `resolveGenerationCompositionForPhraseBars(...)` through the explicit phrase-length frozen-selection sibling.

Allowed domain: `1 / 2 / 4 / 8`.

A length outside that domain is rejected with `InvalidPhraseLengthDomain`. A supported length not admitted by the selected profile is rejected with `NoAdmissibleLawForRequestedLength`. No nearest-length fallback is introduced.

## ONE composition

The exact phrase-length frozen-selection path resolves the composition once and stores that result in the single `StrongRhythmFrozenSelection`. Semantic probing and later random-access physical materialization consume that frozen selection; they do not resolve composition again.

## ONE H1 source

Preparation realizes one `ChordProgressionSource` from the frozen progression identity and strong realization generation context. Every semantic bar points back to that same immutable source.

No bar independently selects progression WHAT.

## H2 WHEN

Preparation calls `projectPhraseHarmonicClock(...)` once. Each bar later consumes the prepared `HarmonicRhythmPlan` from that one projection through an ephemeral `StrongRhythmPhraseExecutionOverride`.

The legacy strong path continues to realize its local one-bar WHEN when the override pointer is null.

## Global WHAT

For a prepared bar, the strong materializer builds only the bounded finite progression window required by the existing tonal materializer.

For local harmonic event `i`:

```text
globalOrdinal = firstGlobalHarmonicOrdinal + i
localWindow[i] = chordProgressionSourceEventAt(oneFrozenSource, globalOrdinal)
```

The finite `ChordProgressionPlan` is therefore a consumer window, not a phrase-global source. P1R does not use `globalOrdinal % plan.eventCount` as source semantics.

## TwoFiveOne regression

The focused corpus freezes the intrinsic period-3 behavior:

```text
global 8  -> intrinsic 2
global 9  -> intrinsic 0
global 11 -> intrinsic 2
global 14 -> intrinsic 2
global 15 -> intrinsic 0
```

This directly covers the historical P1R-T0 failure mode.

## Random access

`materializePreparedPhraseBar(...)` takes `const PreparedPhraseExecution&` plus an explicit `phraseBarOrdinal` and caller-owned physical outputs.

The focused order is:

`7 -> 0 -> 4 -> 7`

Both bar-7 materializations must be canonically identical. No previous-bar cursor, Song position or publication state is consulted.

## Pattern address invariance

`physicalPatternAddress` remains a storage/destination validity coordinate. Explicit `phraseBarOrdinal` is the musical coordinate.

The same prepared phrase and same semantic bar are materialized to two different valid physical addresses from identical caller input material; all canonical drum/synth musical fields must match.

## ValidButEmpty

The focused corpus uses a production-reachable sparse melodic selection and requires a bar with `MelodicMotifStatus::ValidButEmpty` to remain inside the effective phrase.

That bar retains:

- global phrase bar ordinal,
- vocabulary/evolution coordinates,
- harmonic event range,
- the unchanged H2 timeline,
- random-access physical materializability.

No phrase vocabulary is expanded.

## Lifetime carrier

P1R status is:

**LIFETIME CARRIER WIRED / NON-TRIVIAL PRODUCER ABSENT**

Every prepared bar carries the existing `MelodicCrossBarLifetime`. P1R intentionally initializes it to:

```text
entersFromPreviousBar = false
continuesIntoNextBar = false
```

P1R does not implement C2 topology, alter gates, suppress AllNotesOff, or touch transport/MIDI lifetime execution.

## Legacy compatibility

`StrongRhythmPhraseExecutionOverride* == nullptr` remains the legacy branch. Existing one-bar and M1 frozen-selection callers therefore continue to realize local WHEN/WHAT as before.

The P1R focused gate also executes the unchanged M1 P1 production corpus as a compatibility oracle.

## Memory

Actual focused host-ABI measurements on implementation candidate `d6d7ebd6a633ceb5bf379e78dd8bb2ef17b315da`:

```text
sizeof(PreparedPhraseExecution)      = 324
sizeof(PhraseSemanticResult)         = 82
sizeof(StrongRhythmFrozenSelection)  = 48
sizeof(ChordProgressionSource)       = 14
sizeof(SynthPattern)                 = 112
sizeof(DrumPatternSet)               = 1192
```

P1R production code performs no heap allocation and retains no physical N-bar array.

Cardputer fixed-DRAM evidence is **static/linker evidence only**. It is separate from runtime-largest-free-block evidence and must not be interpreted as a runtime heap-fragmentation measurement.

## I2 informational estimate

The focused test prints a conservative physical 8-bar old-state/staging estimate:

```text
oneBar     = 1416
oldState8  = 11328
staging8   = 11328
combined   = 22656
```

These values are **INFORMATIONAL I2 ESTIMATE ONLY / NOT I2 POLICY**. P1R does not allocate those arrays.

## Validation

Focused command:

```bash
bash tests/run_0_9_9_phrase_p1r_tests.sh
```

The runner requires:

- source firewall,
- GCC,
- deterministic repeat,
- Clang when available,
- ASan,
- UBSan,
- unchanged M1 P1 legacy compatibility corpus.

Implementation candidate `d6d7ebd6a633ceb5bf379e78dd8bb2ef17b315da` completed the required matrix terminal GREEN:

| Gate | Run ID | Result |
| --- | ---: | --- |
| Focused P1R | `33092843298` | GREEN |
| Core host / SDL / Cardputer ADV / fixed DRAM / SEQTRAK MIDI-only | `33092843333` | GREEN |
| Stage15 baseline | `33092843244` | GREEN |
| Stage15 tonal register sweep | `33092843217` | GREEN |
| Tonal Materializer | `33092843228` | GREEN |
| Stage15 tonal integration | `33092843391` | GREEN |
| Final tonal acceptance | `33092843723` | GREEN |
| Tonal Projector | `33092843265` | GREEN |
| Global scale | `33092843366` | GREEN |

Core run `33092843333` includes terminal-GREEN host regressions, SDL, Cardputer ADV build, fixed-DRAM budget check and SEQTRAK MIDI-only build. The fixed-DRAM result is static/linker-only evidence.

Final tonal acceptance completed the exhaustive address/key-scale path and frozen legacy corpus without tonal golden regeneration.

This documentation commit changes HEAD. Technical freeze therefore remains conditional on rerunning this complete required matrix on the exact documentation SHA and obtaining terminal GREEN again.

## Troubleshooting

If focused linking reports `resolveGenerationCompositionForPhraseBars(...)` unresolved, confirm `src/generation/composition/phrase_length_request.cpp` is present in every legacy host source list that directly links `strong_rhythm_migration.cpp`.

If semantic preparation reports `SemanticProbeFailure`, verify the scratch reset still supplies a deterministic valid synth pitch source; do not weaken `projectLegacyPitchPattern*` input validation.

If a phrase bar differs only when its physical destination address changes, inspect any fallback to `patternAddress`; prepared phrase execution must always set explicit `phraseBarOrdinal` and frozen selection.

If a TwoFiveOne failure appears after global ordinal 7, inspect the P1R consumer-window construction. The intrinsic `ChordProgressionSource.period`, not finite local plan length, owns wraparound.

## Provenance

- Frozen H2R base: `0f694187fa65e51c08468ce8b28ed88bb6bb8699`.
- Historical P1R-T0 evidence: `d760dfb8623b9cdad00b5a2d9d60c24ef451f738`.
- P1R production implementation: `32e137e857c866b19b3bb2e91549a3767c5528f9`.
- CI target correction: `15878e9e892d01f8c385176dc776c6fc45baeac2`.
- Legacy host manifest fixes: `5a8cfbee6f395ae6c47a185fe93e30f65e755dd8`, `d47a335d90427768101087a8de62e228b0d773f3`, `d6d7ebd6a633ceb5bf379e78dd8bb2ef17b315da`.
- Stage15 source-oracle alignment and literal repair: `aa46e2a0b0167819cb308700c3612079614e5c26`, `4fcb8f0191f0d96f09fd0776abc6743a9c186779`.
- Implementation candidate validated GREEN: `d6d7ebd6a633ceb5bf379e78dd8bb2ef17b315da`.
- Commits after `32e137e857c866b19b3bb2e91549a3767c5528f9` in the chain above are CI/build/source-oracle integration only; they do not change the P1R production seam or musical policy.
- Tonal goldens were not regenerated.
- Frozen H1/H2/phrase-length/semantic policy owners are protected by an exact-base source guard.

## Acceptance checklist

- [x] exact 1/2/4/8 admitted where profile policy permits
- [x] typed invalid-domain and inadmissible-length rejects
- [x] one composition / one phrase identity
- [x] one H1 source
- [x] one H2 projection
- [x] global WHAT projected from intrinsic source
- [x] TwoFiveOne global ordinal 8 -> intrinsic event 2
- [x] random access `7 -> 0 -> 4 -> 7`
- [x] pattern-address independence
- [x] `PhraseSemanticResult` remains authoritative
- [x] production-reachable `ValidButEmpty` retained
- [x] lifetime carrier present and inert
- [x] legacy nullptr-override compatibility
- [x] no Song/Bank/storage publication
- [x] no frozen-owner drift
- [x] no heap / no retained physical N-bar array
- [x] focused GCC/repeat/Clang/ASan/UBSan green
- [x] Core host green
- [x] SDL green
- [x] Cardputer ADV green
- [x] fixed DRAM green — static/linker evidence only
- [x] SEQTRAK MIDI-only green
- [x] required Stage15/tonal matrix green

## Decision

**DECISION A — PRODUCTION PHRASE EXECUTION READY**

Implementation candidate acceptance is complete. Technical freeze is permitted only after the complete required matrix is terminal GREEN again on the exact documentation SHA created by this freeze update.

After that exact-SHA validation: PHRASE-P1R FINAL, then HARD STOP.

C2 is not part of this checkpoint and remains NOT STARTED.
