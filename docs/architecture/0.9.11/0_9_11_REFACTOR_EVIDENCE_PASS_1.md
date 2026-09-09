# GroovePuter 0.9.11 — Refactor Evidence Pass 1

Status: **RESEARCH EVIDENCE / NO PRODUCTION CHANGES**

Branch: `architecture/20260909-0.9.11`

This pass deliberately distinguishes structural debt from the P0 correctness findings in the two preceding evidence passes. A large type, duplicated-looking field, or embedded-oriented implementation is not enough to justify a refactor. The question is whether ownership is ambiguous, whether application policy leaks into presentation, or whether a legacy path can contradict an already-frozen musical contract.

---

## REF-001 — GenrePage is an application transaction coordinator

```text
CLASS: REFACTOR
SEVERITY: P1
STATUS: CONFIRMED
0.9.11: SHOULD, after Material correctness foundation
MEMORY IMPACT: 0 / negligible if kept as concrete operation
```

### Files / functions

- `src/ui/pages/genre_page.cpp`
- `GenrePage::applyCurrent()`
- `GenrePage::cycleApplyMode()`
- generation profile / quantized generation commit callers

### Evidence

`GenrePage::applyCurrent()` currently performs all of the following in one UI Page method:

1. resolves and normalizes genre/recipe/rhythm selection;
2. chooses whether the operation is profile-only, regenerate, or regenerate+tempo;
3. maps genre/recipe to `GrooveboxMode`;
4. creates the requested `GenreSettings` mutation;
5. compares requested vs active state;
6. resolves suggested BPM from a generation profile;
7. branches on PLAY vs stopped state;
8. chooses guarded immediate mutation vs quantized commit;
9. invokes generation/application code;
10. mutates Scene and runtime mode directly for profile-only apply;
11. marks Scene revision;
12. interprets the generation result;
13. converts the result to user feedback.

The Page therefore owns presentation, domain normalization, application policy, mutation transaction selection, runtime coordination, persistence dirtiness, and receipt formatting.

### Why this is not a P0 bug

The current operation can be locally correct. There is no demonstrated wrong-music path caused only by this concentration of responsibilities.

The architectural cost appears when the same musical operation must be triggered by another surface — Song, external control, future orchestration, or tests. The caller would either have to reproduce GenrePage semantics or invoke UI code as an application service.

### Minimal direction

Extract one concrete application operation, not a generic command bus:

```text
ApplyGenreRequest
  genre
  recipe
  rhythm selection
  regenerate?
  use suggested tempo?

applyGenre(request)
  -> prepare / validate / commit through existing generation primitives
  -> ApplyGenreReceipt
```

The UI remains responsible for navigation and rendering the receipt. It must not become the owner of quantization or Scene/runtime arbitration.

### Non-goals

- no generic event bus;
- no repository/service framework;
- no rewrite of quantized generation;
- no new heap-owned command objects.

---

## REF-002 — Genre UI exposes implementation vocabulary as musician controls

```text
CLASS: REFACTOR / UX CONTRACT
SEVERITY: P1
STATUS: CONFIRMED
0.9.11: SHOULD with application-boundary work
MEMORY IMPACT: 0
```

### Evidence

Current visible labels include:

```text
GENRE 1/2
CORRIDOR / VOCABULARY
PROFILE ONLY
MATERIALIZE
MATERIALIZE+BPM
DEPTH
```

These names describe implementation structure or generation machinery rather than decisions a musician naturally makes at the instrument.

### Musician-decision test

The underlying operations are valid, but the exposed decisions are better expressed as musical intent, for example:

```text
KEEP CURRENT MATERIAL
NEW TAKE
NEW TAKE + TEMPO
```

Exact wording remains a UI-design checkpoint; architecture only freezes the rule that implementation nouns do not become product controls merely because the Page can see them.

### Dependency

Do this after `REF-001`, otherwise a vocabulary cleanup can hide the same UI-owned orchestration behind friendlier strings.

---

## KEEP-002 — GF2-I2 FEEL ownership is already disciplined

```text
CLASS: ACCEPT / KEEP
STATUS: CONFIRMED
0.9.11: DO NOT REWRITE AS GENERIC SETTINGS CLEANUP
```

### Files / contracts

- `docs/gf2/GF2_I2_PROFILE_FEEL_CONTRACT.md`
- `src/ui/pages/feel_page.cpp`
- generation frozen-selection resolvers

### Existing owner graph

GF2-I2 already separates three different musical decisions:

```text
Scene FEEL PROFILE
  concrete or AUTO
  -> resolve once against GenerationProfile suggested FEEL
  -> frozen resolved profile shared by all roles/bars

scene.generatorParams.microTimingAmount
  -> musician FEEL intensity for NEXT generation

scene.feel.swingPct
  -> musician LIVE offbeat swing
```

Atlas swing was explicitly demoted to provenance and no longer overwrites the musician's live swing.

`FeelPage` also presents the temporal semantics explicitly:

```text
PROFILE           NEXT GEN / bounded role timing
SWING OFFBEAT     LIVE
FEEL AMOUNT       NEXT GEN
VELOCITY VAR      NEXT GEN
FEEL CYCLE        LOCAL CYCLE
```

### Architectural conclusion

The fact that FEEL-related persisted fields are split between `FeelSettings` and `GeneratorParams` is not, by itself, evidence of duplicate semantic ownership. The existing GF2 contract already gives them distinct meanings.

Do not perform a mechanical `move every FEEL-looking field into FeelSettings` refactor. First preserve the semantic contract and only change physical storage when another bounded change requires it.

---

## DEP-001 — Legacy GenreManager timing parameters overlap the modern FEEL vocabulary

```text
CLASS: DEPRECATE CANDIDATE / REFACTOR
SEVERITY: P2 currently
STATUS: PARTIAL — duplicate representation confirmed, modern caller reachability incomplete
0.9.11: SHOULD census; remove/redirect only with caller evidence
MEMORY IMPACT: 0 or lower
```

### Files

- `src/dsp/genre_manager.h/.cpp`
- `src/dsp/miniacid_engine.h`
- `src/dsp/advanced_pattern_generator.cpp`
- GF2 generation profile / FEEL pipeline

### Evidence

Legacy/parallel `GenerativeParams` and recipe override tables carry:

```text
swingAmount
microTimingAmount
```

while the frozen GF2-I2 contract defines runtime/live and profile FEEL ownership elsewhere.

A public engine accessor still exposes:

```text
MiniAcid::swing() -> genreManager().getGenerativeParams().swingAmount
```

This does not match the GF2-I2 statement that `Scene.feel.swingPct` is the single runtime swing owner.

### Why this is not yet a P0 fix

The current research pass has not demonstrated an authoritative modern playback path that calls `MiniAcid::swing()` and thereby overrides `Scene.feel.swingPct`. GF2-I2 specifically removed known Atlas overwrite paths and proved its production contract.

Therefore the existence of the alias is currently architectural debt/legacy surface, not a demonstrated audible defect.

### Required census

For each legacy timing field/API classify:

```text
current write owner
current read consumers
GF2 production consumer?
legacy-only consumer?
persisted?
UI-visible?
```

If no modern consumer remains, mark decode/compatibility only and remove the public semantic alias in a bounded checkpoint.

If a modern consumer still uses it as runtime swing, escalate that caller to FIX because it contradicts the frozen GF2-I2 single-owner contract.

---

## REF-003 — Scene is a universal persistent aggregate, but not automatically a rewrite target

```text
CLASS: REFACTOR / BOUNDARY DEBT
SEVERITY: P2
STATUS: CONFIRMED STRUCTURAL CONDITION
0.9.11: DEFER broad split; establish bounded ownership seams only when needed
MEMORY IMPACT: unknown; default target 0
```

### Evidence

`Scene` currently contains, among other state:

```text
Pattern banks
MaterialSlot descriptors
Sampler pads
Tape state
FeelSettings
GenreSettings
Drum FX
Song[2]
PhraseCore::PhraseBank
Groovebox mode / flavor
GeneratorParams
Vocal settings
custom phrases
LED settings
track volumes
```

It is simultaneously the persistence document for several domains that now evolve at different lifetimes and storage boundaries.

The Material work exposes the cost: page-local descriptors, sidecar payloads, runtime ActiveMaterial and project-level lifecycle can no longer all be represented by treating `Scene` as the one operational authority.

### Important non-conclusion

A large POD-ish persisted Scene is not inherently wrong on Cardputer. Splitting it into many heap-owned objects/services would increase complexity and potentially memory cost without fixing ownership.

### Minimal direction

Freeze this rule:

```text
Scene = persistent project snapshot / persisted musician choices
Scene != audio authority
Scene != application transaction coordinator
Scene != storage transaction implementation
```

Extract only the bounded operations/identities required by Material, Genre, Song and other future checkpoints. Do not schedule a standalone "split Scene into classes" rewrite.

---

## REF-004 — Song persistence model is Pattern-centric after Material became Pattern-or-Melody

```text
CLASS: REFACTOR, possible FIX escalation pending one entrance-path proof
SEVERITY: P1
STATUS: CONFIRMED MODEL MISMATCH
0.9.11: SHOULD after MaterialId/page transition is stable
MEMORY IMPACT: 0 expected if persisted int16 identity is retained
```

### Files / functions

- `scenes.h`
- `SongPosition::patterns[]`
- `songPatternPage()` / `songPatternBank()` / `songPatternIndexInBank()`
- `MiniAcid::applySongPositionSelection()`
- `MiniAcid::advanceSongPlayhead()`

### Evidence

Song stores four global `pattern` indices. Its resolver converts synth entries into page/bank/local Pattern selection and may request page switching. It does not resolve `MaterialKind`, prepare a Melody sidecar, or stage `ActiveMaterial` through M4.

The data value is already numerically close to the global slot identity needed by Material, so the likely migration should be semantic rather than a larger representation rewrite:

```text
old conceptual type: PatternRef
new conceptual type: MaterialRef
```

The persisted `int16_t` may remain compatible if its domain is redefined and validated carefully.

### Why severity is not yet P0 in this document

The P0 page-switch defect is independently proven. Song is definitely unable to name Pattern-vs-Melody itself, but this pass has not yet captured the complete exact `setSongMode(true)` entrance function proving whether Song can remain in an already-active runtime Melody source.

Do not inflate severity without that final caller trace.

---

## DEP-002 — MORPH schema is legacy/zombie-prone, but not yet proven fully dead

```text
CLASS: DEPRECATE CANDIDATE
SEVERITY: P2
STATUS: PARTIAL
0.9.11: census, then decode-only if no authoritative consumer remains
```

### Evidence

`GenreSettings` persists:

```text
morphTarget
morphAmount
```

Current `GenrePage::pendingSettings()` and `applyCurrent()` explicitly force both to zero on modern UI application.

However legacy `GenreManager` still contains morph-oriented API/schema. This means the fields cannot yet be called dead solely from the UI behavior.

### Required resolution

Trace every non-test read/write. If modern application/generation no longer consumes them:

```text
read legacy documents if compatibility requires it
never expose as musician control
never write non-zero in new saves
remove from authoritative domain vocabulary
```

This matches the musician-decision rule: MORPH is not reintroduced merely because the old schema can represent it.

---

# Revised KEEP / FIX / REFACTOR boundary

```text
KEEP
- M3 ActiveMaterial publication
- M4 NEXT prepare/stage/activate
- runtime note lifetime owner
- MelodyStore explicit ABI
- GF2-I2 FEEL arbitration and single live swing owner
- bounded/static embedded storage patterns

FIX FIRST
- canonical page-aware MaterialId
- one canonical Pattern -> Melody operation
- page transition rebinds active Material
- empty-page Material descriptors reset
- Save As copies full material graph
- explicit unresolved material semantics

REFACTOR AFTER CORRECTNESS
- GenrePage application boundary
- Song PatternRef -> MaterialRef semantics
- implementation vocabulary in Genre UI
- legacy GenreManager timing/morph surfaces after caller census

DEFER / TARGETED ONLY
- broad Scene decomposition
- physical relocation of FEEL fields that already have correct semantic ownership
```

---

# Proposed 0.9.11 checkpoint shape after Refactor Pass 1

```text
0.9.11-A1  Material Identity
0.9.11-A2  Canonical Pattern -> Melody Operation
0.9.11-A3  Atomic Page Material Transition
0.9.11-A4  Empty Page + Explicit Resolution Semantics
0.9.11-A5  Project Material Lifecycle / Save As

0.9.11-B1  Song MaterialRef Semantics

0.9.11-C1  Genre Application Operation
0.9.11-C2  Musician-level Genre UI vocabulary

0.9.11-D1  Legacy GenreManager / MORPH retirement census

0.9.11-E   Scene boundary cleanup only where A-D created proven seams
```

The sequence is intentional. UI and vocabulary refactors are not allowed to obscure or precede the Material correctness work.
