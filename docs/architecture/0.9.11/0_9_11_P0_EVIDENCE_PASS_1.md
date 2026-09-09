# GroovePuter 0.9.11 — P0 Architecture Evidence Pass 1

Status: **RESEARCH EVIDENCE / NO PRODUCTION CHANGES**

Branch: `architecture/20260909-0.9.11`
Provisional 0.9.10 parent: `fcd0d77da5ed6ef38419547477ab26e77ec6ff26`

This pass converts the first architecture-census hypotheses into source-backed findings. It deliberately separates proven correctness defects from structural debt. No implementation is authorized by this document.

---

## EVID-001 — Melody persistent identity aliases across pages

```text
CLASS: FIX
SEVERITY: P0
STATUS: CONFIRMED
0.9.11: MUST
MEMORY IMPACT: 0 / negligible expected
```

### Files / functions

- `src/state/melody_promotion.h`
- `MelodyPromotion::slotPath()`
- `MelodyPromotion::finalPath()`
- `MelodyPromotion::tempPath()`
- `MelodyPromotion::promoteResident()`
- `tests/test_melody_promotion.cpp`

### Evidence

Melody sidecar identity is physically addressed as:

```text
/projects/<project>/melody/v<voice>_s<resident-slot>.gpml
```

`promoteResident()` accepts a resident slot, and resident material slots span only the currently loaded 0..15 page-local range. The path contains no page or global material index.

The global arrangement space is paged: the same local slot number exists independently on every page. Therefore two distinct materials can resolve to one physical GPML path.

### Concrete failure path

```text
project P
page 0 / Synth A / local slot 5 -> promote Melody X
page 1 / Synth A / local slot 5 -> promote Melody Y

both payloads resolve to:
/projects/P/melody/v0_s05.gpml

Y replaces/aliases X
```

Returning to page 0 leaves its descriptor saying Melody while the sidecar now contains Y.

### Invariant violated

```text
Distinct MaterialId values must never address the same persistent payload
unless explicit content deduplication is part of the design.
```

### User / musical consequence

A user can edit two different musical ideas on two pages and have one silently replace the other. The failure is not cosmetic and cannot be recovered from the Pattern bytes after one-way promotion/editing.

### Existing test gap

`tests/test_melody_promotion.cpp` verifies separation across:

- voices;
- local slots;
- projects.

It does not include the page/global dimension. This is the missing adversarial case.

### Minimal direction

Freeze one canonical persistent `MaterialId`. The lowest-risk candidate is conceptually:

```text
project namespace + voice + globalSlot
```

with page/local coordinates derived from `globalSlot`, rather than inventing an unrelated second identity space.

Do not redesign runtime ACTIVE/NEXT for this fix.

### RED characterization required for implementation checkpoint

Promote two different payloads into the same voice/local slot on two different pages and prove that both remain independently loadable.

---

## EVID-002 — Empty-page initialization retains stale MaterialKind

```text
CLASS: FIX
SEVERITY: P0
STATUS: CONFIRMED
0.9.11: MUST
MEMORY IMPACT: 0
```

### Files / functions

- `src/audio/pattern_paging.cpp`
- `PatternPagingService::initializeEmptyPage(Scene&)`

### Evidence

`initializeEmptyPage()` resets:

```text
scene.synthABanks
scene.synthBBanks
scene.drumBanks
```

but does not reset:

```text
scene.materialSlots
```

The page format otherwise treats `materialSlots` as page-owned state: save writes them with the page and load restores them with the page.

### Concrete failure path

```text
resident page:
Synth A / slot 5 = MELODY

initializeEmptyPage(scene)

Pattern banks become empty/default
MaterialKind for slot 5 remains MELODY
```

The new empty page can therefore inherit a descriptor belonging to the previously resident material.

### Invariant violated

Initializing a page must initialize every piece of state whose lifetime and persistence belong to that page.

### User / musical consequence

A nominally empty/new page may try to resolve a Melody sidecar that belongs to previous page state instead of behaving as an empty Pattern page.

### Minimal direction

Reset every resident material descriptor to the canonical empty-page state (`Pattern`) in the same operation that resets its Pattern payloads.

### RED characterization required

Seed one or all descriptors as Melody, invoke `initializeEmptyPage()`, then assert all resident descriptors are Pattern while unrelated project state is untouched.

---

## EVID-003 — Save As copies Melody descriptors but not Melody payloads

```text
CLASS: FIX
SEVERITY: P0
STATUS: CONFIRMED
0.9.11: MUST
MEMORY IMPACT: 0 / small transient copy buffer already exists
```

### Files / functions

- `scene_storage_cardputer.cpp`
- `SceneStorageCardputer::setCurrentSceneName()`
- `src/audio/pattern_paging.cpp`
- `PatternPagingService::copyProjectPages()`
- `PatternPagingService::savePage()` / `loadPage()`
- `src/state/melody_promotion.h`

### Evidence

For a new target name, `SceneStorageCardputer::setCurrentSceneName()` uses `PatternPagingService::copyProjectPages(previous, normalized)` as the project-copy step.

`copyProjectPages()` copies page `.gpp` files and backups only.

The current page format includes `materialSlots`, so a copied page can preserve `MaterialKind::Melody` descriptors.

Melody payloads live separately under:

```text
/projects/<project>/melody/*.gpml
```

and `copyProjectPages()` does not copy that namespace.

### Concrete failure path

```text
Project A
  page N / slot X descriptor = MELODY
  payload exists at /projects/A/melody/...

Save As -> Project B

.gpp page copied A -> B
therefore B descriptor remains MELODY

GPML sidecar is not copied
therefore /projects/B/melody/... is missing
```

The copied project now contains a valid-looking descriptor pointing at material that does not exist in its namespace.

### Invariant violated

A project copy must preserve the complete reachable persistent object graph of that project, or explicitly refuse the copy.

### User / musical consequence

`Save As` can produce a project that appears to have copied correctly but loses promoted/edited Melody material. This is user-data integrity, not cleanup debt.

### Minimal direction

Project lifecycle must own both page payloads/descriptors and Melody sidecars under one project-copy contract. Do not make SceneStorage know individual GPML filenames; give persistence one bounded project-material copy operation or equivalent explicit composition.

### RED characterization required

```text
A: promote Melody, save page
Save As A -> B
switch/load B
assert Melody descriptor + independently copied payload both exist and decode
mutate/delete A
assert B remains intact
```

### Related but not yet fully classified

Project clear/delete currently operates primarily on page files. Orphan GPML cleanup and delete semantics require a separate lifecycle trace. The proven defect here is specifically `Save As` producing descriptor-without-payload.

---

## EVID-004 — NotResident / Invalid material reads collapse to Pattern

```text
CLASS: REFACTOR currently; escalate to FIX if reachable wrong-source publication is proven
SEVERITY: P1 pending caller trace
STATUS: API BEHAVIOR CONFIRMED / PLAYBACK FAILURE NOT YET PROVEN
0.9.11: SHOULD, MUST if caller trace reaches publication
MEMORY IMPACT: 0 / negligible
```

### Files / functions

- `src/state/material_slot_access.h`
- `GroovePuterMaterial::residentKind()`
- `GroovePuterMaterial::materialKind()`
- `tests/test_material_slot_identity.cpp`

### Evidence

The current API intentionally returns `MaterialKind::Pattern` when:

- resident voice/slot is out of range;
- a global slot does not belong to the active page.

The M1 test suite explicitly codifies the out-of-range Pattern fallback.

This conflates:

```text
Pattern
Invalid
NotResident
Unresolved
```

### Important distinction

This is not yet proven to be an audible P0 defect because M3 establishes that the audio thread does not resolve Scene/material descriptors. It only reads a previously published `ActiveMaterial`.

Therefore the remaining question is control-side reachability:

```text
Can Song/page/application resolution call this fail-open API
before the required page is resident and then publish Pattern?
```

Until that caller path is proven, classify this as a bad semantic API / P1 refactor rather than inflating it to P0.

### Expected invariant

```text
UNKNOWN      != PATTERN
NOT RESIDENT != PATTERN
INVALID      != PATTERN
LOAD FAILED  != PATTERN
```

Legacy persisted data that predates MaterialKind is a separate compatibility case and may explicitly decode as Pattern.

### Minimal direction

Use a tiny explicit result (`bool + out`, optional, or small enum). No generic error framework.

---

## KEEP-001 — M3/M4 runtime authority boundary remains healthy

```text
CLASS: ACCEPT / KEEP
STATUS: CONFIRMED BY CURRENT CONTRACT TESTS
0.9.11: DO NOT REDESIGN WITHOUT CONTRADICTORY EVIDENCE
```

### Evidence

`tests/test_m3_active_material_authority.cpp` establishes:

- Scene is not audio authority;
- material selection changes audio only when explicitly published;
- slot and kind travel together;
- voices are independent.

`tests/test_m4_next_material.cpp` establishes:

- preparation is control-side;
- staging does not activate;
- activation moves slot + kind + prepared melody together;
- failed Melody preparation is refused;
- activation has no filesystem I/O/allocation;
- pending buffers are fixed-lifetime allocations rather than churn.

### Architectural conclusion

The first confirmed 0.9.11 defects are upstream identity/persistence/lifecycle defects. They are not evidence that ACTIVE/NEXT or the sequencer needs a rewrite.

Preserve this boundary while repairing the producers that feed it.

---

# Updated dependency order after Pass 1

```text
[0] KEEP runtime boundary frozen
      ACTIVE/NEXT + lifetime owner

[1] Canonical Material identity
      EVID-001
          |
          +------> project-sidecar lifecycle
          |          EVID-003
          |
          +------> control-side resolution semantics
                     EVID-004
                          |
                          +------> Song -> MaterialRef census

[2] Page initialization correctness
      EVID-002
      (independent implementation, but belongs in same correctness foundation)

[3] Promotion durability contract
      payload vs persisted descriptor/page transaction

[4] Domain vocabulary
      Material / Pattern / Melody / Phrase / Song

[5] Genre / Feel / Generator authority

[6] UI application boundary
```

The key change from the initial census is that **project lifecycle can no longer wait as generic persistence cleanup**. Save As already creates a broken project state, so it belongs immediately after canonical Material identity.

---

# Proposed first implementation boundary — NOT YET AUTHORIZED

A future `0.9.11-A` checkpoint should probably be narrower than the initial draft:

```text
0.9.11-A1  Canonical MaterialId + cross-page storage identity
0.9.11-A2  Page descriptor initialization correctness
0.9.11-A3  Project copy / Save As complete material graph
0.9.11-A4  Explicit unresolved-material result + Song caller trace
```

Do not combine vocabulary renames, Scene decomposition, GF2 cleanup, UI redesign, or runtime redesign into A1-A4.

---

# Research state after Pass 1

```text
CONFIRMED FIX / P0:
- Melody sidecar cross-page identity collision
- stale MaterialKind after empty-page initialization
- Save As copies Melody descriptor without Melody payload

CONFIRMED STRUCTURAL DEBT / caller-dependent:
- NotResident/Invalid reads masquerade as Pattern
- Song remains Pattern-centric while slots can be Melody

CONFIRMED KEEP:
- M3 explicit ActiveMaterial authority
- M4 control-side prepare / boundary activation
- fixed-lifetime pending buffers

NEXT RESEARCH:
- exact Song/page control-side resolver path into Active/NEXT
- promotion durability across power loss/page save
- project clear/delete/orphan sidecar behavior
- canonical project-name namespace across Scene/Pattern/Melody stores
```
