# 0.9.9-PMB-A1 — Phrase Memory Budget / Streaming Prepare Audit

Status: **RESEARCH CHARACTERIZATION — FROZEN**

## Purpose

Characterize the real cause of a hardware-observed `PHRASE PREPARE OOM` on Cardputer ADV, and determine whether the current full-8-bar physical staging buffer (`PreparedPhraseArrangement::material[8]`) is an intrinsic requirement of the atomic PREPARE→COMMIT contract, or an implementation artifact that can be replaced with bounded per-bar working memory.

This document changes no production code. It is the frozen record of the investigation and its conclusion, gating a future implementation checkpoint.

## Exact ancestry

```text
UI FINAL FROZEN (software)
31f42ac45183eb7bc65254a3195adf980cae328e

PMB-A1 base = 31f42ac45183eb7bc65254a3195adf980cae328e
```

Branch: `research/20260830-01-0.9.9-phrase-memory-budget-audit`

Production semantic delta: **NONE**
Production files changed: **NONE**
`src/` is byte-identical to the exact base (enforced by `tests/run_0_9_9_phrase_memory_budget_audit_tests.sh`).

## Hardware trigger that motivated this audit

On a cold-booted Cardputer ADV running the UI FINAL candidate, pressing `G` on the PHRASE product view against an empty destination row produced:

```text
[WARN][UI] Generated Phrase -> Song failed at TO=1: PHRASE PREPARE OOM
```

Serial capture of the full boot sequence (pre-existing production `[BOOT-STAGE]`/heap instrumentation, not added for this audit) showed the following heap trajectory:

```text
after M5Cardputer.begin      freeInt=141812  largest=77812
after display.begin          freeInt= 76272  largest=31732   (-65540, display framebuffer)
after AudioTask create       freeInt= 67192  largest=31732   (- 9080)
after critical DSP buffers   freeInt= 48488  largest=31732   (-18704, TempoDelay x2)
after SD mount                freeInt= 18348  largest= 7668   (-30140, largest collapses 31732->7668)
after SMF runtime init         freeInt=  8908  largest= 3060   (- 9116, largest collapses 7668->3060)
setup() complete, steady state freeInt=~6440   largest= 2292   (stable for the rest of the session)
```

The very first `G` press after boot measured `free=6180 largest=2292` — identical to the idle steady state. Repeated `G` presses (4x) produced stable, non-degrading numbers (`free` 6068-6260, `largest` constant at 2292). This rules out a fragmentation-accumulates-with-use story (would show `largest` shrinking further) and a leak story (would show `free` monotonically dropping). **The failure is deterministic and present from the first post-boot attempt**: the device's ordinary boot sequence (display + SD + SMF/MIDI init) already leaves less headroom than Phrase generation's single allocation requires, before the user does anything.

```text
required (single new(std::nothrow) allocation): 11,428 bytes contiguous
available at steady state:                       ~2,292 bytes largest contiguous block
```

This audit deliberately does not pursue reclaiming headroom from Display/SD/SMF — see **Scope boundary** below.

## Scope boundary

This audit investigates only the Phrase generation side of the budget. It does **not** authorize or recommend reducing the Display (~65.5 KB), SD (~30 KB), or SMF/MIDI (~9 KB) boot-time footprints. Making Phrase generation depend on freeing headroom elsewhere would create a fragile cross-subsystem contract ("Phrase works only if Display+SD+MIDI happen to leave >11.4 KB free") that a later unrelated change could silently break again. The goal is instead:

```text
Phrase generation working set <= the ordinary post-boot memory floor
```

Display/SD/SMF footprint reduction remains a legitimate, separate 0.9.x memory project, independent of and not a prerequisite for Phrase generation correctness.

## Finding 1 — byte decomposition (exact, compiler-verified)

```text
SynthStep            =     7
SynthPattern         =   112   (16 steps x 7)
DrumStep             =     6
DrumPattern          =    96   (16 steps x 6)
DrumPatternSet       = 1,192   (8 voices x 96 + 4 automation lanes + groove)
PhraseGenerator::PhraseBar = 1,416   (synthA 112 + synthB 112 + drums 1,192)

GeneratedPhraseSong::PreparedPhraseArrangement = 11,428
  material[8] (8 x PhraseBar)                  = 11,328   (99.1%)
  request + result + selectionTarget + baseRevision
    + songSlot + audibleSongRow + firstLocalSlot + p1r =    100   ( 0.9%)

GroovePuterRhythm::PreparedPhraseExecution = 324
GroovePuterRhythm::PhraseExecutionScratch  = 1,416
```

**The entire 11.4 KB cost is `material[8]`.** Every other field in `PreparedPhraseArrangement` is noise (100 bytes total). This is not a rough estimate; it is `static_assert`-frozen in `tests/test_0_9_9_phrase_memory_budget_audit.cpp` so any future drift in these types breaks this audit's premise loudly rather than silently.

## Finding 2 — semantic PREPARE is already separated from physical materialization

Tracing the P1R-capable route (`GeneratedPhraseP1R::prepare`, `src/dsp/generated_phrase_p1r_materializer.h`):

```text
Stage 1  GroovePuterRhythm::preparePhraseExecution(...)
         -> builds PreparedPhraseExecution (324 B) + PhraseExecutionScratch (1,416 B)
         -> touches NO physical material
         -> ALL semantic/topology/harmonic/length-admissibility failures live here
         -> can fail; nothing physical exists yet to roll back

Stage 2  prepareDestinationIndependentPitchSource(...)
         -> builds exactly ONE PhraseBar (1,416 B) via AtlasRuntime::applyRecipe
            or procedural generation
         -> can fail (AtlasRuntime::applyRecipe returning false)
         -> still touches no [8]-sized physical array

Stage 3  materializePreparedPhraseBar(execution, bar, address, ...) -- per bar
         -> the only stage that writes physical material
```

By the time Stage 3 begins, every bar's full semantic content (harmony, progression, melodic/rhythmic role, admissibility) is already frozen inside the 324-byte `PreparedPhraseExecution`. Stage 3 does not decide *what* the phrase is; it only projects already-decided semantics into a physical `SynthPattern`/`DrumPatternSet` representation.

## Finding 3 — Stage 3 is a proven pure, random-access function

`GroovePuterRhythm::materializePreparedPhraseBar` (`src/generation/migration/phrase_execution.cpp:180`):

```cpp
StrongRhythmMigrationResult materializePreparedPhraseBar(
    const PreparedPhraseExecution& prepared,
    uint8_t phraseBarOrdinal,
    int16_t physicalPatternAddress,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB);
```

Inspection confirms:

- reads only `const PreparedPhraseExecution&` (immutable, frozen after Stage 1), a bar index, and a physical address;
- writes only to the three caller-supplied output references;
- holds no internal static/mutable state;
- has no dependency on whether any other bar was materialized before or after it, or in what order.

This is a load-bearing structural fact, not an assumption: **F(execution, phraseBarOrdinal, physicalAddress) -> status + physical bar**, with no dependency on a mutable cursor, previous physical output, or materialization order.

Its failure modes (`InvalidContext`, `RealizationFailed`, `MaterializationFailed`, `CompatibilityBindingFailed`, `FeelApplyFailed`) are real and can vary per bar, since each bar's harmonic-rhythm pointer differs. Purity does not mean "cannot fail" — it means **identical inputs deterministically reproduce identical results, including identical failures**, every time.

## Finding 4 — deterministic replay, proven at the byte level (not just argued)

`tests/test_0_9_9_phrase_memory_budget_audit.cpp` proves two things against the live production code (unmodified):

```text
PROOF A — full-array repeat
  Call GeneratedPhraseP1R::prepare() twice with identical inputs.
  Result: all 8 bars byte-identical between the two calls.

PROOF B — single-bar preflight/replay vs. full-array reference
  Build PreparedPhraseExecution + pitchSource ONCE (independently of the
  full-array reference run).
  Materialize EVERY bar (0..7) individually into one reused single-bar
  scratch buffer.
  Result: every bar byte-identical to the corresponding bar produced by
  the full 8-bar reference run.
```

Proof B is the direct empirical validation of the two-pass design: a preflight pass that materializes bar N into a throwaway scratch buffer and discards it computes *exactly* the same thing a later commit pass will compute when it materializes bar N again. There is no divergence risk between preflight and commit passes — it is the same deterministic computation, run twice, which Finding 3 guarantees and Finding 4 confirms empirically.

## Finding 5 — legacy (non-P1R) route has no equivalent proof yet

`prepareWithGenerationAttempt`'s legacy branch (`src/dsp/generated_phrase_song.h`, the loop after the P1R disposition check) writes directly into `prepared.material[barIndex]` inside a `for (barIndex = 0..bars)` loop, and **can fail mid-loop**:

```cpp
if (!AtlasRuntime::applyRecipe(recipe, variation, bar.synthA, bar.synthB, bar.drums, nullptr)) {
  prepared.result.error = PhraseGenerator::PhraseError::GenerationFailed;
  return false;   // bars 0..barIndex-1 already written into prepared.material
}
```

This audit did not characterize whether this per-bar legacy failure is itself a pure function of `(recipe, variation, barIndex)` (likely, but unproven), nor whether the procedural (`deriveBar`) branch's `proceduralBase` (a single persistent `PhraseBar`, built once and reused across bars) can be reconstructed deterministically from bounded state. **No claim is made about the legacy route's compatibility with bounded staging.**

## Candidate bounded working set

```text
current:    material[8]                                     = 11,328 B  (+100 B overhead) = 11,428 B

candidate:  PreparedPhraseExecution                          =    324 B  (held for the whole prepare)
          + one PhraseBar-sized buffer, reused:
              - Stage 2 writes pitchSource into it
              - Stage 3 materializes in-place over the same buffer
              - discarded/reused per bar during preflight
              - the identical buffer holds the bar being committed
                                                               = 1,416 B
          + PhraseExecutionScratch (1,416 B), needed only
            transiently during Stage 1 and reusable as the same
            memory as the per-bar buffer once Stage 1 completes

            candidate peak (execution + one bar-sized buffer)   ~= 1,740 B
```

This is an 84.7% reduction relative to the full-array design (1,740 / 11,428), and does not require holding two `PhraseBar`-sized buffers simultaneously for the P1R route: `pitchSource` is copied into the working buffer once, then Stage 3 overwrites that same buffer's `drums`/`synthA`/`synthB` fields in place.

### Margin — explicitly not "comfortable"

```text
observed largest contiguous free block (steady state): ~2,292 B
candidate peak working set:                             ~1,740 B
observed margin:                                          ~552 B
```

This fits the *observed* hardware floor, but the margin is narrow, `largest` is an observed value from one boot/session, not a guaranteed allocator contract, and no hardware validation of the candidate has been performed. **This audit does not claim the candidate is safe on hardware** — only that it is small enough to be worth hardware-proving, unlike the current 11,428-byte requirement which is provably ~5x over the observed ceiling.

## DECISION A

```text
FULL-ARRAY PHYSICAL PHRASE STAGING IS NOT A REQUIRED ATOMICITY OWNER.

For the P1R-capable path, atomic PREPARE -> COMMIT can be preserved with
bounded one-bar physical working memory by:

  1. preparing immutable phrase execution semantics (Stage 1, 324 B);
  2. preflighting every physical bar through the authoritative
     materializer into one reusable scratch object, publishing nothing
     unless every preflight succeeds;
  3. deterministically replaying the identical materialization,
     bar by bar, during commit.

The current material[8] array is therefore an implementation artifact
of GeneratedPhraseSong::generate(), not a semantic or transactional
requirement of the P1R contract.
```

## LEGACY FIREWALL

```text
This conclusion does not authorize removal of full-array staging from
legacy (non-P1R) strong-rhythm routes.

Legacy Atlas/procedural preparation retains failure-capable per-bar work
(AtlasRuntime::applyRecipe can fail mid-loop) and a persistent
proceduralBase carrier, and requires its own equivalent deterministic
preflight/replay characterization (PMB-A2) before it may share a
bounded-memory commit path with the P1R route.
```

## Explicitly out of scope for this audit

- No production code was changed.
- No claim that the candidate design is hardware-safe (margin is narrow; requires hardware proof).
- No claim about the legacy route's compatibility with bounded staging (Finding 5; requires PMB-A2).
- No optimization of Display/SD/SMF boot-time memory footprint (see Scope boundary).
- No implementation of two-pass PREPARE/COMMIT (would be a separate production checkpoint, PMB-P1, gated on PMB-A2).

## Acceptance checklist

- [x] exact base = frozen UI FINAL SHA `31f42ac4`
- [x] zero production (`src/`) delta, enforced by CI firewall
- [x] byte decomposition frozen via `static_assert` in the host test
- [x] failure graph traced for the P1R-capable route
- [x] full-array repeat determinism proven byte-level (Proof A)
- [x] single-bar preflight/replay determinism proven byte-level against full-array reference, all 8 bars (Proof B)
- [x] candidate peak working set computed (~1,740 B) with explicit narrow-margin caveat
- [x] legacy-route firewall stated explicitly — not proven safe, not touched
- [ ] hardware validation of any bounded implementation (future PMB-P1, not this checkpoint)

## Next checkpoints (not started here)

```text
PMB-A2 — LEGACY REPLAY / PREFLIGHT AUDIT (research only)
  Can each legacy bar be reconstructed from immutable request/base state
  + barOrdinal?
  Can every failure-capable legacy operation (AtlasRuntime::applyRecipe,
  procedural generatePattern/generateDrumPattern/deriveBar) be replayed
  identically?
  Is proceduralBase (built once, reused across bars) required as
  persistent compact execution state, or can it be regenerated
  deterministically from a seed?
  What is the minimum legacy execution carrier?
  Note: at the observed largest~=2,292 B floor, two simultaneous
  PhraseBar-sized buffers (proceduralBase + working bar = 2,832 B) do
  NOT fit; the legacy design will likely need a compact descriptor/seed
  rather than a persistent physical proceduralBase, or deterministic
  on-demand regeneration.

PMB-P1 — BOUNDED PHRASE PREPARE/COMMIT (production implementation)
  Gated on PMB-A2. Must include hardware acceptance measuring, for
  cold boot x {1,2,4,8 bars} x {P1R accepted, typed rejection} x
  {repeated G, PLAY-time generation, STOP-time generation}:
    free internal (before / peak / after)
    largest internal (before / peak / after)
  with the specific invariant that largest_after ~= largest_before per
  attempt (the bounded implementation must not itself become a new
  fragmentation source).

  Proposed new hardware invariant for the eventual production contract:

    No normal user command may require a transient contiguous internal
    allocation larger than the proven post-boot largest-free-block floor.

  This would sit alongside, not replace, the existing static .dram0
  budget check (scripts/check_cardputer_dram_budget.sh) as a second gate:

    STATIC DRAM BUDGET (existing)
    +
    RUNTIME PEAK WORKING-SET BUDGET (proposed, not yet implemented)
