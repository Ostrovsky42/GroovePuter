# GroovePuter 0.9.21 — Rhythm Browser and manual-first workflow

**Status:** approved direction; implementation and acceptance pending. **Baseline:** `release/0.9.20-ours` at `85b53e6b77bd167c23cf23fd78e0832ba9e6a0ef`, accepted by product owner. Further 0.9.21 features may be proposed separately. This document is subordinate to `PLAN.md` and records the approved Rhythm Browser contract; it does not assert that any feature is already shipped.

## Product outcome

From DRUMS a musician browses named rhythms by musical group, hears a deterministic candidate alone or with their own bass and lead, selects a genuine available variation, explicitly applies only the destination drums, and continues editing or recording. Cancel leaves accepted music intact. The interface must distinguish browsing, preview, pending boundary, applied, cancelled and unavailable states; it must not claim playback or application before either happens. UX speed targets are hypotheses until measured with a musician on Cardputer ADV.

## Boundaries and sources

- Use only renderable Rhythm Vocabulary archetypes and provenance-reviewed compiled Atlas runtime recipes. The 24 archetypes are grammars, not automatically 24 ready patterns; the six compiled recipes have P1/P2/P3 only when confirmed. Raw Atlas research data is not a firmware library. Do not invent playable entries or variants.
- Groups are discoverability tags, not genre IDs. Include ALL and omit empty groups; cross-genre browsing must remain possible. Browsing must not change Genre, Recipe, BPM, scale, FEEL, kit, synths or Song.
- Eligibility is 4/4 for the first release. Initial groups HOUSE, TECHNO, UKG, BREAKS, HIP-HOP, ACID, DUB and ELECTRO are candidates for audit, not promises of entries; show only groups with proven renderable members. One stable archetype may carry multiple tags.
- A session identifies a candidate by source, stable ID, variant, fixed preview seed and destination context version. Materialization is deterministic; exact Atlas events do not depend on seed. Catalog descriptors are read-only and include source, provenance/rights, available variants and renderability. Persist accepted physical material through existing owners; playback must not require catalog metadata.
- UMG, Looper, Energy FORM, intelligent rhythm matching, extra transport/scheduler, 8-bar generation and a new Scene database are outside this goal. The accepted 0.9.20 history already includes `af4d510`; later UMG experiment commits remain separate and are not to be merged by default.

## Interaction and sound contract

Browser is a DRUMS-local menu or modal with a visible entry point, avoiding a shortcut the musician must memorize. The product owner selected **Tab to start/stop audition while the Browser is open**; this replaces the earlier proposal that reserved Tab for group switching. Choose accessible group controls after the physical Cardputer and SDL input audit. Outside the Browser, DRUMS Tab and global Fn+Tab/Alt navigation retain their existing behavior. Show actual source, mode and destination, and offer physically reachable alternatives where necessary. Layout and hints must fit the device screen.

Preview is transient and read-only: materialize one bounded candidate on the control side, publish at an existing musical boundary, and reuse existing audio/transport ownership. Audition must honor the selected drum output mode and layout, including internal, MIDI and layered output where configured; no separate preview-only route may silently suppress MIDI. WITH SONG retains accepted synth material; SOLO is clearly distinct. No allocation, filesystem access, catalog scan or serialization in the audio ISR. Changing candidates during PLAY must preserve phase, MIDI ownership and note cleanup. Cancel, STOP, page/project changes, errors and ownership conflicts discard preview safely. Preview must not write Scene, Song, Undo, revision, material slots or cursor. A bounded architecture spike must prove the existing path supports this before a large UI implementation; a second transport or persistent schema change requires escalation.

Apply validates destination, revision, ownership and writability; warns before replacing manually edited drums; and uses the existing atomic transaction and one Undo. During PLAY publish at the next permitted bar boundary; when stopped commit immediately. Preserve Synth A/B, harmony, genre, BPM, FEEL, kit, MIDI routing and Song structure. Unsupported Song targets and conflicts must refuse explicitly with zero partial changes. Verify Undo, supported Redo, save and cold reload; do not create implicit slots or overwrite every Song bar.

## Milestones and release gates

One agent run is one milestone or a limited subset, with checkpoint evidence and a commit when complete. No reset/clean/rebase of the UMG experiment and no push to a release line without authorization.

1. **RB-0 — Freeze and baseline:** preserve tracked/untracked/sidecar evidence and hashes, isolate worktrees, record exact baseline host/build/memory/keymap evidence, and establish this goal. Do not conflate the candidate's historical evidence with 0.9.20.
2. **RB-1 — Catalog:** audit archetype renderability and Atlas rights/mapping; deterministic bounded materialization, stable IDs and focused host regressions. Do not claim all entries usable without proof.
3. **RB-2 — Discovery:** audit key ownership and implement truthful, accessible modal navigation and display; SDL and physical key matrix checks.
4. **RB-3 — Audition:** prove existing ownership and implement bounded solo and with-song preview, boundary swaps, conflict cleanup, non-mutation and MIDI regression tests. No hardware means HARDWARE BLOCKED.
5. **RB-4 — Apply:** explicit destination, atomic drums-only transaction, warning, Undo and supported Redo, refusal atomicity, save and cold reload.
6. **RB-5 — Acceptance:** human listening across representative groups, 0.9.20 regression, actual Cardputer screen/input/audio/MIDI/stress and measured memory/stack/timing comparisons. No listening means MUSICALLY UNVERIFIED.
7. **RB-6 — Release:** clean reproducible SHA/build and artifact hashes, documentation and provenance, demonstrations and human/hardware sign-off. Missing gates are blockers, not passes.

Core negative cases include repeated deterministic browsing, cancel with Scene/Song/Undo equivalence, failed or conflicting apply without mutation, manual drum edits, wrong Song destination, STOP/project switch cleanup, physical MIDI note cleanup and stable saved material under a changed generation seed. Existing Genre/Atlas, FEEL, Pattern, Song, MIDI, Synth and project load workflows must remain functional. Human acceptance is: “I made a bass line, found and heard a suitable rhythm with it, applied only drums and continued without loss.”

The acceptance matrix is mandatory, not evidence by declaration: A01 DRUMS discovery; A02 local Tab audition and global Fn+Tab isolation; A03 repeatable audible candidate; A04 exact Atlas variants without synth changes; A05 audible Vocabulary realization; A06 solo; A07 with existing bass/lead; A08 cancel equivalence including revision; A09 stopped drums-only apply and one Undo; A10 playing apply at boundary; A11 Undo; A12 invalid candidate refusal; A13 warning before replacing manual edits; A14 safe Song-target refusal; A15 STOP cleanup; A16 project-switch cleanup; A17 save/cold reload; A18 independence from future runtime seed; A19 output layout, USB/DIN and no stuck notes; A20 hardware stress and resource stability; A21 cross-genre selection without Genre change; A22 existing Atlas/Genre apply regression. Each requires evidence appropriate to its surface, including physical hardware and human listening where applicable.

Checkpoint reports identify milestone, base SHA, changed files, commit SHA, evidence and hashes, behavior actually verified, limitations, blockers/human decisions and the next smallest task.
