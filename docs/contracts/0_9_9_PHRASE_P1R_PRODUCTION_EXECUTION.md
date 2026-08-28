# 0.9.9 PHRASE-P1R — Fresh Production Phrase Execution Replay

Status: **DECISION_A_CANDIDATE**

## Purpose
Replay the frozen PHRASE-P1R production-execution contract on the finalized H1-F1 → W1R → H2R ancestry. This checkpoint wires the already-frozen phrase semantic owners into bounded random-access physical materialization. It does not add C2 lifetime policy, Song/Bank publication, transport behavior, UI, MIDI policy, or storage ownership.

## Exact ancestry
- H1-F1 FINAL: `eae498dc5b6377ddc4a45c2e62a7c33afab92e6c`
- W1R FINAL: `329dcb91e40feb734182f437a8a50f2b61b40fd2`
- H2R FINAL / exact P1R base: `06ffcdc01969eb73b6bd8a452cc9b261a5b51e28`
- frozen old P1R reference: `016bcd6ba514b3a57f8803c63c869f1b2a8953a7`
- old P1R Decision A: **PRODUCTION PHRASE EXECUTION READY**

## Production owner set
Exactly four production files may differ from the H2R base:

```text
src/generation/migration/phrase_execution.h
src/generation/migration/phrase_execution.cpp
src/generation/migration/strong_rhythm_migration.h
src/generation/migration/strong_rhythm_migration.cpp
```

`phrase_execution.h`, `phrase_execution.cpp`, and `strong_rhythm_migration.cpp` replay byte-for-byte from frozen old P1R. `strong_rhythm_migration.h` contains only the corrected-ancestry API adapter described below.

## Corrected H1-F1 accessor adaptation
Frozen old P1R consumed the phrase-global WHAT source through:

```text
chordProgressionSourceEventAt(source, ordinal, out)
```

Finalized H1-F1 exposes:

```text
chordProgressionEventAt(source, ordinal)
    -> ChordProgressionEventResult
```

Fresh P1R keeps the old execution algorithm unchanged and supplies a local adapter inside the already-authorized P1R migration owner. The adapter accepts only `ChordProgressionStatus::Ok` or `ValidButStatic`, copies `result.event`, and fails closed otherwise.

No finite `ChordProgressionPlan` modulo fallback is allowed. `src/generation/roles/chord_progression.*` remains byte-identical to finalized H1-F1.

## Frozen execution contract
Preparation performs exactly one bounded phrase setup:
- exact requested phrase length 1/2/4/8 through the existing phrase-length owner;
- one `StrongRhythmFrozenSelection` / one phrase generation identity;
- one H1 `ChordProgressionSource` for the logical phrase;
- one H2 `PhraseHarmonicClockProjection`;
- one authoritative `PhraseSemanticResult`;
- one caller-owned reusable physical scratch bar for semantic probing.

Physical materialization is random-access by explicit `phraseBarOrdinal`. `physicalPatternAddress` is only a destination/storage coordinate and must not become musical phrase identity.

The transient `StrongRhythmPhraseExecutionOverride` carries already-frozen H2 WHEN and H1 WHAT into the existing one-bar materializer. A null override preserves the legacy path.

## Semantic requirements
- accepted phrase lengths remain exact 1/2/4/8 where profile policy permits;
- invalid-domain and inadmissible lengths retain typed reject reasons;
- phrase-global progression source is selected once;
- H2 WHEN remains bar-local with phrase-global ordinal concatenation;
- WHAT is resolved from the intrinsic H1 source by phrase-global harmonic ordinal;
- `TwoFiveOne` intrinsic period remains 3 and global ordinal 8 resolves intrinsic event index 2;
- materialization order may be `7 -> 0 -> 4 -> 7` without mutable hidden cursor state;
- repeated materialization of the same semantic bar remains identical across different physical pattern addresses;
- production-reachable `ValidButEmpty` semantic bars remain legal;
- `MelodicCrossBarLifetime` carrier is present but all-false in P1R; C2 is not imported.

## Frozen-owner firewall
P1R must not modify:
- `src/generation/roles/chord_progression.*`
- `src/generation/roles/harmonic_rhythm.*`
- `src/generation/composition/phrase_harmonic_clock_projection.h`
- `src/generation/composition/phrase_harmonic_timeline.h`
- `src/generation/migration/phrase_semantic_result.h`
- phrase-length and generation-profile owners.

No Song/Bank/Scene publication, project storage, transport, MIDI lifecycle, UI ownership, heap allocation, or retained physical N-bar array is permitted.

## Hardware list
Focused P1R validation is host-only and requires no hardware. Normal CI compiles the existing Cardputer ADV target and the existing SEQTRAK MIDI-only target. No new hardware interface or wiring is introduced.

## Wiring
None. Existing Cardputer ADV hardware wiring is unchanged.

## Build / flash / validation
Focused host gate:

```sh
bash tests/run_0_9_9_phrase_p1r_tests.sh
```

It runs the P1R source firewall, GCC deterministic repeat, Clang parity when available, ASan, UBSan, and the unchanged M1 P1 compatibility corpus.

Normal exact-head validation must additionally pass the inherited repository matrix, including Core host, SDL, Cardputer ADV, fixed DRAM, SEQTRAK MIDI-only and required Stage15/tonal workflows.

No firmware flashing is required for this checkpoint because P1R introduces no new user-facing hardware interaction. Cardputer validation is compile/static-budget acceptance.

## Expected behavior
Preparation returns `Ready` for admitted phrase requests and leaves one fixed-capacity prepared semantic carrier. Any bar can then be materialized directly by its `phraseBarOrdinal` without first visiting previous bars. The same bar/identity must produce the same physical result regardless of destination pattern address.

The legacy one-bar materializer remains unchanged when `phraseExecutionOverride == nullptr`.

## Memory
P1R uses caller-owned fixed-capacity state only. Frozen old P1R host-ABI evidence was:

```text
sizeof(PreparedPhraseExecution)      = 324
sizeof(PhraseSemanticResult)         = 82
sizeof(StrongRhythmFrozenSelection)  = 48
sizeof(ChordProgressionSource)       = 14
sizeof(SynthPattern)                 = 112
sizeof(DrumPatternSet)               = 1192
```

No heap allocation and no retained physical N-bar array are allowed. Cardputer fixed-DRAM CI remains static/linker evidence only, not runtime largest-free-block telemetry.

## Troubleshooting
- If `chordProgressionSourceEventAt` is unresolved, verify the P1R-local corrected-ancestry adapter still delegates to finalized `chordProgressionEventAt()`.
- If the adapter returns invalid status, inspect the frozen H1-F1 source/result contract; do not add a finite-plan modulo fallback.
- If `resolveGenerationCompositionForPhraseBars(...)` is unresolved in a legacy host runner, confirm the old P1R source-manifest additions are present.
- If semantic preparation returns `SemanticProbeFailure`, keep the deterministic one-bar scratch pitch seed; do not weaken the semantic projector input contract.
- Any frozen-owner diff, Song/Bank/storage dependency, heap allocation, physical N-bar retention, or non-inert cross-bar lifetime is a STOP condition.

## Acceptance checklist
- [ ] merge-base is exact H2R FINAL `06ffcdc01969eb73b6bd8a452cc9b261a5b51e28`
- [ ] exactly one fresh P1R replay commit above H2R
- [ ] production delta is exactly the four P1R owners
- [ ] H1-F1/W1R/H2R frozen owners unchanged
- [ ] old P1R execution cpp and phrase-execution files replay byte-identically
- [ ] corrected accessor adapter delegates only to finalized `chordProgressionEventAt()`
- [ ] no finite-plan modulo fallback
- [ ] exact 1/2/4/8 admission and typed rejects
- [ ] one phrase identity / one H1 source / one H2 projection
- [ ] `TwoFiveOne` ordinal 8 -> intrinsic index 2
- [ ] random access `7 -> 0 -> 4 -> 7`
- [ ] pattern-address independence
- [ ] `PhraseSemanticResult` authoritative
- [ ] `ValidButEmpty` retained
- [ ] lifetime carrier present and inert
- [ ] null override preserves legacy behavior
- [ ] no heap / no retained physical N-bar array
- [ ] focused GCC/repeat/Clang/ASan/UBSan green
- [ ] Core host / SDL / Cardputer ADV / fixed DRAM / SEQTRAK MIDI-only green
- [ ] required Stage15/tonal matrix terminal green

## Decision
Only after all required workflows are terminal GREEN on one exact final SHA:

**PHRASE-P1R FINAL**

**DECISION A — PRODUCTION PHRASE EXECUTION READY**

Then HARD STOP. C2-C0/C2/R1/I2 are downstream and are not part of this replay.
