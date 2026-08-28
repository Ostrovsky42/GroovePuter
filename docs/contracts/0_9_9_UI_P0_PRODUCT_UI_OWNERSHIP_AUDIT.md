# 0.9.9-UI-P0 — PRODUCT UI OWNERSHIP AUDIT + PHRASE ENGINE INTEGRATION PLAN

Status: RESEARCH / DESIGN ONLY  
Production base: `agent/20260828-05-0.9.9-phrase-i1-end-to-end-synth-phrase` @ `fb30bbb93f739d8b3e4514ae931cbc73fc8eeb09`  
Historical GF2 evidence only: `agent/20260826-01-0.9.9-gf2-ui-scaffold` @ `848cb53d248a8040b1a14f90588add2272c3c593`

## Decision

UI-P0 does not change `src/**`.

The exact current authoritative production checkpoint is frozen I1 `fb30bbb9...`. Corrected C2-C0 is only an open research replay (`#405`, based exactly on I1); corrected C2, corrected R1, and hardware lifetime acceptance are not frozen on this ancestry. Therefore:

- follow-playback / STOP-in-place may rely on I1 now;
- product Phrase Length may be designed, but its request ownership ambiguity must be fixed before exposing it as a canonical editable field;
- persistent semantic Phrase identity / progression / harmonic timeline cannot yet be presented honestly after I1 publication because I1 does not retain the full `PhraseSemanticResult` as Scene/runtime UI state;
- cross-bar lifetime visualization must wait for corrected C2 + R1 + hardware acceptance;
- GF2 is naming/ownership evidence only, never technical ancestry.

## 1. Current product information architecture

`src/ui/workflow_mode.h` freezes the current workspace topology:

```text
PERFORM
  MIDI KEYBOARD
  MIDI PLAYER

GENERATE
  GENRE
  FEEL

HUB
  OVERVIEW
  SYNTH A
  SYNTH B
  DRUMS

SONG
  SONG
  PHRASE CORE

SETTINGS
  PROJECT / SETUP
```

Legacy `Generation` and `Texture` page IDs normalize to FEEL. Legacy standalone Synth parameter pages normalize to Synth A/B. Therefore the useful GF2 decision `GENERATE = GENRE -> FEEL` already matches current production architecture.

Synth A/B are symmetric `SynthSequencerPage` instances. Each owns local tabs:

```text
NOTES
KNOBS
MORE
```

NOTES is `PatternEditPage`; it edits the physical `Scene` pattern selected by the engine.

The current PHRASE page is not the new P1R semantic phrase view. It is a mixed surface:

```text
legacy persistent PhraseCore workspace
  slots A/B/C/D
  capture / derive / insert / replace
  PhraseCore::phraseId
  PhraseCore::lengthBars

plus

I1 GeneratedPhraseSong command
  G
  requested bars = PhrasePage::capture_length_
  destination Song row = PhrasePage::destination_row_
```

Those two phrase concepts must not be silently conflated.

## 2. Physical pattern identity and capacity

Current I1 storage is larger than the minimum eight-page hardware acceptance scenario:

```text
kBankCount       = 2
patterns / bank  = 8
patterns / page  = 16
kMaxPages        = 16
```

Canonical physical Song reference is encoded by:

```text
page + bank + slot
    -> songPatternFromPageBankIndex(...)
    -> global pattern reference
```

The product requirement that pages 1..8 all behave as real editable pages is valid as a minimum hardware acceptance set, but UI work must not reduce the existing 16-page storage model.

## 3. Exact I1 transport -> Song row -> physical pattern -> NOTES path

Authoritative chain:

```text
MiniAcid transport
  ↓
SceneManager Song position
  ↓
MiniAcid::applySongPositionSelection()
  ↓
sceneManager_.songPatternAtSlot(songPlaybackSlot_, row, track)
  ↓
songPatternPage / songPatternBank / songPatternIndexInBank
  ↓
SceneManager current Synth A/B bank + pattern selection
  ↓
SynthSequencerPage::tick()
  ↓
PatternEditPage::syncSongPatternContext()
  ↓
NOTES pattern_row_cursor_ / bank_index_ mirror engine selection
```

`applySongPositionSelection()` is the single playback-to-physical-selection mapper. UI must not add a second mapper, transport cursor, modulo-derived phrase cursor, or page-local playback index.

During playback, page switching is also driven from the same resolved physical Song reference via `songPatternPage(firstGlobal)` and `requestPageSwitch(tPage)`.

### Required product behavior

When Synth A NOTES is visible:

```text
Song row 0 -> A physical pattern X -> NOTES shows X
Song row 1 -> A physical pattern Y -> NOTES shows Y
Song row 2 -> A physical pattern Z -> NOTES shows Z
```

Synth B must behave identically for its own track.

Auto-follow is local to a playback-aware editor. Navigating to GENRE / FEEL / DRUMS / SETTINGS must never force the application back to a Synth page and must never change transport ownership.

## 4. Exact STOP-in-place owner

I1 already owns the desired transition.

`MiniAcid::stop()`:

1. settles pending Song/Phrase activation when required;
2. publishes all-notes-off and releases runtime voices;
3. sets `playing = false`;
4. in Song mode writes `SceneManager.songPosition = songPlayheadPosition_`;
5. does **not** restore the pre-PLAY Synth bank/pattern selection.

Because `applySongPositionSelection()` already made the current sounding physical pattern the SceneManager current selection while playing, and `PatternEditPage::syncSongPatternContext()` mirrors that selection, STOP naturally freezes the last sounding physical pattern as the immediate edit target.

This is the correct product contract:

```text
PLAY
  -> follow sounding material
STOP
  -> keep current Song row
  -> keep current physical pattern selection
  -> edit exactly what was heard
```

UI-P1 should characterize and protect this behavior, not invent a new stop state.

## 5. P1R/I1 phrase ownership that UI may consume

Frozen production semantics:

```text
requested phrase length: 1 / 2 / 4 / 8
PhraseLengthRequestResult: Accepted / Rejected
one phraseGenerationIdentity
explicit phraseBarOrdinal
PhraseSemanticResult
one phrase-global ChordProgressionSource = WHAT
PhraseHarmonicTimeline = bar-local HarmonicRhythm WHEN + phrase-global ordinals
physicalPatternAddress = destination only, never musical identity
```

`PreparedPhraseExecution` contains no retained physical address cursor. `materializePreparedPhraseBar(...)` receives both:

```text
phraseBarOrdinal       = musical coordinate
physicalPatternAddress = storage destination coordinate
```

I1 materializes bar `0..N-1` into contiguous safe physical slots, then writes the exact resulting global pattern reference into each corresponding Song row for Synth A, Synth B and Drums.

That mapping is authoritative only for the generated arrangement being prepared. UI must not later infer semantic bar by `patternAddress % phraseBars` or `Song row % phraseBars`.

## 6. Important I1 exposure gap

I1's P1R adapter keeps the full semantic execution caller-owned during PREPARE:

```text
PreparedPhraseExecution
  -> PhraseSemanticResult
  -> harmonic timeline
  -> semantic bars
```

After materialization/publication, `GeneratedPhraseSong::PreparedPhraseArrangement` retains only compact `GeneratedPhraseP1R::PreparationEvidence`, including:

```text
phraseGenerationIdentity
progression id
harmonicEventPositions
execution/materialization status
```

The persistent Scene/Song publication stores physical patterns + Song references. It does not persist the full `PhraseSemanticResult` or an authoritative generated-phrase-to-Song semantic bar mapping.

Therefore UI-P3 must not create a page-local semantic cache merely to display:

```text
IDENTITY
PROGRESSION
HARMONIC TIMELINE
CURRENT SEMANTIC BAR
```

A later checkpoint needs a narrow production-owned read/exposure contract if these values must remain inspectable after commit/reboot. This is an ownership blocker, not a reason to modify frozen musical policy.

## 7. Phrase Length vs Feel Cycle

These are separate concepts and must remain separately named.

### Feel Cycle

Current owner:

```text
Scene::feel.patternBars
```

Current FEEL page label on I1 is `REPEATS`; GF2 correctly demonstrated the product label `FEEL CYCLE`.

FEEL page writes only `scene.feel.patternBars` for this field. It is Scene-persistent and affects local cycle/repeat behavior.

I1 phrase publication deliberately uses `forceSingleBarRows = true` and normalizes `scene.feel.patternBars = 1` after writing generated bars so each Song row addresses one physical phrase bar. That is a publication-side transport normalization; it does not make `patternBars` the phrase-length owner.

### Product Phrase Length

Policy authority exists:

```text
resolveGenerationCompositionForPhraseBars(...)
PhraseLengthRequestResult
  requestedPhraseBars
  effectivePhraseBars
  status Accepted / Rejected
```

But the current editable request value does **not** have an acceptable product owner. `PhrasePage::capture_length_` is page-local state shared by:

- legacy PhraseCore capture length;
- I1 `GeneratedPhraseSong::generate(..., capture_length_, ...)`.

Therefore `capture_length_` must not simply be relabeled `PHRASE LENGTH`.

UI-P2 needs a narrow non-policy request-state owner outside page-local UI (analogous in spirit to generation depth request state), with both the Phrase Length row and any shortcut reading/writing that one request value. The frozen admissibility policy remains in generation code and may reject the request without coercion.

## 8. Generation outcomes

I1 already distinguishes a typed phrase-length rejection from execution failure:

```text
PhraseExecutionStatus::Rejected
  -> "PHRASE LENGTH REJECTED"

other P1R execution failure
  -> "PHRASE EXEC FAILED"
```

Product UI should normalize outcomes into three presentation classes:

```text
ACCEPTED
  CommittedNow
  PendingNextBar

TYPED REJECTION
  PhraseExecutionStatus::Rejected
  preserve requested value; do not silently choose another length

EXECUTION FAILURE
  Busy
  TargetChanged
  OutOfMemory
  InvalidContext / progression / projection / materialization failures
```

Typed rejection is a valid domain result, not a crash.

## 9. Current generation/publication boundaries

There are currently three context-specific `G` behaviors. They are real existing owners, not one unified product command:

1. **GENRE / FEEL generation** — `regenerateWithQuantizedCommit(...)`; owns quantized current-target generation and canonical generation Undo.
2. **PHRASE G** — `GeneratedPhraseSong::generate(...)`; owns multi-bar PREPARE, safe physical allocation, Song publication, quantized live activation and generated-phrase Undo.
3. **Synth NOTES G while stopped** — local physical pattern generation through `ModeManager::generatePattern(...)` followed by Pattern Undo.

UI-P0 does not delete any of them. UI-P5 must make their user-facing scopes explicit and ensure a product `APPLY / GENERATE` surface calls the highest correct production boundary instead of directly writing Song/Scene or calling low-level musical generators from a new screen.

## 10. Undo and publication ownership

Current production has bounded canonical Undo owners:

- GENRE quantized generation -> `UndoKind::Generation`;
- generated Phrase -> `GeneratedPhraseSong` + `UndoKind::Generation`;
- PhraseCore workspace mutation -> `UndoKind::Phrase`;
- Song arrangement mutation -> `UndoKind::Song` + live-arrangement activation lease;
- physical Synth NOTES edit -> `UndoKind::Pattern`.

New UI must call these boundaries rather than mutate their backing storage independently.

## 11. Ownership table

| UI field | Read owner | Write owner | Persistence | Playback effect | Status |
|---|---|---|---|---|---|
| GENRE | `Scene.genre.generativeMode` | `GenrePage` pending request -> existing apply/generation boundary | Scene | next generation; profile/mode apply | AUTHORITATIVE EDITABLE |
| RECIPE | `Scene.genre.recipe` | same GENRE apply boundary | Scene | next generation | AUTHORITATIVE EDITABLE |
| RHYTHM | `Scene.genre.rhythmSelectionMode/archetypeId` | same GENRE apply boundary | Scene | next generation composition | AUTHORITATIVE EDITABLE |
| DEPTH | `GroovePuterState::currentGenerationLevel()` | `set/cycleGenerationLevel()` | session/runtime only | next generation request | AUTHORITATIVE EDITABLE |
| FEEL PROFILE | `Scene.feel.timingProfile` | FEEL page under audio guard | Scene | live timing + next generation | AUTHORITATIVE EDITABLE |
| SWING | `Scene.feel.swingPct` | FEEL page under audio guard | Scene | live offbeat timing | AUTHORITATIVE EDITABLE |
| FEEL AMOUNT | `Scene.generatorParams.microTimingAmount` | FEEL page | Scene | next generation timing realization | AUTHORITATIVE EDITABLE |
| VELOCITY VAR | `Scene.generatorParams.velocityRange` | FEEL page | Scene | next generation velocity realization | AUTHORITATIVE EDITABLE |
| FEEL CYCLE | `Scene.feel.patternBars` | FEEL page | Scene | local cycle window; I1 generated phrase normalizes to 1-row bars | AUTHORITATIVE EDITABLE |
| PHRASE LENGTH | frozen resolver/result owns admission; current requested value is `PhrasePage::capture_length_` | **no acceptable product request owner yet** | none for request | determines requested 1/2/4/8 multi-bar generation | NOT READY |
| CURRENT PHRASE BAR | `PhraseSemanticResult.bars[].temporal.phraseBarOrdinal` during PREPARE | none | not retained by I1 | semantic view only | NOT READY after publication |
| PHRASE IDENTITY | P1R `phraseGenerationIdentity`; **not** `PhraseCore::phraseId` | generation only | not retained as authoritative Scene semantic identity | musical generation identity | NOT READY after publication |
| PROGRESSION | `PreparedPhraseExecution.progressionSource`; compact id in P1R evidence | generation policy only | not retained as full semantic state | phrase-global harmonic WHAT | NOT READY persistent view |
| HARMONIC TIMELINE | `PhraseSemanticResult.harmonicTimeline` | generation policy only | not retained after I1 commit | phrase-global WHEN projection | NOT READY persistent view |
| ACTIVITY | no stable production cadence owner found | none | none | undefined | NOT READY |
| FOLLOW PLAYBACK | I1 Song mode + engine physical selection; currently implicit/active | transport owns selection; UI only mirrors | none | NOTES follows sounding pattern | AUTHORITATIVE READ-ONLY behavior; editable toggle NOT READY |
| CURRENT PHYSICAL PATTERN | SceneManager current Synth bank/pattern set by `applySongPositionSelection()` | transport in Song mode; physical editor when stopped/pattern mode | Scene pattern content; selection runtime | exact sounding/editing material | DERIVED VIEW |
| APPLY / GENERATE | GENRE quantized command; PHRASE `GeneratedPhraseSong`; local NOTES generator | existing context-specific command boundary | corresponding Undo/Scene rules | context specific | AUTHORITATIVE EDITABLE today; UX scope cleanup required |

## 12. GF2 decisions safe to re-apply semantically

Safe evidence:

- `GENERATE = GENRE -> FEEL` — already true in I1.
- Rename FEEL `REPEATS` -> `FEEL CYCLE`; keep owner exactly `scene.feel.patternBars`.
- Add GENRE `DEPTH` row that reads/writes only `GroovePuterState::currentGenerationLevel()`; plain `P` and focused Left/Right must call the same owner.
- Product language: GENRE / RECIPE / RHYTHM / DEPTH are distinct concepts.
- No Activity control without a real cadence owner.

Do not cherry-pick GF2 wholesale. In particular, do not import stale page layout, stale publication assumptions, or old ancestry.

## 13. Legacy / duplicated / ambiguous state to clean up carefully

1. `PhrasePage::capture_length_` mixes PhraseCore capture length with generated phrase request length. This is the highest-priority ownership ambiguity.
2. `PhrasePage::preview_bar_` is a PhraseCore preview cursor, not the transport's semantic phrase bar.
3. `PhraseCore::phraseId` and P1R `phraseGenerationIdentity` are different identity domains despite similar labels.
4. GENRE keeps local pending UI staging (`genre_index_`, recipe/rhythm selection) before explicit APPLY. This is acceptable edit-buffer state, not musical authority, but must resync on authoritative external changes.
5. Pattern NOTES keeps local cursors; in Song mode `syncSongPatternContext()` makes physical bank/pattern selection a derived mirror. Step/edit cursor may remain editor-owned.
6. `G` has three context-specific meanings. Do not add a fourth hidden meaning.

## 14. What must wait for corrected C2/R1

Do not expose cross-bar lifetime until the corrected production line is frozen and hardware accepted.

Blocked UI:

```text
held-across-bar melodic arc
continuation/overlap indicator
runtime cross-bar note state
barrier-release visualization
```

The UI must never infer lifetime from step 15, gate length, equal pitch, or neighboring note data.

Also do not present unresolved DROP/DISPLACE execution grammar or quarantined harmonic bootstrap behavior as final product controls merely because there is UI space.

## 15. Minimal target information architecture

Keep the existing five workflows. Do not add a top-level DAW shell.

```text
GENERATE
  GENRE
    GENRE
    RECIPE
    RHYTHM
    DEPTH
    APPLY scope/status

  FEEL
    PROFILE
    SWING
    FEEL AMOUNT
    VELOCITY VAR
    FEEL CYCLE

SONG
  SONG
    physical rows + track references

  PHRASE
    first phase: generation request / navigation only
    later, after owner exposure:
      LENGTH
      BAR n/N
      IDENTITY
      PROGRESSION
      HARMONY
      MELODY

HUB
  SYNTH A NOTES
  SYNTH B NOTES
    physical editor; follows sounding pattern only while this editor is visible
```

Phrase semantic view and physical NOTES editor remain complementary, never replacements for one another.

## 16. Ordered implementation checkpoints

The provisional P1..P6 split is adjusted because I1 already implements most of the transport mapping, while semantic exposure is currently missing.

### UI-P1 — FOLLOW / STOP CONTRACT HARDENING

Scope: UI + tests only unless a demonstrated defect appears.

- protect the existing I1 mapping;
- update NOTES content only when resolved physical pattern changes;
- update playhead/highlight independently;
- no global page forcing;
- STOP keeps last sounding physical pattern selected;
- Synth A/B symmetry;
- establish redraw counters/timing acceptance without full-screen transport-tick redraw.

### UI-P2 — PHRASE REQUEST OWNERSHIP + LENGTH

Scope: narrow `src/state` request owner + UI/tests; no generation policy changes.

- separate generated Phrase Length request from PhraseCore capture length;
- one session/request owner for 1/2/4/8 selector;
- Phrase UI and any shortcut use the same request owner;
- request passes unchanged into `GeneratedPhraseSong::generate`;
- rejected request remains selected and is reported as typed rejection;
- FEEL CYCLE remains `scene.feel.patternBars` only.

If this requires changing `src/generation/**`, STOP and characterize the missing contract first.

### UI-P3A — PHRASE SEMANTIC EXPOSURE CONTRACT

Not a visual redesign. Establish whether post-commit generated phrase semantics can be authoritatively inspected without duplication.

Needed read model must be production-owned, bounded/fixed-capacity and derive from frozen carriers. No page-local semantic cache.

### UI-P3B — PHRASE SEMANTIC OVERVIEW

Only after P3A.

Read-only first:

```text
LENGTH
BAR n/N
IDENTITY
PROGRESSION
HARMONIC MOVEMENT
```

Semantic bar selection may navigate to the already resolved physical editor mapping, but physical address may not become identity.

### UI-P4 — CROSS-BAR LIFETIME VIEW

Only after corrected C2 + R1 + hardware acceptance.

### UI-P5 — GENERATE / APPLY UX OWNERSHIP CLEANUP

- make GENRE, PHRASE and local physical pattern generation scopes explicit;
- ensure no new screen directly calls low-level generation + Song write + Scene write independently;
- preserve Undo and quantized publication boundaries;
- keep Accepted / Typed Rejection / Execution Failure distinct.

### UI-P6 — HARDWARE USABILITY + REDRAW / PERSISTENCE PASS

Final product workflow acceptance on Cardputer ADV.

## 17. UI-P6 hardware acceptance protocol

### Purpose

Validate the complete hardware workflow:

```text
generate multi-bar material
-> arrange/use in Song
-> PLAY
-> Synth NOTES follows exact sounding physical pattern
-> STOP freezes last sounding pattern
-> edit it
-> PLAY again
-> save/reboot/load
```

### Hardware list

- M5Stack Cardputer ADV
- USB-C data cable
- optional external MIDI target for parallel MIDI verification
- no external I2C device required

### Wiring

No additional wiring is required for the UI test.

If unrelated PORT.A devices are attached, project invariants remain:

```text
SDA GPIO2
SCL GPIO1
M5Unit-Scroll 0x40
CardKeyboard 0x5F
PAHub 0x70
Scroll uses Wire, not Wire1
```

### Build / Flash

From the exact future UI-P6 frozen candidate:

```bash
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Serial monitor: `115200`.

### Expected behavior

Create or generate at least four distinct Synth A bars and four distinct Synth B bars referenced by successive Song rows.

While Synth A NOTES is visible:

```text
row 0 -> NOTES shows row-0 A pattern
row 1 -> NOTES switches to row-1 A pattern
row 2 -> NOTES switches to row-2 A pattern
row 3 -> NOTES switches to row-3 A pattern
```

The same applies to Synth B.

Leaving Synth NOTES for another workspace does not stop transport or force navigation back.

STOP during row 2 keeps:

```text
Song row = 2
Synth physical pattern = row-2 resolved pattern
NOTES = that same pattern
```

Edit one note, PLAY again, and hear the edited physical pattern.

Exercise at least physical pages 1..8. Existing 16-page storage capacity must remain intact.

Save, reboot and load the project; Song references, pattern content and Synth patch/type state must recover according to the existing project persistence contract.

### Troubleshooting

- If NOTES shows the correct playhead over the wrong notes, inspect the I1 physical selection mirror before touching transport.
- If STOP returns to a pre-PLAY pattern, treat it as a regression of the I1 stop-in-place contract.
- If changing UI pages changes Song row, clock owner or play state, treat it as a navigation/transport ownership violation.
- If UI refresh causes audio underruns, inspect redraw frequency; do not solve it by weakening audio timing.
- If a phrase length is rejected, verify it is a typed rejection rather than silently coercing the requested length.

### Acceptance checklist

- [ ] Synth A follows the exact sounding physical pattern across Song rows.
- [ ] Synth B follows the exact sounding physical pattern across Song rows.
- [ ] NOTES content changes only when required; transport highlight does not force full-screen redraw each tick.
- [ ] STOP preserves the last sounding physical pattern as edit target.
- [ ] Edit after STOP changes the same pattern that is played next time.
- [ ] Leaving a Synth page does not alter transport.
- [ ] At least pages 1..8 are independently editable/playable/referenced; 16-page capacity is preserved.
- [ ] Save/reboot/load restores project-scoped pattern/Song state.
- [ ] Phrase Length and Feel Cycle remain distinct.
- [ ] Typed rejection is not displayed as execution failure.
- [ ] No cross-bar lifetime UI is accepted before corrected C2/R1 hardware freeze.

## 18. UI-P0 acceptance

- [x] Exact production base identified as frozen I1 `fb30bbb9...`.
- [x] Corrected C2/R1/hardware lifetime line confirmed not frozen; lifetime UI blocked.
- [x] Current GENERATE / SONG / Synth NOTES / Phrase structure documented.
- [x] Exact I1 follow-NOTES source path documented.
- [x] Existing STOP-in-place owner/path documented.
- [x] Phrase Length and Feel Cycle explicitly separated.
- [x] Semantic phrase bar and physical pattern address explicitly separated.
- [x] Accepted / Typed Rejection / Execution Failure separated.
- [x] Harmonic WHAT / WHEN remain generation-owned.
- [x] No Activity owner invented.
- [x] GF2 treated as semantic evidence, not ancestry.
- [x] Persistent semantic exposure gap identified rather than hidden in UI state.
- [x] Minimal ordered implementation plan produced.
- [x] Production musical semantics unchanged (`src/**` delta must remain zero).
