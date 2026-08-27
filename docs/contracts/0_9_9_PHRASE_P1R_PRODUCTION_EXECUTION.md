# 0.9.9-PHRASE-P1R — Production Phrase Execution

Status: **DECISION_A_CANDIDATE**

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

The focused executable prints actual host-ABI values for:

- `sizeof(PreparedPhraseExecution)`
- `sizeof(PhraseSemanticResult)`
- `sizeof(StrongRhythmFrozenSelection)`
- `sizeof(ChordProgressionSource)`
- `sizeof(SynthPattern)`
- `sizeof(DrumPatternSet)`

Final measured values are recorded only after the exact-head focused CI gate executes. P1R production code performs no heap allocation and retains no physical N-bar array.

Cardputer fixed-DRAM linker evidence is separate from runtime-largest-free-block evidence.

## I2 informational estimate

The focused test also prints a conservative physical 8-bar old-state/staging estimate:

```text
oneBar = sizeof(DrumPatternSet) + 2 * sizeof(SynthPattern)
oldState8 = 8 * oneBar
staging8 = 8 * oneBar
combined = oldState8 + staging8
```

This is **INFORMATIONAL ONLY / NOT I2 POLICY**. P1R does not allocate those arrays.

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

Final Decision A additionally requires terminal-green repository jobs for Core host, SDL, Cardputer ADV, fixed DRAM, SEQTRAK MIDI-only and the required Stage15/tonal matrix.

## Troubleshooting

If focused linking reports `resolveGenerationCompositionForPhraseBars(...)` unresolved, confirm `src/generation/composition/phrase_length_request.cpp` is present in the shared Stage15 host source list.

If semantic preparation reports `SemanticProbeFailure`, verify the scratch reset still supplies a deterministic valid synth pitch source; do not weaken `projectLegacyPitchPattern*` input validation.

If a phrase bar differs only when its physical destination address changes, inspect any fallback to `patternAddress`; prepared phrase execution must always set explicit `phraseBarOrdinal` and frozen selection.

If a TwoFiveOne failure appears after global ordinal 7, inspect the P1R consumer-window construction. The intrinsic `ChordProgressionSource.period`, not finite local plan length, owns wraparound.

## Provenance

- Frozen H2R base: `0f694187fa65e51c08468ce8b28ed88bb6bb8699`.
- Historical P1R-T0 evidence: `d760dfb8623b9cdad00b5a2d9d60c24ef451f738`.
- P1R changes are bounded to strong-rhythm execution plumbing, new phrase-execution files, tests/guards/workflow and this contract.
- Frozen H1/H2/phrase-length/semantic policy owners are protected by an exact-base source guard.

## Acceptance checklist

- [ ] exact 1/2/4/8 admitted where profile policy permits
- [ ] typed invalid-domain and inadmissible-length rejects
- [ ] one composition / one phrase identity
- [ ] one H1 source
- [ ] one H2 projection
- [ ] global WHAT projected from intrinsic source
- [ ] TwoFiveOne global ordinal 8 -> intrinsic event 2
- [ ] random access `7 -> 0 -> 4 -> 7`
- [ ] pattern-address independence
- [ ] `PhraseSemanticResult` remains authoritative
- [ ] production-reachable `ValidButEmpty` retained
- [ ] lifetime carrier present and inert
- [ ] legacy nullptr-override compatibility
- [ ] no Song/Bank/storage publication
- [ ] no frozen-owner drift
- [ ] no heap / no retained physical N-bar array
- [ ] focused GCC/repeat/Clang/ASan/UBSan green
- [ ] Core host green
- [ ] SDL green
- [ ] Cardputer ADV green
- [ ] fixed DRAM green
- [ ] SEQTRAK MIDI-only green
- [ ] required Stage15/tonal matrix green

## Decision

**DECISION_A_CANDIDATE**

Do not declare `DECISION A — PRODUCTION PHRASE EXECUTION READY` until the complete required matrix is terminal green on one exact final HEAD.

HARD STOP after P1R FINAL. C2 is not part of this checkpoint.
