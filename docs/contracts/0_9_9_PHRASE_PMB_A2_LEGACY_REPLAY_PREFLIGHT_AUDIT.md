# 0.9.9-PMB-A2 — Legacy Replay/Preflight Audit

Status: **RESEARCH CHARACTERIZATION — FROZEN**

## Purpose

PMB-A1 proved that the P1R-capable route's per-bar physical materialization is a pure, deterministic, random-access function, and that a single-bar preflight/replay strategy can replace the full 8-bar `material[8]` staging buffer for that route. PMB-A1 explicitly did **not** extend that conclusion to the legacy (non-P1R) strong-rhythm route, and flagged it as a required firewall.

This checkpoint characterizes the legacy route on the same terms: reachability, failure graph, determinism, and minimum working-set carrier.

## Exact ancestry

```text
UI FINAL FROZEN (software)
31f42ac45183eb7bc65254a3195adf980cae328e
   |
PMB-A1 FROZEN (research)
03f633d2e431c75ec9b2f4402507b9253f785a74
   |
PMB-A2 base = 03f633d2e431c75ec9b2f4402507b9253f785a74
```

Branch: `research/20260831-01-0.9.9-pmb-a2-legacy-replay-preflight`

Production semantic delta: **NONE**
Production files changed: **NONE**
`src/` is byte-identical to the exact base (enforced by `tests/run_0_9_9_phrase_pmb_a2_legacy_audit_tests.sh`).

## Finding 1 — the legacy route is not reachable via live recipe selection

`GeneratedPhraseSong::prepareWithGenerationAttempt` reads `scene.genre.recipe` directly, unclamped, to decide the route via `selectStrongRhythmRoute`. The GENRE page's live recipe picker (`recipeChoicesForGenre` in `src/ui/pages/genre_page.cpp`) only ever exposes `kBaseRecipeId` and the named catalog constants up to `kDustyJazzRecipeId` (17). A host sweep of every `(mode, recipe)` pair across that entire live-reachable range (all 16 modes x recipes 0..17) confirms **zero** of them resolve to `StrongRhythmRoute::Legacy` — every one of them is either `kBaseRecipeId` (proven non-Legacy for all modes in PMB-A1) or an explicitly special-cased named route.

The legacy branch is reachable only through an **out-of-range persisted `scene.genre.recipe` value** (e.g. a stale save from before a recipe was added/renumbered, or externally-edited scene data) — `sceneRecipe()`/`normalizeRecipeForGenre()` clamp such values for *display*, but `prepareWithGenerationAttempt` does not apply the same clamp before generating. This lowers the practical risk profile of this whole route (it is not a path ordinary users exercise by choosing genres/recipes), but does not make it dead code — it must still degrade safely for stale/imported data, so this audit proceeds as planned.

## Finding 2 — sub-route survey (out-of-range recipes 18..39, all 16 modes)

`prepareWithGenerationAttempt`'s legacy branch has two sub-routes, selected by `AtlasRuntime::hasRecipe(recipe) && AtlasRuntime::variationCount(recipe) >= 3`:

```text
legacy + Atlas sub-route:        0 combinations found
legacy + procedural sub-route:  all Legacy-routed combinations
```

Every combination that reaches `StrongRhythmRoute::Legacy` in the surveyed range resolves to the **procedural** sub-route. This is consistent with the Atlas data catalog covering exactly the recipe IDs the route selector explicitly special-cases as non-Legacy — the two are correlated by construction in the current catalog. This is an observation from the surveyed range, not a proof for all 256 possible `uint8_t` recipe values, and is explicitly not asserted as permanent: `AtlasRuntime`'s catalog is append-only/generated and could grow into this range in the future, at which point this observation must be re-checked, not assumed.

## Finding 3 — the Atlas sub-route (when reachable) is already trivially bounded

`AtlasRuntime::applyRecipe(recipeId, variationIndex, synthA, synthB, drums, metadata)` (`src/dsp/atlas_runtime.cpp:65`):

- looks up a **static, generated data table** (`AtlasGenerated::Recipe`/`Pattern`);
- clears and fully repopulates its three output arguments from `pattern.events[]`;
- has zero RNG, zero mutable/static state, zero dependency on call order or prior bars.

Its only failure modes (`!recipe`, `variationIndex >= patternCount`, `!validatePattern`) are static-data-validity checks on `(recipeId, variationIndex)`, which are themselves pure functions of `(bars, barIndex)` via `roleForBar`/`atlasVariationForRole`. Confirmed by repeat-call test: calling it twice for the same `(recipe, variation)` is byte-identical. **The Atlas sub-route needs no persistent base at all** — it already operates on a single `PhraseBar`-sized buffer per bar, identically to the frozen P1R materializer's purity property (PMB-A1 Finding 3).

## Finding 4 — the procedural sub-route's per-bar body is entirely infallible

Tracing `prepareWithGenerationAttempt`'s procedural loop body:

```cpp
PhraseGenerator::PhraseBar migratedBase = proceduralBase;
applyCurrentMigration(scene, genre, 0, phraseBarOrdinal, migratedBase);
PhraseGenerator::deriveBar(migratedBase, role, prepared.request.seed, barIndex, bar);
```

- `applyCurrentMigration` (`generated_phrase_song.h:153`) discards the return value of `migrateStrongRhythmMaterial` (`(void)GroovePuterRhythm::migrateStrongRhythmMaterial(...)`) — whatever it does internally, this call site cannot abort the loop.
- `PhraseGenerator::deriveBar` (`phrase_generator.h:315`) returns `void`. It is a pure, unconditional function of `(base, role, seed, barIndex)`: it computes `randomState` deterministically from `seed`/`barIndex`, applies one of several in-place transforms selected by `role` (itself a pure lookup via `roleForBar(bars, barIndex)`), and cannot fail.

**The only failure-capable operation anywhere in the legacy per-bar loop is `AtlasRuntime::applyRecipe`, and only inside the Atlas sub-route** (Finding 2/3). The procedural sub-route, once `proceduralBase` exists, cannot fail per bar at all.

## Finding 5 — `proceduralBase` does not require persistent retention (central claim)

`proceduralBase` is built lazily, once, from a freshly constructed `GrooveboxModeManager` seeded via `prepared.request.seed` (itself a pure function of `(pageIndex, songStart, bars)` established earlier in the same call). Proven by test: **two independently constructed `GrooveboxModeManager` instances, seeded identically, produce a byte-identical `proceduralBase`** (`generatePattern` x2 + `generateDrumPattern`, all `const` methods on the manager, no shared mutable RNG). This means `proceduralBase` can be **regenerated on demand** rather than held across a preflight/commit boundary — exactly the same property PMB-A1 established for P1R's `pitchSource`.

## Finding 6 — `deriveBar` tolerates in-place aliasing

`deriveBar`'s first line is `output = base;`, after which only `output` is read or written. Proven by test: calling `deriveBar(buffer, role, seed, barIndex, buffer)` with `base` and `output` as the **same object** produces byte-identical results to calling it with two separate buffers. Combined with Finding 5, this means a bounded procedural implementation needs **exactly one** `PhraseBar`-sized buffer at a time: regenerate `proceduralBase` directly into it, migrate in place (`applyCurrentMigration` already reads/writes a single `PhraseBar`'s fields in place — this is how it is already called), then `deriveBar` in place over the same buffer.

## Finding 7 — full-array control case

The real legacy procedural `GeneratedPhraseSong::prepare()` path, called twice end-to-end with identical inputs (forced into the Legacy branch via an out-of-range recipe, per Finding 1), is byte-identical across all 8 bars. This is the baseline that Findings 4-6 must be — and are — consistent with.

## Candidate minimum legacy execution carrier

```text
current (procedural sub-route): material[8]                    = 11,328 B (+100 B overhead)

candidate: one PhraseBar-sized working buffer, reused:
             - regenerate proceduralBase directly into it (Finding 5)
             - applyCurrentMigration in place (already reads+writes in place)
             - deriveBar in place over the same buffer (Finding 6)
                                                                 =  1,416 B
         + GenerativeParams (compiled, held across bars)        =     80 B
         + GenreBehavior    (compiled, held across bars)        =     28 B
         + GenreSettings    (the raw genre, already tiny)       =     10 B
                                                        candidate peak ~= 1,534 B
```

No second `PhraseBar`-sized buffer is required for the procedural sub-route, contrary to the naive "base + output = 2,832 B" concern raised before this audit — the aliasing property (Finding 6) collapses that to one buffer. **1,534 B is comfortably under the observed largest-free-block floor (~2,292 B) with more margin than PMB-A1's P1R candidate (~1,740 B / ~552 B margin)** — this candidate has ~758 B of margin. The same "narrow margin, requires hardware validation" caveat from PMB-A1 applies: this is an observed-floor fit, not a guaranteed allocator contract.

The Atlas sub-route (Finding 3) needs only one `PhraseBar` (1,416 B) and no base at all — trivially within budget, though its live reachability in combination with Legacy is currently zero per Finding 2.

## DECISION A2

```text
THE LEGACY PROCEDURAL SUB-ROUTE CAN SHARE THE SAME BOUNDED-MEMORY
PREFLIGHT/REPLAY STRATEGY AS THE P1R ROUTE.

  1. proceduralBase requires no persistent retention -- it is
     deterministically regenerable on demand from (seed, mappedMode,
     flavor, compiled params/behavior), all of which are cheap scalars;
  2. the per-bar body (migration + deriveBar) is infallible once
     proceduralBase exists, and tolerates full in-place aliasing into
     a single reused PhraseBar-sized buffer;
  3. the Atlas sub-route is separately and more simply bounded (Finding 3),
     though currently unreachable in combination with Legacy (Finding 2)
     within the surveyed recipe range.

The legacy route's minimum working set (~1,534 B procedural / ~1,416 B
Atlas) is therefore also an implementation artifact of
GeneratedPhraseSong::generate(), not an intrinsic requirement -- the
firewall PMB-A1 raised against extending its conclusion to Legacy is
lifted for the procedural sub-route.
```

## Caveats and remaining open items

- Finding 1's reachability claim covers recipes 0..17 (every named/live-selectable constant) across all 16 modes. It does not exhaustively cover all 256 possible `uint8_t` values, though those above 17 are, by construction, exactly the "out-of-range" values this whole audit is about.
- Finding 2's "Atlas+Legacy = 0 combinations" is an observation over the surveyed range (18..39), not a proof over the full byte range, and is explicitly flagged as revisable if the Atlas catalog grows.
- This audit does not measure `migrateStrongRhythmMaterial`'s (non-frozen, legacy variant) own internal determinism directly — its return value is discarded by the caller (Finding 4), so it cannot affect the loop's control flow regardless, but a future implementation should not assume its *output* is deterministic without the same kind of explicit repeat-call proof PMB-A1 and this document apply everywhere else. Flagged for PMB-P1, not resolved here.
- No production code was changed. No claim that the candidate design is hardware-safe — margin is narrower than P1R's but still unverified on real hardware.
- No implementation of a unified bounded PREPARE/COMMIT across both routes (P1R + legacy) — that remains PMB-P1, a separate production checkpoint.

## Acceptance checklist

- [x] exact base = frozen PMB-A1 SHA `03f633d2`
- [x] zero production (`src/`) delta, enforced by CI firewall
- [x] live-recipe-range reachability swept and asserted (Finding 1)
- [x] out-of-range sub-route survey recorded, not over-claimed (Finding 2)
- [x] Atlas sub-route purity proven byte-level (Finding 3)
- [x] procedural per-bar failure graph traced to a single failure-free conclusion (Finding 4)
- [x] proceduralBase on-demand regeneration proven byte-level (Finding 5)
- [x] deriveBar in-place aliasing proven byte-level (Finding 6)
- [x] full-array control case proven byte-level (Finding 7)
- [x] candidate minimum carrier computed with explicit margin and caveats
- [ ] hardware validation (deferred to PMB-P1, gated on both A1 and A2)

## Next checkpoint

```text
PMB-P1 — BOUNDED PHRASE PREPARE/COMMIT (production implementation)
  Gated on PMB-A1 (P1R route) and PMB-A2 (legacy route), both frozen.
  Must unify both routes behind one bounded single-buffer preflight/
  replay implementation of GeneratedPhraseSong::generate(), remove
  material[8], and pass hardware memory acceptance as specified in
  PMB-A1 (cold boot x {1,2,4,8} x {P1R accepted, typed rejection} x
  {repeated G, PLAY-time, STOP-time}, tracking free/largest internal
  heap before/peak/after, with the explicit invariant that
  largest_after ~= largest_before per attempt).
```
