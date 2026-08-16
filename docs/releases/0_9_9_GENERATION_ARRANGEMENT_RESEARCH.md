# GroovePuter 0.9.9 Research — Generation Revision, Compatibility & Live Arrangement

Status: **RESEARCH / DESIGN ONLY**  
Branch: `research/0.9.9-generation-arrangement`  
Research date: 2026-08-16  
Exact research baseline: `dev_0.9.6 @ 0b284d1fdb6596957ffa998b0d5effcc27d761ca`  
Production behavior changed by this track: **NO**

## 1. Research question

The provisional 0.9.9 release theme combines two roadmap directions that were previously separate:

1. Generation Revision / Compatibility.
2. Song / Phrase Live Arrangement.

The working hypothesis is that both belong to one architectural problem:

> **MUSICAL MATERIAL LIFECYCLE**

```text
Genre / Feel / Generation
          |
          v
   generated material
          |
          v
   Pattern / Phrase
          |
          v
 pending replacement
          |
          v
  musical boundary
          |
          v
   Song / playback
          |
          v
 Save / reboot / Load
```

This document tests that hypothesis against the current repository. It does **not** authorize a monolithic production rewrite and does **not** define temporary Undo APIs while 0.9.8 is unfinished.

## 2. Executive verdict

**Verdict: MERGE AS ONE RELEASE TRACK, SPLIT AS AN IMPLEMENTATION SERIES.**

The merge is not artificial. Current production code already crosses the Generation / Pattern / Phrase / Song boundary in several places:

- quantized generation prepares immutable material and publishes it at `BAR_START`;
- Song generation allocates Pattern storage and commits Pattern contents plus Song references transactionally;
- generated multi-bar phrases allocate Pattern material and write Song rows;
- Phrase Core persists references to Pattern material and can write/insert those references into Song;
- Song already separates edit slot and playback slot and has runtime LiveMix behavior;
- Song reverse already has a pending musical-boundary activation path.

These are different implementations of the same lifecycle concerns: **prepare, own, commit, activate, reference, persist, restore**.

However, the regression surface is too large for one production PR. There are currently multiple mutation and activation mechanisms with different semantics. 0.9.9 should therefore be one roadmap/release track with narrow ordered PRs after 0.9.8 freezes its mutation/Undo contract.

The former standalone `0.9.11 — Song / Phrase Live Arrangement` is a valid **candidate for absorption into 0.9.9 at roadmap level**. Its removal as a separate roadmap slot should remain conditional until the split gates in section 13 are cleared.

## 3. Baseline audit

The old 0.9.4 SHA was not used as the sole architectural truth.

### 3.1 Last merged/frozen production lineage

- `0.9.4 FINAL` was merged by PR #282.
- Frozen 0.9.4 SHA: `d3db4e48ebc08862bdaf9f62532414f009839192`.
- `0.9.6 Output Ownership` production PR #285 is now merged.
- Current `dev_0.9.6` head: `0b284d1fdb6596957ffa998b0d5effcc27d761ca`.
- Therefore the exact 0.9.9 research baseline is `0b284d1fdb6596957ffa998b0d5effcc27d761ca`.

The roadmap statement that 0.9.6 was still waiting for merge was stale at research time. The separate 0.9.6 research PR #284 remains a draft, but the production ownership implementation is already merged in #285.

### 3.2 Generation / Song / Phrase delta from 0.9.4 to 0.9.6

A commit comparison from `d3db4e48...` to `0b284d1f...` shows no changes to the Generation, Song, or Phrase implementation files. The 0.9.6 delta is concentrated in output ownership, queues, output persistence, sampler/output integration, and related UI/tests.

This is useful evidence, but it does **not** make the 0.9.4 tree the baseline. It proves that the relevant musical-material implementation survived unchanged into the later merged release while adjacent ownership and persistence contracts changed.

### 3.3 0.9.7 state

0.9.7 Device Profiles is still an unmerged stacked series. PRs #286 through #293 are drafts. The latest observed R7 head is:

`10e8d4be3a878386189ebf17d88b6757ae928df7`

The R1-R7 stack is based on the 0.9.4 lineage and is not an integrated successor to `dev_0.9.6` yet. A comparison from 0.9.4 through R7 changes MIDI/device-profile/bootstrap/docs/tests but does not change Generation/Song/Phrase implementation files.

0.9.9 research may use this as forward evidence only. Production 0.9.9 must rebaseline after 0.9.7 integration rather than copying its stack into this branch.

### 3.4 0.9.8 state

A branch named `research/0.9.8-undo-safe-editing` exists, but at research time it points to the same R7 SHA:

`10e8d4be3a878386189ebf17d88b6757ae928df7`

No distinct 0.9.8 commit and no 0.9.8 PR were found. Therefore:

- 0.9.8 is **not frozen**;
- there is no canonical integrated Undo/mutation API for 0.9.9 to consume yet;
- 0.9.9 may define requirements for that boundary;
- 0.9.9 production code must wait for the frozen 0.9.8 contract;
- 0.9.9 must not create temporary Undo services, compatibility shims, or a parallel mutation history.

## 4. Current ownership map

| Area | Current owner/path | Persistent mutation | Pending/live boundary | Important observation |
|---|---|---:|---:|---|
| Current-pattern Generation during PLAY | `src/generation/migration/quantized_generation_commit_impl.h` | Yes | Next bar | Fixed double-buffer candidate publication already exists |
| Current-pattern Generation while stopped | same | Yes | Immediate | Uses a different commit path from PLAY |
| Song-cell / Song-row Generation | `src/dsp/song_pattern_materializer.h` + `SongPage` | Yes | Immediate guarded commit | Pattern allocation + Pattern data + Song refs are one logical mutation |
| Generated multi-bar Phrase -> Song | `src/dsp/generated_phrase_song.h`, `src/dsp/phrase_generator.h` | Yes | STOP only | UI rejects PLAY; allocator differs from Song materializer |
| Phrase capture/derive/write | `src/phrase/*`, `PhraseWorkspace` | Yes | Guarded immediate mutation | PhraseBank is a persisted reference domain |
| Direct Song editing | `src/ui/pages/song_page.cpp` | Yes | Guarded immediate mutation | Many direct mutation call sites exist |
| Song local Undo | `SongPage` `g_undo_history` | Runtime history for persistent edits | Immediate | Legacy/local owner; must not be extended by 0.9.9 |
| Song playback-slot switching | `MiniAcid::setSongPlaybackSlot()` | No | Immediate | Existing live arrangement primitive, not yet quantized |
| Song reverse | `queueSongReverseToggle()` | Scene change at boundary | Song-row boundary | Existing precedent for pending arrangement activation |
| Phrase persistence | `src/phrase/phrase_persistence.h` | Yes | N/A | Phrase identity and Pattern refs survive save/load |
| Output ownership | merged 0.9.6 `src/output/*` | Yes | Runtime projection | Must remain orthogonal to musical-material generation |

## 5. Finding A — quantized Generation already implements a material lifecycle

`src/generation/migration/quantized_generation_commit_impl.h` is not merely a key handler. It already owns a bounded lifecycle:

```text
capture exact PatternTarget
        |
        v
prepare private candidate
(no Scene mutation)
        |
        v
fixed slot: Writing -> Ready
        |
        v
PendingNextBar
        |
        v
BAR_START claims Ready -> Reading
        |
        +-- target changed -> cancel
        |
        v
publish Pattern/Genre/Feel state
        |
        v
markSceneMutated() once
```

Important properties already present:

- no control-side heap allocation for pending material;
- newest intent can replace an unpublished pending candidate;
- AudioTask does not run the expensive generator;
- exact page/bank/slot target is captured and revalidated;
- target drift cancels publication instead of writing into a different Pattern;
- full and synth-only scopes are explicit;
- Scene revision advances once after successful boundary publication.

This is strong evidence for a common lifecycle boundary.

The weakness is fragmentation: stopped generation and playing generation commit through different paths, and the pending callback is still installed through a compatibility bridge in `GenreSceneView`.

## 6. Finding B — Generation Compatibility is real production debt, not a cleanup label

Several compatibility contracts are observable in current code and must survive 0.9.9:

### 6.1 Persisted genre identity

`src/dsp/genre_manager.h` explicitly preserves persisted `GenerativeMode` values `0..8` and keeps new values append-only. Recipe IDs also carry historical identity.

0.9.9 must not renumber enum values to make the code prettier.

### 6.2 Migration backends are intentionally runtime-only

`src/generation/generation_backend.h` defines `LegacyAtlas`, `LegacyProcedural`, and `Vocabulary` as a migration-only runtime route and explicitly keeps backend selection out of Scene persistence.

0.9.9 should finish compatibility at the lifecycle boundary rather than persist an implementation backend as user state.

### 6.3 Stable generation identity

`src/generation/generation_context.h` separates phrase identity from realization level so P1/P2/P3 can share a stable identity domain.

Any revised material transaction must preserve deterministic request identity instead of deriving a new seed merely because publication is delayed.

### 6.4 Existing BAR_START hook is already labelled a compatibility bridge

`GenreSceneView::setPendingCommitHook()` exists to bridge the newer quantized material commit into MiniAcid's historical BAR_START path while suppressing the old audio-thread regeneration behavior.

This is exactly the kind of compatibility seam that 0.9.9 may retire **after** an equivalent canonical boundary exists. It must not be removed first.

## 7. Finding C — Song generation and arrangement already share storage ownership

`src/dsp/song_pattern_materializer.h` demonstrates the strongest current Generation <-> Arrangement coupling.

A Song generation action does not only create notes. It must:

1. identify safe Pattern storage;
2. avoid overwriting manual/imported material;
3. prepare all requested tracks before mutation;
4. write generated Pattern contents;
5. write Song references to those Patterns;
6. update Song length;
7. advance Scene revision once.

The implementation already prepares `PreparedMaterial` before invoking the synchronous commit callback. Failure before commit leaves Scene unchanged.

This means the correct ownership boundary is larger than `Generator` but smaller than the entire UI: it is the **persistent musical-material transaction**.

## 8. Finding D — Song-generated Pattern liveness is a compatibility contract

Song-generated Pattern ownership is stored in an existing `unused` bit:

`kSongGeneratedOwnershipBit = 0x01`

This intentionally avoids a new Scene/DRAM field and survives PatternPaging persistence. The allocator may reuse a uniquely-owned generated Pattern in place or reclaim an unreferenced Pattern that carries this ownership bit. Non-empty manual/imported material is never reclaimed by this path.

This behavior must be preserved or explicitly migrated.

### Open liveness question: Phrase references

`SongPatternMaterializer::globalPatternReferenceCount()` currently counts references from the two Songs. Phrase Core can also persist Pattern references in `PhraseBank` as `ReferenceView`, and current Phrase UI identifies those references as mutable backing (`REF MUT`).

This is **not declared a bug by this research**. Two interpretations are possible:

1. Phrase reference views are liveness roots, so a Pattern still referenced by PhraseBank must not be reclaimed.
2. `REF MUT` deliberately means the Phrase aliases mutable backing, including later Song-generation replacement/reuse.

0.9.9 must make this contract explicit and test it. Silent behavior is not acceptable because this is a Save/reboot/Load compatibility boundary.

## 9. Finding E — Phrase Core is already a persisted mutation domain

Phrase is not a future feature stub.

Current `src/phrase/*` provides:

- A/B/C/D Phrase slots;
- stable Phrase IDs and parent IDs;
- 1/2/4/8-bar lengths;
- MAIN / VARIATION / BREAK / END roles;
- capture from Song;
- derived reference views;
- insert/replace back into Song;
- Pattern-reference previews;
- persistence and sanitization.

`PhraseWorkspace` wraps Phrase mutations in the repository's synchronous AudioGuard convention and advances Scene revision once after success.

`insertIntoSong()` validates capacity before the first write and shifts complete `SongPosition` rows, preserving non-Phrase lanes.

Therefore Song/Phrase Live Arrangement would necessarily mutate the same persistent material graph as Generation Compatibility. This supports the shared release-track hypothesis.

## 10. Finding F — the live arranger is partially present already

Song already distinguishes:

- **edit slot**: Scene active Song slot A/B;
- **playback slot**: MiniAcid runtime Song playback slot A/B.

`SongPage` exposes both (`E:A/B`, `P:A/B`) and has a runtime LiveMix mode. `Ctrl+B` can switch the playback slot independently from the edit slot.

The important limitation is semantic: `MiniAcid::setSongPlaybackSlot()` applies the new slot immediately and resets Song bar state. It is not queued to a bar or Song-row boundary.

By contrast, Song reverse already has a pending model:

```text
queueSongReverseToggle()
        |
        v
pending while PLAY + Song mode
        |
        v
advanceSongBar_()
        |
        v
SongCycleBoundary.advanceRow
        |
        v
apply reverse once
```

So the repository already has both halves needed for live arrangement:

- independent edit/play material;
- a musical-boundary activation precedent.

The missing work is to give live material activation a coherent contract rather than adding more one-off booleans.

## 11. Finding G — generated Phrase -> Song exposes the current gap

`PhrasePage::generatePhraseToSong()` rejects generation while transport is playing and shows `STOP PLAYBACK FOR PHRASE`.

The underlying path uses:

- `GeneratedPhraseSong` for current Genre/Atlas/procedural + strong-rhythm generation;
- `PhraseGenerator` for 1/2/4/8 bar role structure;
- contiguous Pattern allocation;
- Song row writes;
- a synchronous guarded commit;
- one Scene revision advance on success.

This path is functional while stopped, but it has no pending activation lifecycle during playback.

It also uses a different storage policy from `SongPatternMaterializer`: generated phrases require contiguous empty Pattern slots, whereas Song-cell generation can safely reuse Pattern slots marked as Song-generated.

0.9.9 should not hide this difference behind a large generic allocator. The research conclusion is to unify **transaction and activation semantics first**, while preserving specialized allocation policies until evidence supports consolidation.

## 12. 0.9.8 dependency — hard ownership rule

The current Song page already contains a private one-level Undo implementation:

- `UndoHistory`;
- `UndoCell` vectors;
- Paste/Cut/Delete snapshots;
- direct restoration through Song mutation calls.

Song double-tap generation also has a separate provisional rollback receipt containing old Pattern reference, Song length, and a Scene revision snapshot.

These are existing local mechanisms, not a license for 0.9.9 to add a third one.

### Required 0.9.8 relationship

0.9.8 must own the canonical persistent mutation/Undo semantics. After 0.9.8 freezes, 0.9.9 should consume that contract for logical actions such as:

- generate one Song cell;
- generate a Song row;
- generate a multi-bar phrase;
- write/insert a Phrase into Song;
- replace prepared material at a musical boundary.

Research may specify that one logical action should become one undoable transaction. It must not invent the class/function that implements this before 0.9.8 exists.

### Explicit prohibition

Before 0.9.8 freezes, do **not** add:

- `GenerationUndoAdapter`;
- `ArrangementUndoShim`;
- a second Scene snapshot service;
- an 0.9.9-specific transaction history;
- a compatibility wrapper that 0.8.9 must later delete.

## 13. Proposed 0.9.9 conceptual boundary

The following names are **conceptual roles, not proposed C++ APIs**.

```text
Producer
  Genre / Feel / G
  Song G
  Phrase capture/derive/generate
  Manual arrangement
        |
        v
[Prepared Material]
  immutable candidate
  target identity
  no live Scene mutation
        |
        v
[Persistent Mutation]
  canonical owner = frozen 0.9.8 contract
  Pattern + Song + Phrase changes as one logical action
  exactly one persistent revision outcome
        |
        v
Committed Scene material
        |
        +--------------------------+
        |                          |
        v                          v
 Save / reboot / Load       [Activation Intent]
                             runtime-only by default
                             exact target + serial
                             explicit boundary
                                    |
                                    v
                             Musical Boundary
                             STOP immediate / next bar /
                             next Song row
                                    |
                                    v
                               Playback projection
```

### Required semantic rules

1. **Preparation is read-only.** Expensive generation and allocation planning happen before the real-time boundary and before persistent mutation.
2. **Persistent mutation is one logical transaction.** Pattern contents and the references that make them reachable must not become independently half-committed.
3. **Undo ownership comes from 0.9.8.** 0.9.9 does not own a competing history.
4. **Activation is separate from persistence.** Editing Song B while Song A plays can persist immediately while activation of B waits for an explicit musical boundary.
5. **Boundary execution is bounded.** AudioTask may copy/publish already prepared state; it must not run Atlas/procedural generation, allocate heap, or scan large ownership graphs.
6. **Target drift is explicit.** A pending action either keeps an exact stable target or cancels; it must never silently retarget to whatever cursor/page is current later.
7. **Rapid intent has one defined policy.** Prefer current quantized generation semantics: newest unpublished intent wins, with no unbounded queue.
8. **Output Ownership is orthogonal.** Generated or rearranged notes must not reset 0.9.6 output mode/routing ownership.
9. **Device Profile projection is orthogonal.** After 0.9.7 integration, device capabilities may affect output projection, not musical-material identity.
10. **Pending runtime state is non-persistent by default.** Save/reboot/load restores committed material, not a half-finished future activation.
11. **Compatibility IDs are stable.** Existing persisted genre/recipe/material identities are not renumbered for internal cleanup.
12. **Pattern liveness has one documented graph.** Song references, Phrase references, generated ownership bits, and reclaim policy must have explicit semantics.

## 14. Merge vs split gates

The current evidence supports one release track, but the old 0.9.11 direction must be split back out if any hard gate fails.

### Gate S1 — 0.9.8 transaction expressiveness

After 0.9.8 freezes, verify that its canonical transaction can represent a logical mutation involving multiple Pattern slots plus Song/Phrase references without an 0.9.9-specific Undo shim.

**Fail -> split or revise 0.9.8 contract before production 0.9.9.**

### Gate S2 — activation independence

Verify that runtime live activation can remain a bounded transport concern while the persistent edit uses the common mutation contract.

If live arrangement requires a separate invasive rewrite of transport, clock, Song storage, and Scene ownership that cannot be isolated behind prepared material + activation intent, the former 0.9.11 should remain separate.

### Gate S3 — regression isolation

Each implementation PR must have an independently testable surface. If a single change must simultaneously rewrite Generation algorithms, Phrase storage, Song UI, persistence, and transport to be testable, the track is too coupled and must be split.

### Gate S4 — persistence compatibility

If safe live arrangement requires an incompatible Scene or PatternPaging format change that cannot be backward-decoded independently from Generation compatibility, split the persistence migration from live arrangement.

### Gate S5 — real-time budget

If live activation cannot be reduced to bounded publication of prepared state at a musical boundary and instead requires generation/allocation/scanning on AudioTask, do not merge the implementation paths.

## 15. Recommended production slicing after 0.9.8

This research does not create these branches yet.

### 0.9.9-A — compatibility and liveness contract

Tests/documentation first:

- persisted genre/recipe identity;
- Pattern address compatibility;
- Song-generated ownership-bit behavior;
- explicit Phrase-reference liveness semantics;
- Save/reboot/Load invariants;
- no Output Ownership reset.

No generator rewrite.

### 0.9.9-B — canonical prepare/commit semantics

Align the existing material producers with the frozen 0.9.8 transaction contract while keeping their specialized generation algorithms and allocation policies.

Targets include:

- quantized Generation STOP/PLAY semantic parity;
- SongPatternMaterializer logical transaction;
- GeneratedPhraseSong/PhraseGenerator mutation outcome;
- PhraseWorkspace writes.

The goal is common lifecycle semantics, not one giant generic generator.

### 0.9.9-C — live activation boundary

Introduce one bounded activation policy for prepared/committed material during PLAY.

Initial candidate semantics:

- stopped transport: immediate activation;
- current Pattern replacement: next bar, preserving current quantized Generation behavior;
- Song arrangement/play-slot activation: next Song-row boundary;
- newest unpublished intent wins;
- target mismatch cancels rather than retargets.

Exact semantics require host tests before UI exposure.

### 0.9.9-D — Phrase/Song live arrangement wiring

Use existing edit/play slot separation and Phrase generation/write functionality. Remove STOP-only limitations only where 0.9.9-C proves a safe activation contract.

Do not make Phrase UI itself the transport owner.

### 0.9.9-E — UI and compatibility cleanup

Only after the lifecycle is tested:

- expose pending/committed/cancelled state consistently;
- remove obsolete compatibility bridges whose replacement is proven;
- keep old persistence decode paths where required;
- update help/docs.

## 16. Required host regression matrix

No tests are added by this research-only PR. These are the minimum tests for production 0.9.9 slices.

### Generation publication

- PLAY: generated material does not alter the currently sounding bar before boundary.
- PLAY: exact candidate publishes once at next bar.
- target page/bank/slot changes before publication -> candidate cancels.
- repeated request before boundary -> newest unpublished intent wins.
- STOP: equivalent logical action commits immediately.
- Scene revision advances exactly once for one successful logical persistent mutation.

### Pattern ownership/liveness

- manual/imported non-empty Pattern is never reclaimed by Song generation.
- uniquely-owned Song-generated Pattern can be safely reused according to current contract.
- orphan Song-generated Pattern can be reclaimed according to current contract.
- Phrase reference liveness behavior is tested according to the explicit S1 decision, not left accidental.
- repeated Song G does not leak Pattern slots.

### Song/Phrase atomicity

- failure to allocate all required material leaves Pattern data and Song/Phrase refs unchanged.
- Phrase insert overflow leaves Song unchanged.
- Phrase replace/insert is one logical mutation.
- multi-bar generated phrase failure cannot expose a partial arrangement.
- full logical action produces one 0.9.8 Undo record after that contract exists.

### Live arrangement

- edit slot can change while a different playback slot sounds.
- requested playback material does not cut into the middle of the current defined musical unit.
- boundary activation happens exactly once.
- rapid A/B activation requests have deterministic newest-intent semantics.
- pending activation target can be cancelled without a persistent phantom mutation.
- disabling LiveMix returns to the documented edit/play ownership rule.

### Persistence / reboot

- committed generated material survives Save/reboot/Load.
- PhraseBank identity and references survive Save/reboot/Load.
- 0.9.6 Output Ownership survives the same roundtrip untouched.
- once 0.9.7 is integrated, Device Profile selection/projection survives according to its own contract and does not alter musical material.
- pending generation/activation runtime state does **not** resurrect after reboot unless a later explicit contract says otherwise.
- legacy Scenes continue decoding with stable enum/recipe identities.

## 17. Hardware assumptions and future acceptance

This PR is docs-only and changes no firmware behavior, so it requires no new hardware flash acceptance.

For later production 0.9.9 hardware work, the relevant target remains Cardputer ADV with the existing repository audio/MIDI setup. No new I2C/SPI peripheral assumption is introduced by this design.

The critical hardware checks will be musical timing rather than peripheral wiring:

- no audible mid-bar Pattern tear when generation is requested during PLAY;
- no stuck notes when live arrangement changes playback material;
- exact boundary behavior visible in screen status and serial diagnostics;
- edit slot remains editable without disturbing the sounding slot before activation;
- Save/reboot/Load restores committed material only.

## 18. Non-goals

0.9.9 research does **not** propose:

- a rewrite of Genre, Feel, Atlas, or Vocabulary generation algorithms;
- a new Scene schema merely to make architecture look uniform;
- a new persistent GenerationBackend selector;
- an unbounded pending-action queue;
- heap allocation or generation on AudioTask;
- a replacement for 0.9.8 Undo ownership;
- temporary Undo shims;
- merging 0.9.7 or unfinished 0.9.8 production code into this research branch;
- changes to 0.9.6 Output Ownership semantics;
- Sampler work from deferred 0.9.5.

## 19. Rebaseline rule before production

Before the first production 0.9.9 PR:

1. identify the exact merged heads for 0.9.7 and 0.9.8;
2. confirm the 0.9.8 mutation/Undo contract and its tests;
3. compare those heads against this research baseline for all paths listed in section 4;
4. update the 0.9.9 architecture document if ownership changed;
5. do not copy this research branch's base history over newer merged release lines;
6. begin production work from the then-current integrated release head.

If 0.9.8 changes the mutation boundary materially, that newer contract wins over conceptual names in this document.

## 20. Research acceptance checklist

- [x] Exact latest merged production baseline identified: `0b284d1fdb6596957ffa998b0d5effcc27d761ca`.
- [x] 0.9.4 checked but not treated as sole architecture truth.
- [x] 0.9.6 production merge state verified.
- [x] 0.9.7 unmerged stacked state verified.
- [x] 0.9.8 confirmed not frozen/integrated.
- [x] Generation/Song/Phrase deltas checked against current later branches.
- [x] Current Generation pending-publication owner identified.
- [x] Current Song Pattern allocation/ownership path identified.
- [x] Current Phrase persisted mutation path identified.
- [x] Existing live edit/play Song-slot separation identified.
- [x] Existing musical-boundary Song reverse precedent identified.
- [x] Existing local Song Undo owner identified and explicitly excluded from 0.9.9 expansion.
- [x] Save/reboot/Load compatibility surface identified.
- [x] Common ownership boundary confirmed at release-track level.
- [x] Monolithic production implementation rejected.
- [x] Hard split gates recorded.
- [x] No production code changed.
- [x] No temporary Undo API/shim added.

## 21. Final roadmap recommendation

Use the provisional release theme:

> **0.9.9 — Generation Revision, Compatibility & Live Arrangement**

with the architectural subtitle:

> **Musical Material Lifecycle**

Treat the old `0.9.11 — Song / Phrase Live Arrangement` as **provisionally absorbed**, because current code proves a genuine common lifecycle boundary.

Do **not** interpret that as permission for one broad implementation PR. The correct delivery model is a sequence of compatibility, transaction, activation, and UI slices, all rebased on the frozen 0.9.8 mutation/Undo owner.

If any split gate in section 14 fails after 0.9.8 integration, restore Song/Phrase Live Arrangement as a separate release track rather than forcing architectural uniformity.