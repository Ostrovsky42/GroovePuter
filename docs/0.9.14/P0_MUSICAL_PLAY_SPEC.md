# 0.9.14 P0 — Musical play, first slice (specification only)

Status: **specification, no product code.** Base: `fa326f45` (M0-A). Decision source: `M0_A_MUSICAL_PLAY_BASELINE.md` §11–12.

Tags used below: **[OBSERVED]** read from code or measured; **[INFERENCE]** reasoned, not verified; **[OPEN]** needs an owner
decision or a measurement before build.

## 1. What P0 is

One engine-level action that turns the kept generated phrase into a short arc, so that a player who pressed TAKE once can hear the
idea developed, broken and returned to, at 4-bar sections, with no UI change.

Cycle of 4-bar sections, in order, after the original section A (TAKE, phrase law Loop):

| Section | Phrase law, depth | Bar functions [OBSERVED, `phraseTrajectoryForLaw`, M0-A corpus] |
|---|---|---|
| A (already exists) | Loop | Statement ×4 |
| DEVELOP | DevelopReturn, P3 | Statement, Build, RepeatWithGhosts, **Turnaround** |
| BREAK | SparseDrift, P3 | Statement, RepeatWithGhosts, **Break**, **Return** |

* The **Return** bar is the fourth bar of the BREAK section. DEVELOP does not return inside itself; it ends in Turnaround.
* RETURN acceptance uses that production Return bar. An exact repeat of A (as in the audition renders) is **not** part of acceptance.
  [OBSERVED: the production Return bar was heard as a return, fourth listening pass.]
* Only the 8 new bars are published (DEVELOP + BREAK). Bounded by the existing `reserveMaterialIds(≤8)` and by a page of 16 slots.

Non-goals: UI, keys, toasts; KEEP as a workflow; repeated development A → A′ → A″; persistence across sessions; new preservation claims;
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

### 2.2 Replay verification R0 (must pass before any law change)

1. Rebuild the phrase **with the original law** from the stored recipe.
2. Compare the rebuilt events **for every lane** (Synth A, Synth B, drums) with the phrase currently in the Song rows.
   Byte-equal → proceed. Anything else → refuse with a typed status; never publish a partly matching arc.
3. Also compare against the origin evidence (bass plan, pitch witness, harmonic rhythm): an independent cross-check of the rebuild.

Consequence, stated on purpose **[INFERENCE, made explicit]**: R0 also detects **manual edits**. P0 works only on an **unedited** generated
phrase. Rebuilding from generation inputs cannot carry hand edits, so an edited phrase is refused, not silently overwritten and not silently
"developed" while losing the edit. Preserving edits is a later checkpoint.

### 2.3 Finding the real input set (first task of the build)

**[OPEN]** Differential test: for each candidate input (each field above, BPM, flavor, generative-parameter groups), change only it and regenerate;
an input belongs in the recipe iff the output changes. Inputs the recipe cannot hold (large compiled structures) are instead **re-read from
the scene and guarded by R0** (if they changed, R0 fails). Report the resulting recipe size and permanent DRAM delta.

## 3. Commit path

**[INFERENCE]** Reuse the existing generated-phrase transaction (PREPARE → target revalidation → MaterialId reservation → commit through the
UndoOwner and audio guard → publish descriptors → publish origin), with two parameters added: phrase law/depth override for the rebuilt
sections and the stored recipe. One Undo receipt for the whole 8-bar publication (fits `GeneratedPhraseUndoPayload`, bars ≤ 8).
Synth A bars get canonical MaterialIds as in D1-A. The origin sidecar is replaced by the published 8-bar phrase (D1-B rule); the recipe describing
section A stays until its own lifecycle events. Typed failures: not generated, edited, recipe missing, no safe slots/rows, reservation failed,
target changed, archetype not admitted. Semantic UNKNOWN never blocks (M0-A firewall).

## 4. Admission

* **Excluded from the first slice, explicitly:** Acid and House. **[OBSERVED]** their archetypes are not admitted to phrase evolution (always Loop);
  admitting them widens the musical task. They return a typed "not admitted" status.
* **Candidate set:** archetypes admitted by `phraseEvolutionEnabled` (in the M0 corpus: Techno, Darksynth, Dub, Funk, UKG, DnB families).
* 8-bar phrases exist for Dub only (typed length-policy rejection elsewhere); P0 uses 4-bar sections, so this does not block it.

## 5. Audibility criterion (per included scenario)

**[INFERENCE]** A scenario is included only if all three hold:
1. **Structural:** in DEVELOP or BREAK, the **bass or the chord lane** changes (attack pattern or pitch classes) in at least one bar. Changing both
   every time is not required. Drum-only change is reported as such and does **not** qualify a scenario.
2. **Heard in the full mix** at the new default mix (§6), by the owner.
3. **Related but changed:** owner hears it as connected to A yet different.

Evaluation set: a fixed list of identities per genre (proposal: ordinals 0–7), **including the known weak cases (UKG)**, not the best Dub identity.
**[OBSERVED, M0-A, ordinal 0, P3]** bass or chord changes in Funk (DEVELOP and BREAK) and in Dub (**BREAK only**; Dub's DEVELOP is drums-only);
Techno, UKG and DnB change drums only, in both sections.
**[OPEN]** inclusion threshold (proposal: structural criterion met in ≥ 6 of 8 identities). If Techno/UKG/DnB fail it, P0 ships for the archetypes
that pass and the rest are listed as a gap; extending bar-function mutation to the bass role is a separate decision, not assumed here.

## 6. Mix defaults

**[OBSERVED]** track faders default to 1.0 for all ten voices (`scenes.h:417`), `setTrackVolume` clamps to 1.5 (about +3.5 dB), faders are stored in the
scene JSON, and a scene without the key loads at 1.0 (`scenes.cpp:594`). The ~16 dB difference is total-synth vs total-drum RMS on a host render;
it does not mean each synth needs +16 dB.

Decision taken by the owner: raise synth levels by default. Constraints:
* **New scenes only.** Change the default of newly created scenes; keep the loader default for a missing key at 1.0 so old scenes do not change sound.
  Scenes that saved explicit 1.0 keep it. **[OPEN]** identify every new-scene creation path (initializer vs reset) and change only those.
* Faders alone give at most about +3.5 dB for the synths. Choose SynthA/SynthB (up to 1.5) and drum values (down) from **comparative renders**
  with clipping/limiter checks; report the resulting synth-to-drum RMS ratio. **[OPEN]** if faders cannot reach a usable ratio, a voice-level gain
  change is a separate compatibility decision and is out of P0.
* Host render only until compared on the device; state that in results.

## 7. Verification

* Host tests: R0 passes on unedited phrases for every included genre/ordinal; R0 refuses after a manual edit, after a scene load, after Undo, and
  when a hidden input changed; law change publishes the same 8 bars the M0 tool produces for the same inputs; failure paths publish nothing.
* Unchanged gates: D0-C…D1-C1, M0-A, PMB-P1 (GCC/ASan/UBSan, heap bound), host, unified Song slots, SDL, Cardputer ADV, SEQTRAK.
* Budgets: `PreparedPhraseArrangement` ≤ 1024 B; report recipe size and permanent DRAM before/after (ADV 189736 / 191488, SEQTRAK 189680 / 191488 today).
* Audio: `tools/m0/render_m0a_audio.sh` renders the arc from the published bars; owner listens to the fixed identity set. The structural table is
  regenerated by `tools/m0/build_m0a.sh`.

## 8. Open decisions for the owner

1. Inclusion threshold for §5 (proposal: 6 of 8 identities) and the identity set.
2. If drum-only archetypes fail §5: ship P0 for passing archetypes only, or extend the bass role's bar-function mutation first.
3. Concrete synth/drum default values (from comparative renders, §6).
4. Whether an edited phrase should later be supported by carrying the edit, or stay refused.
