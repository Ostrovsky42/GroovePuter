# Generation reroll identity and UI/UX contract

**Status:** proposed follow-up contract; documentation only

**Stack base:** `agent/20260812-genre-reroll-consistency` @ `93da75a6b60b2f5167df47c615eb8c88cfa2178c`

**Frozen parent:** PR #254 @ `8a70ffd3aa55b9779a0a9244220a525c7c60598c`

**Scope:** remove ambiguity from generation commands and status, keep local rerolls inside the active musical identity, and make the 240x135 layout collision-free.

**Implementation status:** no production change is authorized by this document.

**Release horizon:** P0 captures current-correctness and UI work eligible for 0.9.1. P1/P2, including bounded-novelty algorithms, retained-neighbor materialization and quantized Drums publication, are explicitly post-release.

---

## 1. Problem statement

Hardware acceptance proved that reroll publication, attempt ownership and current Full/Synth quantized commit work. The follow-up SDL review exposed a different class of problem:

1. A local Synth Notes reroll can be technically accepted without a visible or reliably audible indication that anything changed.
2. The screen does not clearly distinguish a browsed Genre from the Genre that currently owns generated material.
3. Parent overlays and tab chrome can cover child-page controls and content.
4. SDL still has a legacy plain-`O` randomizer whose musical semantics do not match the release `G` contract.
5. The current interfaces make it too easy for request scope or a local address to leak back into composition/archetype selection.

The bug fixed by `93da75a6` demonstrated the last risk directly: a Synth-specific bank/slot address replaced the full-material `patternAddress`, and that value later became `phraseOrdinal`. Local generation could therefore select another composition branch despite retaining the same Genre, recipe and P-level.

The immediate direction is not “more randomness.” For 0.9.1 it is one stable musical identity, explicit current scope and unambiguous feedback. Bounded novelty is a separate post-release algorithmic task.

## 2. Evidence from the SDL review

The review used the 240x135 SDL target at `93da75a6`, changed the active Genre from Acid to Synthwave, and exercised `G` and `O` on Synth B.

| Observation | User impact | Required response |
|---|---|---|
| Global Feel and mute overlays are painted over the lower content area after the page draws. | Lower grid rows and the final Genre explanation can be obscured. | Reserve layout space or compose overlays inside an explicit parent-owned viewport. |
| The Synth tab strip is drawn over the child Pattern Edit page's bank/pattern selector. | The user loses the address/context needed to understand which pattern is being edited. | Pass a reduced child viewport below the tabs; child pages must not assume the full content rectangle. |
| Several footer hints are clipped at 240 pixels. | Commands are incomplete precisely where the user needs confirmation. | Show only the primary contextual actions; move the complete map to Help. |
| Browsing a new Genre changes the displayed corridor before Apply, while the Scene still owns the old Genre. | A later Synth reroll can sound inconsistent even when it correctly uses the active Scene state. | Display `ACTIVE` and `NEXT/PENDING` explicitly. |
| Synth B `G` was accepted but the visible note/slide/accent topology appeared unchanged. | The user cannot distinguish success, a constrained collision, pending publication or ignored input. | Show scope, ordinal and request state; briefly mark changed steps. |
| Plain `O` invokes an SDL-only legacy `randomize303Pattern(1)` fallback and creates a much larger change. | Desktop rehearsal teaches a command and musical behavior that hardware does not promise. | Remove the release fallback or gate it behind an explicit debug/chaos modifier. |

This evidence is sufficient to define the contract. Temporary screenshots are diagnostic artifacts and are not part of the repository or acceptance baseline.

## 3. Product goal

A user should be able to answer all of these questions from the current screen and the resulting sound:

- Which Scene-owned Genre/recipe/material address is active, and which runtime P-level will the next request join to it?
- Is the highlighted Genre merely pending?
- What will plain `G` replace on this page?
- Was the request accepted, queued, committed, cancelled, rejected or unchanged?
- Did only the selected voice change?
- Does the result remain recognizably inside the active musical identity?

Success is a generation workflow that is predictable in scope but varied in realization.

## 4. Non-goals

The following are explicitly outside this follow-up:

- modifying or reopening frozen PR #254;
- mixing F-03 Atlas `P1 -> variant0 / P2 -> variant1 / P3 -> variant2` mapping into this work;
- adding Genres, changing the Genre catalogue or retuning every recipe;
- increasing global RNG strength as a substitute for identity ownership;
- persisting the session-only reroll attempt table;
- persisting P1/P2/P3 as Scene musical content;
- changing the current immediate Drums `G` path as part of the 0.9.1 P0 work;
- promoting GEN-02 or GEN-03 into the 0.9.1 release gate;
- changing the note-entry meaning of the `G` key;
- redesigning the hardware keyboard or adding touch interaction;
- introducing heap allocation or another live `Scene` copy in a realtime path.

## 5. Normative vocabulary

### 5.1 Active and pending selection

- **Active material context** is the Scene-owned Genre/recipe combined with the current full-material address.
- **Current P-level** is runtime/session request state. It is read and joined to the active material context when a generation request is accepted; it is not Scene musical content and is not persisted.
- **Pending selection** is a browsed UI value that has not yet been applied.
- A pending selection MUST NOT silently affect local Synth or Drum reroll.
- On GENRE, plain `G` is the explicit exception: if pending differs from active, it applies pending and requests a full reroll in one action.
- The UI MUST NOT label a pending selection as active.

### 5.2 Generation identity

`GenerationIdentity` is the stable upstream musical context from which variations are realized. The exact C++ representation may stay split across fixed-capacity values, consistent with `GENERATION_COMPOSITION_MODEL.md`; the name below describes the interface boundary, not a mandatory God object.

Conceptually it contains or references inputs from two ownership domains:

```text
Scene/material context:
  Genre + recipe/variant
  full-material patternAddress / phraseOrdinal
Runtime request context:
  current P-level
Resolved musical identity:
  rhythm and composition/archetype identity
  progression, phrase law and tonal/harmony context
  semantic voice-role assignments
```

The identity is resolved before request scope. `SynthA`, `SynthB` or `Drums` MUST NOT participate in composition/archetype selection.

### 5.3 Variation request

A request makes the transformation explicit:

```text
VariationRequest
  identity
  scope: Full | SynthA | SynthB  // current quantized scopes
  attemptOrdinal
  current material snapshot/references
  publication target token
```

The current shared tuple-local attempt stream is normative:

- Full Genre `G` and Synth Notes `G` use the same full-material tuple key;
- an accepted Synth reroll consumes the next ordinal in that tuple;
- scope may salt downstream realization only after stable identity resolution;
- scope may never select a different phrase/composition identity.

Current Drums `G` also uses the active Genre context, runtime P-level and session attempt allocation, but it remains a separate immediate `regenerateDrumsWithStrongRhythmMigration()` path. `Drums` is not a member of the current `QuantizedGenerationScope`. Adding it to the unified request/publication layer is a post-release behavior change.

### 5.4 Commit scope

Current-base generation and publication behavior is explicit and independently testable:

| Request | Material that may be committed | Material that must remain byte-equivalent | Current publication |
|---|---|---|---|
| Full | Synth A + Synth B + Drums | unrelated Scene state | STOP immediate; PLAY atomic at `BAR_START` |
| Synth A | Synth A only | Synth B + Drums | STOP immediate; PLAY atomic at `BAR_START` |
| Synth B | Synth B only | Synth A + Drums | STOP immediate; PLAY atomic at `BAR_START` |
| Drums | Drums only | Synth A + Synth B | immediate under `AudioGuard`, including during PLAY |

The `BAR_START`, target-cancellation and pending-publication contract is normative for current quantized scopes `Full`, `SynthA` and `SynthB`. Drums does not silently inherit that contract. A future quantized Drums scope must be proposed, implemented and accepted separately.

## 6. Command contract

Plain `G` is contextual but never ambiguous:

| Context | Plain `G` | Identity | Commit scope |
|---|---|---|---|
| GENRE, pending differs | Apply + full reroll | pending Genre/recipe requested, joined with current P-level/address and made active on commit | A + B + Drums |
| GENRE, no pending diff | Full reroll | next attempt of active tuple | A + B + Drums |
| Synth A Notes | Voice reroll | active full identity | Synth A only |
| Synth B Notes | Voice reroll | active full identity | Synth B only |
| Drums | Drum reroll | active full identity | Drums only, current immediate path |
| Note entry | note `G` | not a generation command | current edited note |

Requirements:

- If GENRE pending differs from active, plain `G` calls the apply-and-regenerate path: pending settings become the requested settings and a full reroll is requested.
- If GENRE has no pending difference, plain `G` produces the next accepted full reroll of the active tuple.
- `G` on Synth Notes requests the next realization of only the selected synth inside the active tuple.
- A local page may change commit scope, but MUST NOT replace the active musical identity with a page-, synth-bank- or synth-slot-specific identity.
- During PLAY, Full and Synth requests keep rendering the current bar and publish only at `BAR_START`; current Drums `G` remains the documented immediate exception.
- P1/P2/P3 selects how the next request realizes its vocabulary. Cycling P-level is not reroll, does not generate material and is not persisted.
- Where exposed, `Alt+G` is the separate explicit `CHAOS` operation. It is outside P1/P2/P3 and the tuple-local reroll contract.
- Historical MORPH fields remain decode-compatible only. MORPH is not a user reroll control and does not select the attempt.
- SDL and embedded release builds expose the same command semantics.
- Plain `O` MUST NOT invoke an SDL-only musical randomizer in release behavior.
- Any broad randomizer MUST use the explicit `Alt+G`/debug `CHAOS` surface, never plain `G`, plain `O` or P-level.

## 7. UI/UX requirements

### UI-01 — parent/child layout ownership

Each page receives an explicit drawable viewport after parent chrome is reserved:

```text
header
page tabs / parent chrome
child content viewport
operation status
footer
```

- Parent tabs, Feel HUD, mute state and footer MUST NOT paint inside the child viewport unless the child explicitly allocates that region.
- Synth tabs MUST sit above, not on top of, the Pattern Edit bank/pattern selector.
- The lower Synth grid and the final Genre explanation MUST remain readable with Feel and mute status visible.
- Layout acceptance is performed at the logical 240x135 resolution in every public theme.

This requirement should be implemented through geometry ownership, not page-specific text offsets.

### UI-02 — footer priority

The footer is a contextual prompt, not a complete key map.

- It shows the primary action and at most the secondary action that fits without clipping.
- Full bindings remain available through page-aware Help.
- Text MUST fit at 240 pixels without ellipsis, truncation or collision.
- Labels use the same verb as the operation status: `FULL REROLL`, `VOICE REROLL`, `DRUM REROLL`, `APPLY`.

### UI-03 — active versus pending Genre

Browsing and applying are two visible states. A suitable compact form is:

```text
ACTIVE  ACID
NEXT    SYNTHWAVE  [PENDING]
ENTER:APPLY   G:APPLY+REROLL
```

Exact copy may be shortened for the display, but both identities and the pending marker are required when they differ.

- Left/Right browsing changes only pending selection.
- Enter promotes pending using the chosen Apply mode.
- GENRE plain `G` promotes pending and requests a full reroll; with no pending difference it requests the next active-tuple reroll.
- Local Synth/Drum `G` uses active identity only.
- After a successful immediate Apply/commit, or after a pending full PLAY commit reaches `BAR_START`, the active label updates and the pending marker clears.

### UI-04 — operation feedback

Every current quantized Full/Synth request exposes a small state machine:

```text
ACCEPTED -> PENDING BAR -> COMMITTED
         -> CANCELLED
         -> FAILED
```

The visible message contains, as space permits:

```text
scope + attempt + active Genre + result
```

Examples:

```text
B REROLL #2  SYNTHWAVE  COMMITTED
FULL #4      ACID       NEXT BAR
A REROLL #7  LOFI       CANCELLED
```

- Feedback persists long enough to be read and is not hidden beneath the global HUD.
- `PENDING BAR` remains visible until commit, cancellation or replacement.
- Current Drums `G` reports immediate success/failure and MUST NOT claim `PENDING BAR` until a separate quantized Drums change is implemented.
- A successful commit briefly highlights meaningful changed steps/events on the selected grid when such visible changes exist.
- P0 feedback reports request lifecycle, not a novelty guarantee. `VALID UNCHANGED` belongs to the post-release GEN-02 result model.

## 8. Generation architecture requirements

### GEN-01 — one identity boundary

The generation flow is ordered as:

```text
resolve command intent
  -> on GENRE, promote pending when it differs; otherwise use active context
  -> on local pages, use active Scene/material context only
  -> join the current runtime/session P-level
  -> resolve/reuse stable musical identity
  -> allocate accepted attempt ordinal
  -> realize requested variation inside identity
  -> validate current identity/target invariants
  -> publish through explicit commit scope
```

No page-local bank/slot address may replace the full-material `patternAddress` before composition resolution. A regression gate must continue to reject `synthPatternAddressFor()` or `allocateSynthAttemptFor()` in this path.

### GEN-02 — bounded novelty

**Release classification:** post-0.9.1. This is not a current release gate and requires a separate algorithmic PR.

A reroll has both a lower and an upper distance bound.

The upper bound preserves:

- Genre/recipe/P-level;
- rhythm/composition/archetype identity;
- progression, phrase law and cadence class;
- semantic role and legal register/harmony corridor;
- required rhythmic anchors and protected space.

For a non-empty selected synth, the lower bound should normally change at least two meaningful events when the legal space permits. Meaningful dimensions include:

- onset or rest state;
- pitch;
- duration/gate;
- accent/velocity class;
- slide/articulation.

A metric MUST compare musical event data, not screen pixels or raw RNG state. Thresholds may be role- and density-aware; a sparse one-note role must not be forced to violate its grammar merely to reach an arbitrary count.

When a generated candidate is below the lower bound:

1. derive up to a small fixed number of internal candidates from the already accepted ordinal;
2. do not consume additional public attempt ordinals;
3. keep all candidates inside the same identity;
4. if none meets the bound, apply a deterministic legal mutation or return `VALID UNCHANGED`.

The retry count and storage are fixed at compile time. There is no unbounded search, scoring animation or heap allocation.

### GEN-03 — local reroll compatibility

**Release classification:** post-0.9.1. Direct retained-neighbor materialization is not required to ship the current identity fix.

A Synth-only reroll is evaluated against the material that will remain in the Scene:

- current Drums;
- the retained neighboring synth;
- current harmony/phrase constraints;
- the selected synth's semantic role.

The long-term materializer should realize the selected role directly against those retained constraints. Generating a disposable full A+B+Drums candidate and publishing one component is acceptable only while tests prove equivalent compatibility and fixed resource bounds.

### GEN-04 — deterministic ownership

- The tuple attempt table remains session-only, fixed-capacity and deterministic.
- First accepted request for a tuple remains attempt 0.
- More than 64 distinct tuples never disable generation; evicted history may restart at attempt 0.
- Target cancellation consumes an accepted ordinal but publishes nothing.
- Reboot/reset clears reroll history.
- Persistence decode never regenerates material implicitly.
- P-level and the attempt table remain runtime/session-only; neither is Scene/NVS/project state.
- P1/P2/P3, tuple reroll and `Alt+G CHAOS` remain three distinct operations.
- Historical MORPH fields never become an attempt selector again.

## 9. Realtime and resource contract

All implementation PRs must preserve:

- no heap allocation on request, realization, cancellation or commit paths;
- no recursion or unbounded candidate search;
- fixed-size request/candidate/status storage;
- no additional persistent live `Scene` copy;
- complete Full commit or single-Synth commit, never a partially published Full result;
- PLAY publication for current quantized Full/Synth scopes only at `BAR_START`;
- current Drums `G` remains immediate until a separate quantized Drums change is accepted;
- immediate STOP behavior;
- normal Cardputer ADV, fixed-DRAM and SEQTRAK MIDI-only builds;
- existing watchdog, stack and DRAM margins unless a reviewed budget explicitly replaces them.

## 10. Implementation slices

Do not implement this document as one broad PR. Release classification is normative.

### 10.1 0.9.1 / P0 — current correctness and UI clarity

#### P0-A — layout containment

- introduce explicit parent/child viewports;
- move Synth tabs, Feel/mute status and footer into reserved regions;
- shorten contextual footer copy;
- add 240x135 layout/source regressions.

#### P0-B — state and command clarity

- show active versus pending Genre;
- expose generation request state and scope;
- remove or debug-gate the SDL plain-`O` fallback;
- keep desktop and embedded key semantics identical.

#### P0-C — frozen semantic regressions

- preserve `93da75a6` full-material address and shared attempt-stream correction;
- pin pending GENRE `G` as apply + full reroll and no-pending `G` as next active attempt;
- pin Synth-only scope, P-level session ownership, MORPH retirement and explicit `Alt+G CHAOS` separation;
- document current immediate Drums behavior without changing it.

### 10.2 Post-release / P1-P2 — new algorithms and publication behavior

#### P1-A — stable identity request boundary

- make identity input explicit without creating a large mutable God object;
- make `VariationRequest` and commit scope inspectable in host tests;
- retain the address/attempt correction already present in `93da75a6`.

#### P1-B — bounded novelty

- add role-aware musical distance metrics;
- add fixed internal candidate derivation;
- distinguish committed change from valid unchanged output;
- verify deterministic corpus and resource limits.

#### P2-A — retained-neighbor realization

- materialize only the requested semantic role against current retained material;
- remove disposable neighboring candidates if equivalent compatibility can be preserved;
- re-audit stack/DRAM before embedded acceptance.

#### P2-B — optional quantized Drums scope

- add Drums only through an explicit `QuantizedGenerationScope` design;
- preserve current synths byte-for-byte;
- define PLAY cancellation, supersession and `BAR_START` ownership;
- treat the behavior change as post-release, not a correction implied by this document.

Each slice is independently revertible. F-03 remains a separate feature stack.

## 11. 0.9.1 / P0 acceptance gates

### 11.1 Host/source regression

- [ ] Full and Synth-local requests for the same tuple expose identical identity fields.
- [ ] Scene/material context owns Genre/recipe/address; current P-level is joined from runtime/session request state.
- [ ] P-level is absent from Scene/project/NVS persistence and cycling it does not generate material.
- [ ] Scope cannot affect composition/archetype selection or phrase ordinal.
- [ ] No Synth-specific address/attempt allocator re-enters the composition path.
- [ ] Pending GENRE `G` applies pending settings and requests Full; no-pending GENRE `G` advances the active tuple.
- [ ] Synth A commit leaves Synth B and Drums byte-equivalent.
- [ ] Synth B commit leaves Synth A and Drums byte-equivalent.
- [ ] Current Drum `G` leaves Synth A and Synth B byte-equivalent and remains outside `QuantizedGenerationScope`.
- [ ] Full commit remains atomic A+B+Drums.
- [ ] Accepted, cancelled and evicted attempts preserve the #254 ordinal contract.
- [ ] Reboot/reset starts a tuple again at attempt 0.
- [ ] Note-entry `G` remains a note.
- [ ] `Alt+G CHAOS` remains separate from P-level and tuple reroll.
- [ ] MORPH is absent as a user reroll/attempt control.
- [ ] SDL plain `O` cannot invoke a release randomizer.

### 11.2 SDL visual/interaction acceptance

- [ ] Synth tabs do not cover the bank/pattern selector.
- [ ] Feel/mute HUD does not cover the Synth grid or Genre text.
- [ ] Footer text fits at 240x135 in every public theme.
- [ ] Browsing Genre shows old `ACTIVE` and new `PENDING` simultaneously.
- [ ] Enter applies according to Apply mode; pending GENRE `G` visibly means `APPLY+REROLL`.
- [ ] Synth B `G` reports attempt, active Genre and `COMMITTED` or `NEXT BAR`.
- [ ] A committed local change highlights changed events briefly.
- [ ] Accepted, pending, committed, cancellation and failure states are distinguishable.
- [ ] SDL and Cardputer key-help copy describe the same commands.
- [ ] Current Drum `G` never displays a false `NEXT BAR` state.

### 11.3 CI/build acceptance

- [ ] Core host regressions pass.
- [ ] Generation/persistence regression suites pass.
- [ ] SDL build passes.
- [ ] Cardputer ADV normal build passes.
- [ ] Cardputer ADV fixed-DRAM build passes.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] Required matrix is green on one immutable candidate SHA.

### 11.4 Hardware acceptance

- [ ] Select a new Genre: pending and active are visibly different before Apply.
- [ ] With pending Genre selected, GENRE `G` applies it and requests a Full A+B+Drums reroll.
- [ ] With no pending difference, repeated GENRE `G` creates successive attempts inside the active identity.
- [ ] Synth A/B `G` can change only the selected voice and keeps the active Genre/composition identity.
- [ ] Drums and neighboring synth are unchanged after a local Synth reroll.
- [ ] During PLAY, old material continues until a complete Full/Synth commit at `BAR_START`.
- [ ] Current Drums `G` remains immediate and is not represented as quantized publication.
- [ ] P1/P2/P3 changes request realization level but does not itself reroll.
- [ ] `Alt+G` is visibly separate `CHAOS`; MORPH is not exposed as reroll control.
- [ ] Repeated `G` has no watchdog, reboot, freeze or progressive degradation.
- [ ] Cancellation, more than 64 tuples and reboot retain the #254 behavior.
- [ ] No tab, HUD, status or footer text overlaps on the physical 240x135 display.
- [ ] Plain `O` does not expose a hidden release randomizer.

## 12. Post-release P1/P2 acceptance

These gates do not block 0.9.1:

- [ ] Bounded-novelty tests cover sparse and dense roles without violating identity invariants.
- [ ] A role-aware musical distance metric, fixed internal derivation and `VALID UNCHANGED` behavior are accepted on their own immutable SHA.
- [ ] Retained-neighbor realization proves compatibility with current Drums and the untouched synth.
- [ ] Any quantized Drums scope proves target cancellation, supersession and atomic `BAR_START` publication separately.
- [ ] Post-release resource changes pass a fresh stack/DRAM audit and the full embedded matrix.

## 13. Success metrics

The 0.9.1/P0 follow-up is complete when:

1. all UI collision checks pass at 240x135;
2. every generation action reports its actual scope and lifecycle state;
3. local rerolls preserve stable identity and untouched tracks exactly;
4. pending GENRE apply/reroll, runtime P-level, MORPH retirement and `Alt+G CHAOS` match the frozen code contract;
5. desktop and hardware expose one release command contract;
6. Full/Synth PLAY publication remains atomic at `BAR_START`, while Drums is accurately reported as immediate;
7. the full required CI matrix and hardware checklist pass on one unchanged SHA.

Post-release P1/P2 succeeds only after a separate profile matrix demonstrates bounded, perceptible variation without cross-Genre/archetype drift and retained-neighbor/resource gates pass.

## 14. Pull request scope statements

Use the following summary for 0.9.1/P0 implementation PRs:

> Preserve the existing full-material identity across GENRE and Synth entry points; distinguish pending GENRE apply+reroll from active-tuple reroll; preserve runtime-only P-level, session-only attempts, MORPH retirement and explicit `Alt+G CHAOS`; report the current immediate Drums path honestly; remove SDL-only release semantics; and reserve non-overlapping 240x135 UI regions. Do not add new generation algorithms or quantized Drums behavior.

Use the following only for separate post-release P1/P2 PRs:

> Preserve one active full-material generation identity while adding bounded, observable local variation, retained-neighbor constraints or an explicitly designed quantized Drums scope. Keep every algorithm fixed-capacity and realtime-safe. Do not modify #254, add F-03, redesign Genre vocabulary or broaden global randomness.
