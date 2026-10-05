# 0.9.14 P0 — Musical play, first slice (specification only)

Status: **engine path implemented; MATERIAL page D gesture under verification (section 13); device acceptance open.** Base: `fa326f45` (M0-A). Decision source: `M0_A_MUSICAL_PLAY_BASELINE.md` §11–12.

Tags used below: **[OBSERVED]** read from code or measured; **[INFERENCE]** reasoned, not verified; **[OPEN]** needs an owner
decision or a measurement before build.

## 1. What P0 is

One engine-level action that turns the kept generated phrase into a short arc, so that a player who pressed TAKE once can hear the
idea developed, broken and returned to, at 4-bar sections. Section 13 documents the subsequent UI gesture.

Cycle of 4-bar sections, in order, after the original section A (TAKE, its naturally selected phrase law):

| Section | Phrase law, depth | Bar functions [OBSERVED, `phraseTrajectoryForLaw`, M0-A corpus] |
|---|---|---|
| A (already exists) | naturally selected | determined by the accepted TAKE; see §12 |
| DEVELOP | DevelopReturn, P3 | Statement, Build, RepeatWithGhosts, **Turnaround** |
| BREAK | SparseDrift, P3 | Statement, RepeatWithGhosts, **Break**, **Return** |

* The **Return** bar is the fourth bar of the BREAK section. DEVELOP does not return inside itself; it ends in Turnaround.
* RETURN acceptance uses that production Return bar. An exact repeat of A (as in the audition renders) is **not** part of acceptance.
  [OBSERVED: the production Return bar was heard as a return, fourth listening pass.]
* Four or eight new bars are published. A section whose programme already equals A is skipped (§12). Bounded by the existing `reserveMaterialIds(≤8)` and by a page of 16 slots.

Non-goals: KEEP as a workflow; repeated development A → A′ → A″; persistence across sessions; new preservation claims;
Genre validation; changing the DSP.

## 2. Rebuild contract (what "the kept idea" means)

**[OBSERVED]** D1-B/B1 stores `phraseGenerationIdentity` (a `uint16` = attempt ordinal modulo a constant), root, scale, progression source,
per-bar bass plan / pitch witness / harmonic rhythm. That is evidence about the result. It is **not** the inputs needed to regenerate it.
The M0 tool works because it re-prepares under fixed conditions in one process; that is not yet a proof of rebuild from stored context.

**[OBSERVED]** What generation reads (from `materializationSettingsFor`, `preparePhraseExecution`, `prepareDestinationIndependentPitchSource`):
`GenreSettings` (mode, recipe, morph, rhythm selection mode/id), realization level, the **full** attempt ordinal, bar count,
feel timing profile and amount, tonal root and scale, and (for pitch source) the genre manager's compiled generative parameters and behavior,
mode flavor and BPM.

### 2.1 Stored context: `GeneratedPhraseRecipe` (session-local, bounded)

Published by `GeneratedPhraseSong` together with the origin sidecar, in engine state, **outside** `PreparedPhraseArrangement`
(its 1020-byte / 1024 limit is not touched). Fields: the values listed above that are not derived from other stored fields.
**[OPEN]** exact field list: settled by the differential test in §2.3.
Cleared on Undo of the phrase, on successful Legacy generation and on scene load (same lifecycle as the origin sidecar).

### 2.2 Replay verification R0 (reproduces the source phrase only)

1. Rebuild the phrase **with its naturally selected law** from the stored recipe.
2. Compare the rebuilt **canonical musical events of every lane** (Synth A, Synth B, drums) with the phrase currently in the Song rows.
   "Equal" means equal note/hit, step, timing offset, duration, articulation flags, velocity/probability and effect fields as a canonical
   event list, **not** raw memory bytes (padding and service fields are excluded). Any difference → typed refusal; never publish a partly matching arc.
3. Cross-check against the origin evidence (bass plan, pitch witness, harmonic rhythm).

**R0 proves that the original phrase is reproduced. It does not prove that the recipe is complete for the later laws.** A setting can leave the
original Loop unchanged and still change DEVELOP or BREAK; R0 passes and the new section is then built in a different context. Therefore:

* **R1 (context fingerprint):** at law-application time every dependency that is *not* stored in the recipe must be re-read and compared with a fingerprint
  taken when A was published; a mismatch is a typed refusal, independent of R0.
* Neutral, unedited-only consequence **[INFERENCE, made explicit]**: R0 also detects manual edits. P0 works only on an **unedited** generated phrase;
  rebuilding cannot carry hand edits. Refusal is a **P0 boundary, not the intended long-term behaviour**: for a real instrument, losing the ability to develop
  an idea after improving it is a poor boundary. Supporting edited phrases is a future product task; P0 does not design edit transfer.

### 2.3 Finding the real input set (first task of the build)

**[OPEN]** The recipe/fingerprint contents are derived from the dependencies of **all three laws** (Loop, DevelopReturn, SparseDrift), following preparation
**and** materialization (including trajectory selection, bar-function mutation, feel, tonal materialization, pitch source, progression, harmonic clock),
not from Loop alone. A differential test (change one candidate input, regenerate all three laws, compare) is used to **check** that list. A test that shows
no change for an input on the sampled identities does **not** prove the input is unnecessary. Each dependency must either be reproduced from stored values
(recipe) or verified unchanged (R1). Report recipe size and permanent DRAM delta.

### 2.4 Depth

The recipe records the realization depth of A. Trajectory 8 (Break) exists only at P3 and production's default depth is P2.
**[OBSERVED, section 9]** the Loop phrase for one identity differs between P2 and P3 in most bars, so developing a P2 phrase with P3 programmes would change law
and depth at once. P0 therefore requires A to have been generated at P3 (typed `DepthNotP3` refusal). The product default (P2) is not changed by this slice.
**[UX INFERENCE]** Before a user action is connected, the ordinary path must be decided: either the TAKE that precedes it creates A at P3, or developing a P2
phrase (with an explicit depth change) is supported separately. An action that refuses on default settings is not a finished feature.

## 3. Commit path and lifecycle

**[INFERENCE]** Reuse the existing generated-phrase transaction (PREPARE → target revalidation → MaterialId reservation → commit through the
UndoOwner and audio guard → publish descriptors → publish origin), with two parameters added: phrase law/depth for the rebuilt sections and the stored recipe.
One Undo receipt for the whole 8-bar publication (fits `GeneratedPhraseUndoPayload`, bars ≤ 8). Synth A bars get canonical MaterialIds as in D1-A.

Lifecycle, defined for this slice:
* **A is not modified.** Its Song rows, patterns and descriptors stay as they are. DEVELOP and BREAK are both prepared from **A's one recipe**
  (never from each other), and are written into the rows following A.
* **Sidecars.** The origin sidecar becomes the published 8 bars (D1-B rule: latest generated phrase). A's recipe is retained by its own lifecycle
  (cleared on Undo of A, on successful Legacy generation, on scene load).
* **Calling again** while the cycle exists: typed "cycle already published"; no silent second cycle and no re-roll in P0.
* **Undo** restores the state before the cycle (rows, patterns, descriptors) in one step, clears the 8-bar origin (D1-B rule), and **keeps A's recipe**,
  so the action can be repeated after Undo.
* Typed failures: not generated, edited (R0), context changed (R1), recipe missing, depth not P3, archetype not admitted, no safe slots/rows, reservation
  failed, target changed. Semantic UNKNOWN never blocks (M0-A firewall).

## 4. Admission

* **Excluded from the first slice, explicitly:** Acid and House. **[OBSERVED]** their archetypes are not admitted to phrase evolution (always Loop);
  admitting them widens the musical task. They return a typed "not admitted" status.
* **Candidate set:** archetypes admitted by `phraseEvolutionEnabled` (in the M0 corpus: Techno, Darksynth, Dub, Funk, UKG, DnB families).
* 8-bar phrases exist for Dub only (typed length-policy rejection elsewhere); P0 uses 4-bar sections, so this does not block it.

## 5. Admission criteria for this experiment

These are **conditions of an experiment**, not a musical norm. A convincing Techno development can live in drums, density and orchestration; the bass/chord
requirement below exists because the experiment asks whether development can reach the lanes the default mix currently hides. Listening is **not** turned
into a runtime validator.

**Evaluation set.** For every considered genre: identities (attempt ordinals) **0–7**, all sections at P3. The known weak UKG case is added as a separate
control example if it is outside that range. Results are per identity and per archetype (the archetype an identity selects), with the count of identities that reach each archetype.
Failing examples stay in the report; they are never replaced by better ones.

**Structural condition, 8/8.** For an archetype to be included, in **every** identity of the set that selects it, the full cycle (DEVELOP and BREAK together)
changes the **bass or the chord lane** (attack pattern or pitch classes) in at least one bar. Not both lanes, and not in every section. A weaker threshold is rejected:
it would admit cases where the requested change never reaches the target lanes.

**Listening condition, at least 6 of 8** identities of each included archetype, judged by the owner at the new default mix:
1. **DEVELOP** is heard as related to A but changed;
2. **BREAK** is heard as a contrast;
3. the production **Return** bar is heard as a return.
The three results are recorded **separately** per identity; a good BREAK does not confirm a good DEVELOP.

**Scope of the claim.** Whatever passes is a limited prototype for those archetypes on those identities. If only Funk passes, the result is named that way and makes
no claim about DEVELOP in general. Drum-only archetypes are listed as a gap; extending bass transformations is not part of this slice.

## 6. Mix defaults

**[OBSERVED]** track faders default to 1.0 for all ten voices (`scenes.h:417`), `setTrackVolume` clamps to 1.5 (about +3.5 dB), faders are stored in the
scene JSON, and a scene without the key loads at 1.0 (`scenes.cpp:594`). The ~16 dB difference is total-synth vs total-drum RMS on a host render;
it does not mean each synth needs +16 dB.

Owner decision: raise synth levels by default. Constraints:
* **New scenes only.** Change the default of newly created scenes; keep the loader default for a missing key at 1.0 so old scenes do not change sound.
  Scenes that saved explicit 1.0 keep it. **[OPEN]** identify every new-scene creation path (initializer vs reset) and change only those.
* **Values stay open [GAP]** until comparative renders exist (§7). The balance is chosen for **audibility of the parts and for keeping the drum impact**.
  RMS ratio is a diagnostic, not a target to equalize. Renders are also checked for loudness gained by constant limiting (peak, crest factor, time at the ceiling).
* **No DSP gain change in P0.** If faders cannot reach a usable balance, a voice-level gain change is a separate compatibility decision.
* Host render only until compared on the device; results say so.

**Owner's mix choice (after listening to A, B, C on three identities):** **B, synths 1.5 and drums 0.8** (new-scene defaults candidate). It gives
about −11…−12 dB synth-minus-drums by the diagnostic, keeps most of the drum level (about −2 dB absolute), and is inside the fader range, so no DSP gain
change is needed. Still open: a check on the device output stage, and identifying every new-scene creation path (old scenes keep 1.0).

## 7. Verification (order of work)

1. **Fixed corpus and mix variants with `tools/m0`, before any product path** (`tools/m0/build_m0a_p0.sh`): per genre × ordinals 0–7 at P3, the structural table for
   DEVELOP and BREAK (drums / bass rhythm / bass pitch / chord bars changed vs A, Return bar equality), the P2-vs-P3 Loop comparison (§2.4), and a mix table
   (synth-only vs drums-only RMS, peak, time at the ceiling) for candidate fader sets.
2. If several archetypes pass the structural condition: build the narrow slice. If none does: the next step is a musical transformation that reaches bass/chords,
   not a more elaborate recipe.
3. Host tests: R0 passes on unedited phrases for every included genre/ordinal; R0 refuses after a manual edit, scene load, Undo, and when a stored input differs;
   R1 refuses when a non-stored dependency changed **even though R0 passes**; the law change publishes the same 8 bars the M0 tool produces; failure paths publish nothing.
4. Unchanged gates: D0-C…D1-C1, M0-A, PMB-P1 (GCC/ASan/UBSan, heap bound), host, unified Song slots, SDL, Cardputer ADV, SEQTRAK.
5. Budgets: `PreparedPhraseArrangement` ≤ 1024 B; report recipe size and permanent DRAM before/after (ADV 189736 / 191488, SEQTRAK 189680 / 191488 today).
6. Audio: `tools/m0/render_m0a_audio.sh` renders the arc; the owner listens to the fixed identity set.

## 9. Results of the fixed corpus (§7 step 1; `tools/m0/build_m0a_p0.sh`, host, P3, ordinals 0–7)

**Reachability.** Identities of 8 that land on an archetype admitted to phrase evolution (the only ones a law change can apply to):
Techno 5, UKG 7, DnB 7, Dub 2, Funk 2, Darksynth 1, Acid 0, House 0. For Dub and Funk, six of eight identities would be refused as "archetype not admitted"
(steppers, straight_drive, funk_house_bridge, halftime_switch).

**Structural condition (§5), per archetype, over the identities that reach it** (bass or chord changes in at least one bar of the cycle):

| Archetype | applicable identities | pass |
|---|---|---|
| DnB / sparse_fast_break | 2 | **2 of 2** |
| Funk / sparse_fast_break | 2 | 1 of 2 |
| Dub / broken_techno | 2 | 1 of 2 |
| UKG / classic_2step, UKG / skippy_2step | 2 each | 1 of 2 each |
| Techno / machine_syncopation, UKG / machine_syncopation | 3 each | 0 |
| Techno / broken_techno, DnB / ghosted_roll, DnB / two_step_roll, Darksynth / broken_techno | 2, 4, 1, 1 | 0 |

Two observations: (1) the result varies **within** an archetype (it depends on the identity's own bass plan), so per-archetype counts are small and unstable;
(2) where bass/chord changes at all, it is 1 or 2 of 4 bars. By the 8/8 rule exactly one archetype passes, on two identities. That is not "several archetypes".

**Depth (§2.4).** The Loop phrase at P2 and at P3 for the same identity is **not** the same phrase: drums differ in every bar of most identities
(Techno 32 of 32 bars, Dub 32, UKG 32, DnB 28, Funk 20 with 8 bass bars). Developing a P2 phrase with P3 programmes changes law and depth together, so P3 stays required.

**Mix (§6), engine faders only, Loop phrase A, host render.** Synth-minus-drums RMS: default −15.6 (Techno), −17.0 (Dub), −16.1 dB (Funk).
Synths 1.5 with drums 1.0: −12.3 / −13.6 / −12.8 dB. Synths 1.5 with drums 0.45: −7.6 / −8.8 / −8.0 dB, at the cost of the drums being about 5 dB quieter.
Peak of the full mix stays at or below 0.15 of full scale and no sample reaches the ceiling, so nothing is being limited; there is large headroom. Faders alone
cannot bring the synths above about −8 dB relative to the drums. Balances B and C are in the published audition page for listening; no values are chosen yet.

**Outcome against the rule in §7.** One archetype passing on two identities is too thin to build the recipe path on. Options for the owner:
(a) ship a prototype limited to DnB / sparse_fast_break and state it as such; (b) make the bar-function programmes act on the bass and chord roles and widen
the admitted-archetype list first, then re-run this corpus; (c) accept drum-led development explicitly for the archetypes that fail (a musical judgment, not a structural one).
Recommendation: (b), because the same corpus shows the limit is the transformations and the admission list, not the recipe mechanism.

## 8. Open decisions

1. The historical §9 choice was superseded by the B1/B2 realization changes and the engine path in §11. Mix C is the current audition candidate (§10); new-scene defaults and a device check remain open.
2. Ordinary path for depth (section 2.4 and 11): P3 TAKE, or explicit P2 development. Measurement is done (section 9); the decision before wiring a user action is open.
3. The identity set beyond 0–7 (the UKG control example).
4. Later: supporting edited phrases (a future product task, not P0).

## 10. Mix update after B1 listening

At mix B (synths 1.5, drums 0.8) the bass was still too quiet on `broken_techno_ord0`. Owner decision: **mix C (synths 1.5, drums 0.45) as the candidate**, no DSP gain change, DEVELOP strength unchanged
until the other seven renders are heard. Mix C puts the synth lanes about 8 dB under the drums and the drums about 5 dB under their default level; whether the drums are still strong enough is a listening question.
Eight cycles at mix C are on the audition page. Device check and new-scene-path identification remain open (spec section 6).

## 11. Engine path: implemented state (`ccd0c6b0`, `80e3d4b1` and later)

**[OBSERVED]** Code: `GeneratedPhraseRecipe` (`src/state/generated_phrase_recipe.h`, 48 B, session-local, in engine state);
`GeneratedPhraseSong::generateCycle`; `applyPhraseLawToExecution` shared with the M0 tool (golden dumps unchanged, 1464/1464 baseline bars).
Tests: `tests/run_0_9_14_p0_cycle_tests.sh` (in CI), all other gates green at `80e3d4b1`.

* Recipe = genre settings + materialization settings (level, attempt ordinal, feel, root, scale) + identity + page/slot/rows + R1 fingerprint of the
  engine-read pitch-source inputs (genre manager recipe/mode/params/behavior, flavor, BPM).
* R1 also compares the current scene genre and the scene-derived settings with the recipe. R0 rebuilds the kept phrase from the recipe and compares the canonical
  hash of every lane of every bar with the Song rows (an edit, or a row pointing elsewhere, is refused).
* Verified on host: A untouched; each section equals an independent rebuild from the same recipe (independent of physical pattern address); eight distinct
  MaterialIds; origin sidecar of 8 bars; one Undo removes the cycle and keeps the recipe; repeat after Undo gets fresh ids; second call refused
  (`CycleAlreadyPublished`). Typed refusals covered: no recipe, P2, edited, context (scale, genre, BPM), rows occupied, Undo of A, Acid, AcidRolling, House (each with
  a recipe present, so `NotAdmitted` is not masked by `NoRecipe`).
* Live (host, real engine render): `PendingNextBar`, all eight rows present at commit, activation at the bar boundary, nothing left pending; stop or Undo while pending
  leaves no state and playback never enters the undone rows.
* Cycle start: when stopped, Song position is set to DEVELOP (first row after A).

**Firmware build (Xtensa, `scripts/build.sh`, loop stack 32768 B) [OBSERVED]:**
* Permanent DRAM (`.dram0.data + .dram0.bss`): 189736 B before the recipe (`ccd0c6b0`), 189784 B after: **+48 B**, exactly the recipe. `.data` unchanged.
* Static stack frames (`-fstack-usage`, measured before the UI gesture using a temporary call site behind a build flag, not committed):
  `generateCycle` 5216 B, `verifyKeptPhrase` 2896, `preparePhraseExecution` 1200, `applyPhraseLawToExecution` 704, `materializeOneBar` 160.
  For comparison `generate` 2576, `prepareWithGenerationAttempt` 1808, `applyPreparedPersistent` 1536.
  Deepest known chain `generateCycle -> verifyKeptPhrase -> preparePhraseExecution` = about 9.3 KB of static frames before the frames below `preparePhraseExecution`
  and before the UI/loop frames above; the existing `generate` chain is about 5.6 KB by the same count. These are compile-time frames, **not** a run-time high-water mark.
  The cycle costs roughly 3.7 KB more stack than `generate`; whether that is safe on the device is unmeasured. Lowest-cost reduction if needed: verify R0 with a
  smaller scratch, or run R0 before the two executions and the arrangement are live.

**Not verified:** run-time stack high-water on the device, sound of the published cycle on the device, mix C on the device and new-scene defaults.

## 12. Finding: the kept phrase is never Loop; DEVELOP repeats A in 39% of identities

**[OBSERVED]** Published-cycle renders (14 takes) showed five DEVELOP sections identical to A in every lane. Diagnosis (`P0_CYCLE_DIAG=1`, `P0_CYCLE_SURVEY=1` in
`tools/m0/p0_cycle_render.cpp`, read-only): the kept phrase generated by TAKE at P3 carries **its own natural phrase law**, chosen from its identity.
Survey of the eight admitted archetypes used so far, identities 0..7, P3 (64 phrases): natural law is **RepeatReply 39, DevelopReturn 25, Loop 0, SparseDrift 0**.
Section 1 of this document assumed "A = Loop"; that is not what production makes.

* When A's natural law is DevelopReturn (25 of 64), requesting DevelopReturn for DEVELOP reproduces A's own bar-function programme: the section equals A. The plan does
  differ from bar 0 inside the phrase, but A's own bars 1..3 already are Build / RepeatWithGhosts / Turnaround, so nothing new is published.
* BREAK (SparseDrift) never coincides with A's natural law in this sample.
* The earlier "empty DEVELOP" is therefore not a missing legal transformation (the ghost-add search, the bass Build/Turnaround add and the protected spaces are not the cause);
  it is a request that names the law A already has.

**Resolved in `008ef57`:** when DEVELOP would repeat A, publish BREAK alone (four bars) and return `developSkipped=true`, `bars=4`. Otherwise publish DEVELOP and BREAK (eight bars). When neither section adds anything, return `NothingToAdd`. R0 still rebuilds A with its natural law. The original §1 table's `A = Loop` describes the first stitched audition, not the natural law of a product TAKE.

## 13. MATERIAL page gesture (post-engine UI slice)

On the product MATERIAL page, select **STYLE REWORK** and **LENGTH 4B**, then press **G** to generate a new TAKE. Press **D** while its source context is still valid. D calls the existing `GeneratedPhraseSong::generateCycle` on the UI/control thread; it does not perform musical work in the audio callback. A successful result says `DEVELOP + BREAK 8B` or `BREAK ONLY 4B`, followed by `IN SONG` or `NEXT BAR`. The MATERIAL page keeps a compact cycle and Song-row summary visible while the session recipe and published origin remain live. The existing generated-phrase Undo receipt removes the whole addition in one action and clears the summary. The MATERIAL BANK page keeps its distinct D (derive) command.

A source TAKE at a different length is refused before invoking the cycle, with `MAKE A 4B TAKE TO GROW`. A P2 source is refused by the engine and presented as `SET REWORK, THEN NEW TAKE`; changing STYLE after the TAKE cannot silently convert its already generated material. Other typed failures are shown as brief cause-specific messages. This gesture does not change the default P2 style, persist the session recipe, or claim support for edited A, Acid, or House.

**Device gate remains open:** build the exact UI candidate with FS1B, record ELF/BIN checksum and DRAM, flash that BIN, then test both the eight-bar and four-bar outcomes, bar-boundary activation while playing, Undo, repeated attempt, visible refusals, and stack/heap high-water under repeated cycles. Assess mix C with the physical output separately; a host render does not set new-scene faders.

## 13. Mix C for new scenes: implemented

**[OBSERVED]** `applyNewSceneMix` (`scenes.h`): Synth A/B faders 1.5 (the fader maximum), drums 0.45, applied only in `SceneManager::wipeToZero()` and `SceneManager::loadDefaultScene()`.
Those are the new-scene paths: new project (`createNewSceneWithName`), Project page clear, Ctrl+Alt+Backspace reset, and the boot fallback when no scene can be parsed.
Neither function touched `trackVolumes` before, so a "new" scene used to **inherit the previous scene's faders**; it now starts from mix C.

* Loaders are unchanged: the streaming loader and the document loader set 1.0 before parsing, so a scene file without `trackVolumes` loads at 1.0 (tested through both).
* A saved scene reloads with exactly its own faders (tested: a changed kick fader survives, other drums keep 0.45).
* DSP gain, master volume (0.6) and the fader clamp are unchanged.
* Not verified: how mix C sounds on the Cardputer speaker beyond the owner's listening of the published cycle; drums are about 5 dB quieter than before in new scenes.
