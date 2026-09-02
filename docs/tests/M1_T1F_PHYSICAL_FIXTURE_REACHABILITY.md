# M1-T1F physical fixture reachability

Tests-only characterization from `47801d9c0b8efe92a502286ed10ebcce3eb43785`.

## Decision C — test / domain observability gap

The exact base exposes 16 raw mode values and 18 raw recipe values: 288 mode/recipe pairs. `selectStrongRhythmRoute()` returns non-Legacy for all 288, yielding 73,728 route-valid rows across pattern addresses `0..255`. `resolveGenerationComposition()` can return Applied through fallback profile resolution. Neither is an observable admission contract for independently admitted production profiles.

No existing production-owned predicate or catalog derives the previously asserted 198 authoritative profile combinations. This is not a claim that 198 is globally wrong; it is not reproducible from an observable production owner on this base.

T4 RestHeavy and T5 DriftPhrase are `OBSERVABILITY_BLOCKED`. A T6 production-executable candidate was observed but is not frozen as authoritative: Electro (3), recipe 0, address 7, Melodic Synth B, DelayedAnswer, CallResponse, phraseBars 4, semantic onsets `{6,14}`. Its authoritative-profile membership is unproven.

No production policy, M1 wiring, M1L, or source change is made. Hard stop.
