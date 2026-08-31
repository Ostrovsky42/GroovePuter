# 0.9.9-PMB-P1 — Bounded Phrase PREPARE/COMMIT

Status: **PRODUCTION IMPLEMENTATION**

## Purpose

PMB-A1 proved the P1R-capable route's per-bar physical materialization is a pure,
deterministic, random-access function and can be bounded to a single reused
`PhraseBar`-sized scratch instead of an 8-bar `material[8]` array. PMB-A2 proved
the same for the legacy (non-P1R) route: `proceduralBase` is deterministically
regenerable on demand and `PhraseGenerator::deriveBar` tolerates full in-place
aliasing. Both research checkpoints were gated on **not** touching production.

This checkpoint spends both proofs: it replaces the single 11,428 B
`PreparedPhraseArrangement::material[8]` heap allocation with a bounded
PREFLIGHT (prove every bar materializes, discard the output) + COMMIT (replay
the identical materialization, one bar at a time, persisting immediately) split,
sharing one reused `PhraseBar`-sized scratch buffer across both phases and both
routes. This directly targets the real-hardware `PHRASE PREPARE OOM` root cause
(a deterministic post-boot memory floor of free~=6,180 B / largest~=2,292 B
colliding with an 11,428 B contiguous allocation) measured before this
checkpoint began.

## Exact ancestry

```text
UI FINAL FROZEN (software)
31f42ac45183eb7bc65254a3195adf980cae328e
   |
PMB-A1 FROZEN (research)
03f633d2e431c75ec9b2f4402507b9253f785a74
   |
PMB-A2 FROZEN (research)
3e3c997fbde003ead9efc80b31ae2c92abc59859
   |
PMB-P1 base = 3e3c997fbde003ead9efc80b31ae2c92abc59859
```

Branch: `agent/20260831-02-0.9.9-pmb-p1-bounded-phrase-prepare`

## What changed

`PreparedPhraseArrangement` (`src/dsp/generated_phrase_song.h`) no longer holds
physical material. It holds a compact, trivially-copyable plan instead:

```text
before: material[8] (PhraseGenerator::PhraseBar x 8)  = 11,328 B (+100 B other fields)
after:  useP1RRoute + p1rExecution (route A)
        genre + legacy* fields       (route B, mutually exclusive with A)
                                                       =    560 B
```

Route selection (`selectStrongRhythmRoute`) is unchanged and still decides,
once per PREPARE call, which of the two mutually-exclusive shapes is populated
— `p1rExecution` (a `GroovePuterRhythm::PreparedPhraseExecution`, the frozen
P1R Stage 1 semantic plan) for the P1R-capable path, or the compact
`genre`/`legacyRecipe`/`legacyMappedMode`/`legacyFlavor`/`legacyBpm`/
`legacyParams`/`legacyBehavior`/`legacyAtlas` fields for the legacy path.
`genre` is common to both (the legacy per-bar migration step needs it).

Two new on-demand, per-bar materializer functions replace the old bulk
8-array fill:

- `GeneratedPhraseP1R::materializeOneBar(engine, execution, phraseBarOrdinal, physicalPatternAddress, scratch)`
  (`src/dsp/generated_phrase_p1r_materializer.h`) — Stage 2 (destination-independent
  pitch source) + Stage 3 (`materializePreparedPhraseBar`, already proven pure)
  for exactly one bar into a caller-owned scratch.
- `GeneratedPhraseSong::materializeLegacyBar(engine, scene, prepared, barIndex, bar)`
  (`src/dsp/generated_phrase_song.h`) — regenerates `proceduralBase` (or looks up
  the Atlas table) directly into the caller-owned scratch, migrates, and derives
  in place, per PMB-A2 Findings 3, 5 and 6. No `migratedBase` copy: `deriveBar`
  writes back over the same buffer it read from.

`prepareWithGenerationAttempt` (PREPARE) runs a PREFLIGHT loop over both routes:
for each requested bar, materialize into one reused `PhraseGenerator::PhraseBar`
scratch, check the authoritative per-bar status, and discard the physical
output. If every bar passes, PREPARE succeeds with the compact plan; if any
bar fails, PREPARE fails and **no Scene/Song/pattern state has been touched**.

`applyPreparedPersistent` (COMMIT) replays the identical materialization —
same route, same compact plan, same per-bar inputs — one reused scratch at a
time, copying each bar immediately into `scene.synthABanks`/`synthBBanks`/
`drumBanks` and setting `song.positions[...]` as it goes, exactly as PMB-A1/A2
proved preflight and replay produce byte-identical output.

`generate()` now stack-allocates the plan (`PreparedPhraseArrangement
preparedStorage{}`) instead of `new (std::nothrow) PreparedPhraseArrangement`
— the heap allocation this whole investigation started from is gone. The
`OutOfMemory` result enum/status text is kept for API completeness but is no
longer reachable from this call site.

## What did not change (explicit non-goals, held)

Display allocation, SD allocation, SMF/MIDI memory, UI FINAL pages, Phrase
Length policy, P1R harmonic policy, legacy musical policy, Song ownership,
transport/follow behavior. This is a memory-execution refactor of
`GeneratedPhraseSong`/`GeneratedPhraseP1R`'s internal staging strategy only —
byte-identical Synth A/B/Drums output for the same
(request, seed, attempt ordinal, phrase identity, barOrdinal, physical
address), proven by the regression tests below.

## Regression coverage

- P1R route (`tests/run_0_9_9_phrase_p1r_tests.sh`): focused production
  execution, deterministic repeat, ASan/UBSan gates, legacy M1 compatibility —
  unchanged, still green against the new bounded `prepare()`/
  `materializeOneBar()` API.
- E0a (`tests/run_0_9_9_e0a_tests.cpp` / `.py`): `sameMaterializedBars` proves
  a same-request PREPARE rerun materializes byte-identical physical bars on
  both routes — this is the practical, hardware-relevant determinism contract
  (PMB-A1/A2 already proved plan-state determinism implies material
  determinism; a raw `memcmp` of the whole plan struct is not used here, since
  `PreparedPhraseExecution`'s nested aggregates are built by assigning from
  locally-constructed values with explicit-field initializers, which leaves
  inter-field alignment padding as unspecified stack content — logically
  identical plans can carry different padding bytes with no material effect).
- I1 (`tests/run_0_9_9_phrase_i1_tests.sh`), D2
  (`tests/run_0_9_9_d2_tests.sh` / `test_phrase_live_arrangement_0_9_9_source.py`),
  UI FINAL (`tests/run_0_9_9_ui_final_tests.sh`): all green with zero changes
  required to UI FINAL's own suite; D2/I1's frozen source-text guards were
  updated (not weakened) to match the new call/anchor shape — the 8-bar
  physical staging array is explicitly forbidden from returning
  (`test_phrase_live_arrangement_0_9_9_source.py`).
- Phrase Core, Core regressions, SDL desktop build, Cardputer ADV build, fixed
  DRAM budget, Cardputer ADV+SEQTRAK MIDI-only build: run locally against this
  exact tree before CI, matching the workflow definitions in
  `.github/workflows/`.

PMB-A1's and PMB-A2's own probe test files/workflows
(`test_0_9_9_phrase_memory_budget_audit.*`, `test_0_9_9_phrase_pmb_a2_legacy_audit.*`)
are removed from this branch — they asserted against the pre-PMB-P1 API
(`material[8]`, the old 8-array `prepare()` signature) and are superseded by
this checkpoint. Their empirical findings remain permanently on their own
frozen branches/PRs (PMB-A1, PMB-A2).

## Hardware acceptance (post-CI-green only)

Cold boot; record free heap + largest free contiguous block together, not
either alone (`heap_caps_get_free_size`/`heap_caps_get_largest_free_block`,
`MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`) before and after each G.

**Result (2026-08-31, exact SHA `a52cf32a2cb6ec4ebea8f846574db19667fb3015`,
PR #410): PMB-P1 HARDWARE GREEN.**

1. Before first G: free~=6,180 B / largest~=2,292 B — matches the measured
   pre-fix floor exactly. PASS.
2. 1B / 2B / 4B G all succeeded end-to-end (full PREPARE->COMMIT), zero
   `PHRASE PREPARE OOM` occurrences. PASS.
3. 8B on the session's active genre/recipe was typed-rejected
   (`PHRASE LENGTH REJECTED`) — this is pre-existing, frozen P1R
   length-admissibility policy for that genre, unrelated to memory (the same
   policy would reject 8B on the pre-PMB-P1 code too, before ever reaching
   the removed allocation). The pre-fix OOM was never length-specific: the
   old `material[8]` allocation was unconditional regardless of the
   requested `bars`, so 1B/2B/4B succeeding through the exact same
   stack-allocated-plan code path is equivalent evidence to 8B for the
   claim this checkpoint makes. Accepted as sufficient without hunting for
   an 8B-admissible genre on this device.
4. Repeated G across the session (many more than 20, interleaved with page
   navigation and 40+ autosave revisions) showed no monotonic decrease in
   `largest` — it stayed flat at 2,292 B throughout. PASS.
5. Normal navigation, one more successful G (post Ctrl+Arrow destination
   move, no page revisit in between) with `largest` unchanged immediately
   after. PASS.

A real, separate UI bug was found during this testing session (PHRASE's
`destination_row_` desyncing from the SONG page cursor, and resetting to
`currentSongPosition()` whenever the PHRASE page is re-created) — this is
**not** a PMB-P1 defect and is tracked as a follow-up (planned as a larger
PHRASE workflow redesign, not a PMB-P1 fix).

## Caveats

- The bounded working set (~1,416 B `PhraseBar` scratch + ~324 B
  `PreparedPhraseExecution` for P1R, or ~1,416 B scratch + small compiled
  params for legacy) is a narrower margin than the pre-fix 11,428 B
  allocation was wide of the floor, but has not yet been hardware-validated
  at the time this document was written — that validation is the acceptance
  protocol above, not assumed by this document.
- This is an implementation-strategy change (preflight-then-replay), not a
  musical-policy change. Any future policy change to either route's
  generation logic is out of scope for this checkpoint and must not be
  bundled with it.

## Acceptance checklist

- [x] exact base = frozen PMB-A2 SHA `3e3c997f`
- [x] `material[8]` removed from `PreparedPhraseArrangement`; no equivalent
      8-bar physical array reintroduced anywhere in the generate() path
- [x] PREFLIGHT/COMMIT share one reused `PhraseBar`-sized scratch per call,
      not per route
- [x] zero Scene/Song/pattern mutation before every requested bar passes
      PREFLIGHT
- [x] P1R route: byte-identical to the frozen Stage 1/2/3 materializer,
      unchanged musical output
- [x] legacy route: byte-identical per PMB-A2 Findings 3/5/6, unchanged
      musical output
- [x] D2/I1/UI FINAL/P1R/E0a/Phrase Core/Core/SDL/Cardputer ADV/DRAM
      budget/SEQTRAK MIDI-only all green locally at this exact tree
- [x] full CI green at the exact commit SHA that will be flashed (`a52cf32a`)
- [x] hardware acceptance protocol above passed on real hardware —
      `PMB-P1 HARDWARE GREEN`, 2026-08-31

## Next checkpoint

None planned. This closes the `PHRASE PREPARE OOM` investigation started from
the real-hardware failure report, gated only on CI green followed by the
hardware acceptance protocol above.
