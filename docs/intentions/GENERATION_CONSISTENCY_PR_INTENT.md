# Generation Consistency PR Intent

Status: **intent only**  
Base branch: `dev_0.9_test`  
Intent branch: `agent/20260808-02-generation-consistency-intent`  
Scope: generation key semantics, materialization consistency, tests, and user-facing documentation  
Production code in this branch: **none**

## Purpose

Define the implementation contract for a narrow follow-up PR that makes generation behavior predictable across GroovePuter without introducing the next-generation Rhythm Archetype architecture.

The user-facing rule is:

```text
G = GENERATE CURRENT SCOPE
```

The follow-up PR must preserve the existing three-page causal model:

```text
GENRE -> FEEL -> GENERATION
```

and must not turn GENRE or FEEL into additional hidden owners of pattern editing.

## Verified current behavior

The current `dev_0.9_test` runtime does not have one generation path.

### GENERATION page

`G` / `Enter` materializes the selected Song row.

### SONG

- `G` generates the selected Song cell into a safe free pattern slot and assigns the new reference.
- double `G` generates Synth A, Synth B, and Drums for the current row as one logical mutation.
- `Alt+G` generates the selected Song area.
- `Ctrl+G` cycles the Song generator mode.
- `P` moves the cursor to the current playhead; it is not a generation command.

SONG generation uses `SongPatternMaterializer` and attempts an Atlas-backed recipe realization before falling back to procedural generation.

### SYNTH A / SYNTH B pattern editor

`G` calls the local synth-pattern randomization path.

The local path does use the active compiled Genre parameters and Genre behavior, including Recipe overrides, but it does not follow the same Atlas-first realization path as SONG.

### DRUMS

- `G` randomizes the current drum pattern.
- `Ctrl+G` randomizes the focused drum voice.
- `Alt+G` chaos-randomizes the full drum pattern.

The normal and focused-voice paths use active compiled Genre parameters. `Alt+G` is intentionally a destructive/chaotic operation and is not required to preserve a strict genre realization.

### Pattern selection keys

`Q..I` select pattern slots `1..8` on Synth, Drums, and Song surfaces where those keys are owned by pattern selection.

They are not generation commands and must remain independent of generation.

### Other conflicting letters

- `P` already means playhead navigation in SONG and derive-parent selection in PHRASE.
- `I` is used by the MIDI Player channel inspector.
- `G` on SYNTH SOUND is an oscillator adjustment shortcut, not pattern generation.

The future PR must not introduce `O`, `P`, or `I` as generation aliases.

## Problem statement

The current behavior is technically genre-aware but musically inconsistent.

For Atlas-backed recipes, SONG can materialize a reviewed multi-track Atlas variation while local Synth/Drum `G` uses procedural generation from compiled Genre/Recipe parameters. Therefore the same active Genre and Variant can produce materially different results depending on which page initiated generation.

This is especially visible for Atlas-backed recipes:

- Chicago Jack
- Rolling Acid
- Classic 2-Step
- Dark Skippy
- Deep Chord
- Minimal Space

The inconsistency is not that Genre is ignored. The inconsistency is that different UI surfaces realize the same Genre/Recipe through different generation pipelines.

## Decision

The follow-up implementation PR will standardize semantics and realization without rewriting the generator architecture.

### Contract 1: one primary generation key

Where a page supports material generation, unmodified `G` means:

```text
Generate material for the current editable scope.
```

No additional plain-letter generation aliases are added.

### Contract 2: scope is page-local and explicit

Expected meanings:

```text
GENERATION     G      -> selected Song row
SONG           G      -> selected cell
SONG           G,G    -> current row A+B+Drums
SONG           Alt+G  -> selected area
SYNTH PATTERN  G      -> current synth pattern
DRUMS          G      -> current drum pattern
DRUMS          Ctrl+G -> focused drum voice
DRUMS          Alt+G  -> CHAOS operation
```

Existing non-generation uses of `G` outside pattern-generation surfaces are not silently changed in this PR. In particular, SYNTH SOUND may continue to use `T/G` for oscillator adjustment, but its footer/help must make that context explicit.

### Contract 3: pattern selection remains separate

```text
Q W E R T Y U I
1 2 3 4 5 6 7 8
```

`Q..I` select slots only. They do not generate, mutate, randomize, or choose generation modes.

### Contract 4: active Genre and Recipe are mandatory generation inputs

Every normal generation path must use the current Scene Genre state as authoritative input:

- active `GenerativeMode`;
- active `GenreRecipeId`;
- compiled Genre/Recipe parameters;
- active Genre behavior;
- deterministic generation seed where the existing pipeline exposes one.

No page may construct a partially initialized `GenerativeParams` object or bypass current Recipe overrides.

### Contract 5: Atlas-backed recipes use one realization policy

For a Recipe available in `AtlasRuntime`, normal generation should use the same Atlas-first policy regardless of whether generation starts from SONG, SYNTH PATTERN, or DRUMS.

Conceptually:

```text
active Genre + active Recipe + requested variation/scope
                    |
                    v
          shared realization policy
             /             \
       Atlas exists       no Atlas
           |                 |
           v                 v
    Atlas realization   procedural fallback
```

The follow-up PR may reuse/refactor the existing SONG realization code, but must not introduce a second parallel Atlas implementation.

### Contract 6: local pattern generation preserves local edit semantics

SONG and local editors have different storage semantics and must keep them.

SONG generation remains copy-on-write:

- find a safe free pattern slot;
- materialize into it;
- assign the Song reference;
- never silently overwrite a pattern referenced elsewhere;
- fail atomically when no safe slot is available.

SYNTH PATTERN and DRUMS generation remain explicit in-place edits of the currently selected pattern. If that pattern is referenced by Song/Phrase, those references continue to observe the edited material exactly as they do for manual note/hit edits.

Unifying realization must not accidentally convert local editors to copy-on-write.

### Contract 7: multi-track SONG coherence is preserved

Double-`G` SONG row generation remains the strongest coordinated operation.

When an Atlas recipe provides Synth A, Synth B, and Drums for one variation, a single row generation must use one coherent variation identity for all requested tracks.

The implementation must not independently choose an Atlas variation per track within the same row transaction.

### Contract 8: local extraction from Atlas is deterministic

When local SYNTH or DRUM generation uses an Atlas-backed recipe, it must extract only the requested track from one well-defined variation selection policy.

The future implementation must define that variation selection once and cover it with host tests. It must not depend on unrelated UI focus or stale page-local state.

### Contract 9: CHAOS remains explicitly outside strict realization

`Alt+G` on DRUMS remains a deliberate chaos operation.

Requirements:

- label/help it as `CHAOS`, not normal `GEN`;
- continue to keep output bounded by safe data limits;
- do not claim that the result is a strict Atlas/Genre realization;
- normal `G` must remain the predictable Genre/Recipe path.

### Contract 10: FEEL ownership does not expand

This PR must not make FEEL choose musical events.

FEEL continues to own timing and velocity interpretation only. The generation consistency work must not duplicate swing/humanization ownership inside page-local randomizers.

## Proposed implementation direction

Keep the change small and centered on reuse of the existing materialization policy.

Expected touched areas in the implementation PR:

- `src/ui/pages/song_page.cpp`
- `src/ui/pages/pattern_edit_page.cpp` and its retained pattern-editor handler implementation
- `src/ui/pages/drum_sequencer_page.cpp` and its retained drum handler implementation
- `src/dsp/miniacid_engine.cpp`
- `src/dsp/song_pattern_materializer.h`
- `src/dsp/song_pattern_materializer.cpp`
- `src/dsp/atlas_runtime.h`
- `src/ui/docs/keys.md`
- `README.md`
- relevant page-aware `Alt+H` help definitions
- host regression tests under `tests/`

The exact helper/class boundary is intentionally not prescribed by this intent document. The implementation should prefer the smallest extraction that lets SONG, Synth, and Drums share realization policy without moving unrelated Scene, transport, or UI ownership.

## Non-goals

The follow-up PR must not:

- implement Rhythm Vocabulary or Rhythm Archetype architecture;
- redesign Phrase Core;
- replace `GenreManager`/Genre Scene ownership;
- add new genres or Atlas recipes;
- change FEEL ownership;
- change synth engines or drum synthesis;
- change Song pattern-address semantics;
- rewrite pattern storage or Scene persistence;
- add a new scheduler, transport, MIDI writer, or audio task;
- make local Synth/Drum editors copy-on-write;
- make `O`, `P`, or `I` generation shortcuts;
- broadly remap SYNTH SOUND controls unless required only to make help/footer text truthful.

## Required host regressions

The implementation PR must add tests that prove at least the following invariants.

### Key semantics

1. SYNTH PATTERN plain `G` reaches normal pattern generation.
2. DRUMS plain `G` reaches normal drum generation.
3. DRUMS `Ctrl+G` remains focused-voice generation.
4. DRUMS `Alt+G` remains the explicit chaos path.
5. SONG plain `G`, double `G`, `Alt+G`, and `Ctrl+G` retain their documented meanings.
6. SONG plain `P` remains cursor-to-playhead.
7. `Q..I` remain pattern selectors and do not call generation.

### Genre/Recipe realization

8. Normal Synth generation consumes the active compiled Genre/Recipe state.
9. Normal Drum generation consumes the active compiled Genre/Recipe state.
10. For an Atlas-backed recipe, local Synth generation uses Atlas-first realization before procedural fallback.
11. For an Atlas-backed recipe, local Drum generation uses Atlas-first realization before procedural fallback.
12. For a non-Atlas recipe, normal generation still falls back to the existing procedural generator.
13. No new path constructs incomplete/unsafe `GenerativeParams`.

### Coherence and mutation

14. SONG row generation chooses one variation identity for A+B+Drums.
15. Local Synth generation mutates only the selected synth pattern.
16. Local Drum generation mutates only the selected drum pattern unless `Ctrl+G` intentionally scopes to one voice.
17. Local editor generation does not allocate or assign a new Song pattern reference.
18. SONG copy-on-write safety remains unchanged.
19. A failed SONG materialization leaves both Scene data and Scene revision unchanged.
20. Successful generation increments Scene revision exactly through the established mutation path.

## Hardware assumptions

Target hardware: M5Stack Cardputer ADV / ESP32-S3 using the repository's current Arduino/M5Cardputer build profile.

No external wiring is required for this change.

Validation uses:

- built-in Cardputer keyboard;
- built-in display;
- ES8311 audio output to speaker or headphones;
- Serial at the repository's normal diagnostic baud rate;
- optional USB MIDI only to confirm no regression in normal runtime ownership.

This PR must not modify Cardputer ADV I2C, I2S, SD, PA enable, or USB MIDI hardware ownership.

## Hardware acceptance matrix

At minimum validate these three Atlas-backed recipe families because they expose different rhythmic vocabularies:

### Chicago Jack

1. Select `Chicago Jack` on GENRE.
2. Apply/materialize according to the selected policy.
3. Generate a full SONG row with double `G`.
4. Listen to A+B+Drums together.
5. Open the generated Synth A pattern and press local `G`.
6. Open the Drum pattern and press local `G`.
7. Confirm the local results remain recognizably inside the same active recipe rather than reverting to unrelated generic random material.

### Classic 2-Step

Repeat the same flow and specifically confirm that kick/snare/hat placement remains recognizably 2-step/skippy rather than collapsing to straight four-on-the-floor behavior.

### Deep Chord

Repeat the same flow and confirm the normal generator remains sparse and dub-oriented rather than becoming dense generic acid/techno material.

## Screen acceptance

The user should be able to infer the current meaning of `G` without remembering hidden global rules.

Expected visible behavior:

- SYNTH PATTERN footer/help: `G:GEN` or equivalent explicit wording.
- DRUMS footer/help: normal `G:GEN`; `Alt+G:CHAOS` visible in help.
- SONG footer/help: `G:GEN`, with double/selection behavior documented in `Alt+H`.
- SYNTH SOUND footer/help: oscillator shortcut is explicitly shown as sound control, not generation.
- no help page describes `O`, `P`, or `I` as generation aliases.

## Serial acceptance

Normal generation must not produce:

- reboot/reset;
- watchdog timeout;
- audio mutation deadlock;
- allocation failure;
- continuously growing audio underruns during ordinary repeated generation tests.

If generation diagnostics are emitted, they should make the selected Genre/Recipe and chosen realization path distinguishable enough to diagnose Atlas versus procedural fallback without enabling expensive permanent realtime logging.

## Quick validation sequence

On one firmware build:

1. Boot with a clean project or known test Scene.
2. Choose Chicago Jack and generate from SONG.
3. Generate locally on Synth A, Synth B, and Drums.
4. Repeat with Classic 2-Step.
5. Repeat with Deep Chord.
6. Verify `Q..I` only select slots.
7. Verify SONG `P` still moves to playhead.
8. Verify DRUMS `Alt+G` is clearly distinguishable as CHAOS.
9. Save, reboot, and load the Scene.
10. Confirm generated pattern contents and Song references persist as before.
11. Run ordinary playback while navigating Synth/Drums/Song and confirm no new audio instability.

## Acceptance checklist

The follow-up implementation PR is acceptable only when all of these are true:

- [ ] `G = Generate current scope` is true on every pattern-generation surface.
- [ ] No new plain `O`, `P`, or `I` generation shortcut exists.
- [ ] `Q..I` remain pattern-slot selection only.
- [ ] SONG generation remains copy-on-write and atomic.
- [ ] Local Synth/Drum generation remains explicit in-place editing.
- [ ] Active Genre and Recipe are consumed by every normal generation path.
- [ ] Atlas-backed recipes use Atlas-first realization for SONG, Synth, and Drums.
- [ ] Procedural fallback remains available for recipes without Atlas material.
- [ ] SONG row generation keeps one coherent variation across A+B+Drums.
- [ ] DRUMS `Alt+G` is documented as CHAOS, not strict genre realization.
- [ ] FEEL ownership remains timing/velocity only.
- [ ] Existing pattern matrix PAGE/BANK/SLOT behavior is unchanged.
- [ ] Scene persistence behavior is unchanged.
- [ ] Host regressions cover key semantics, Atlas-first behavior, fallback, scope, and mutation safety.
- [ ] Chicago Jack hardware smoke passes.
- [ ] Classic 2-Step hardware smoke passes.
- [ ] Deep Chord hardware smoke passes.
- [ ] Screen/footer/help text matches runtime behavior.
- [ ] No new audio underrun, watchdog, or heap regression is observed in the acceptance run.

## PR boundary

The future implementation should be one narrow consistency PR from the then-current `dev_0.9_test`.

If implementing the shared realization policy requires broad changes to Rhythm Archetype, Phrase storage, Scene codec, transport, or audio ownership, those changes are out of scope and must be split into later architecture work.

The release-oriented objective is not to make generation more sophisticated. It is to make the same active Genre/Recipe mean the same thing regardless of whether the user presses `G` in SONG, SYNTH PATTERN, or DRUMS.
