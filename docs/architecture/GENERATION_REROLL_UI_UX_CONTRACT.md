# Generation reroll identity and UI/UX contract

**Status:** proposed follow-up contract; documentation only

**Stack base:** `agent/20260812-genre-reroll-consistency` @ `93da75a6b60b2f5167df47c615eb8c88cfa2178c`

**Frozen parent:** PR #254 @ `8a70ffd3aa55b9779a0a9244220a525c7c60598c`

**Scope:** remove ambiguity from generation commands and status, keep local rerolls inside the active musical identity, and make the 240x135 layout collision-free.

**Implementation status:** no production change is authorized by this document.

---

## 1. Problem statement

Hardware acceptance proved that reroll publication, attempt ownership and quantized commit work. The follow-up SDL review exposed a different class of problem:

1. A local Synth Notes reroll can be technically accepted without a visible or reliably audible indication that anything changed.
2. The screen does not clearly distinguish a browsed Genre from the Genre that currently owns generated material.
3. Parent overlays and tab chrome can cover child-page controls and content.
4. SDL still has a legacy plain-`O` randomizer whose musical semantics do not match the release `G` contract.
5. The current interfaces make it too easy for request scope or a local address to leak back into composition/archetype selection.

The bug fixed by `93da75a6` demonstrated the last risk directly: a Synth-specific bank/slot address replaced the full-material `patternAddress`, and that value later became `phraseOrdinal`. Local generation could therefore select another composition branch despite retaining the same Genre, recipe and P-level.

The required direction is not “more randomness.” It is one stable musical identity, explicit variation scope, bounded novelty and unambiguous feedback.

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

- Which Genre/recipe/P-level identity is active now?
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
- changing the note-entry meaning of the `G` key;
- redesigning the hardware keyboard or adding touch interaction;
- introducing heap allocation or another live `Scene` copy in a realtime path.

## 5. Normative vocabulary

### 5.1 Active and pending selection

- **Active identity** is the Genre/recipe/P-level and full-material address currently owned by the Scene and used by generation.
- **Pending selection** is a browsed UI value that has not yet been applied.
- A pending selection MUST NOT silently affect local reroll.
- The UI MUST NOT label a pending selection as active.

### 5.2 Generation identity

`GenerationIdentity` is the stable upstream musical context from which variations are realized. The exact C++ representation may stay split across fixed-capacity values, consistent with `GENERATION_COMPOSITION_MODEL.md`; the name below describes the interface boundary, not a mandatory God object.

Conceptually it contains or references:

```text
Genre + recipe/variant + P-level
full-material patternAddress / phraseOrdinal
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
  scope: Full | SynthA | SynthB | Drums
  attemptOrdinal
  current material snapshot/references
  publication target token
```

The current shared tuple-local attempt stream is normative:

- Full Genre `G` and Synth Notes `G` use the same full-material tuple key;
- an accepted Synth reroll consumes the next ordinal in that tuple;
- scope may salt downstream realization only after stable identity resolution;
- scope may never select a different phrase/composition identity.

### 5.4 Commit scope

Generation scope and commit scope are explicit and independently testable:

| Request | Material that may be committed | Material that must remain byte-equivalent |
|---|---|---|
| Full | Synth A + Synth B + Drums | unrelated Scene state |
| Synth A | Synth A only | Synth B + Drums |
| Synth B | Synth B only | Synth A + Drums |
| Drums | Drums only | Synth A + Synth B |

While stopped, an accepted result may commit immediately. During PLAY, publication remains atomic at `BAR_START`. Cancellation or target invalidation never redirects a result to a new address, and an already accepted request still consumes its ordinal.

## 6. Command contract

Plain `G` is contextual but never ambiguous:

| Context | Plain `G` | Identity | Commit scope |
|---|---|---|---|
| GENRE | Full reroll | active or explicitly applied full identity | A + B + Drums |
| Synth A Notes | Voice reroll | active full identity | Synth A only |
| Synth B Notes | Voice reroll | active full identity | Synth B only |
| Drums | Drum reroll | active full identity | Drums only |
| Note entry | note `G` | not a generation command | current edited note |

Requirements:

- `G` on GENRE produces the next accepted full reroll of the active tuple.
- `G` on Synth Notes produces a new realization of only the selected synth inside the active tuple.
- The page from which `G` is pressed may change the commit scope, not the identity.
- SDL and embedded release builds expose the same command semantics.
- Plain `O` MUST NOT invoke an SDL-only musical randomizer in release behavior.
- If a broad/chaos randomizer is retained for development, it MUST require an explicit modifier or debug build and identify itself as `CHAOS`, never as ordinary reroll.

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
- Apply promotes pending to active using the chosen Apply mode.
- Local Synth/Drum `G` uses active identity only.
- After successful Apply, the active label updates and the pending marker clears.

### UI-04 — operation feedback

Every generation request exposes a small state machine:

```text
ACCEPTED -> PENDING BAR -> COMMITTED
         -> CANCELLED
         -> FAILED
         -> VALID UNCHANGED
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
B REROLL #8  TECHNO     UNCHANGED
```

- Feedback persists long enough to be read and is not hidden beneath the global HUD.
- `PENDING BAR` remains visible until commit, cancellation or replacement.
- A successful commit briefly highlights meaningful changed steps/events on the selected grid.
- An unchanged but valid candidate MUST NOT be reported as a visibly new generated pattern.

## 8. Generation architecture requirements

### GEN-01 — one identity boundary

The generation flow is ordered as:

```text
resolve active tuple
  -> resolve/reuse stable musical identity
  -> allocate accepted attempt ordinal
  -> realize requested variation inside identity
  -> validate novelty and invariants
  -> publish through explicit commit scope
```

No page-local bank/slot address may replace the full-material `patternAddress` before composition resolution. A regression gate must continue to reject `synthPatternAddressFor()` or `allocateSynthAttemptFor()` in this path.

### GEN-02 — bounded novelty

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

## 9. Realtime and resource contract

All implementation PRs must preserve:

- no heap allocation on request, realization, cancellation or commit paths;
- no recursion or unbounded candidate search;
- fixed-size request/candidate/status storage;
- no additional persistent live `Scene` copy;
- complete Full commit or single-role commit, never a partially published Full result;
- PLAY publication only at `BAR_START`;
- immediate STOP behavior;
- normal Cardputer ADV, fixed-DRAM and SEQTRAK MIDI-only builds;
- existing watchdog, stack and DRAM margins unless a reviewed budget explicitly replaces them.

## 10. Implementation slices

Do not implement this document as one broad PR. Recommended order:

### P0-A — layout containment

- introduce explicit parent/child viewports;
- move Synth tabs, Feel/mute status and footer into reserved regions;
- shorten contextual footer copy;
- add 240x135 layout/source regressions.

### P0-B — state and command clarity

- show active versus pending Genre;
- expose generation request state and scope;
- remove or debug-gate the SDL plain-`O` fallback;
- keep desktop and embedded key semantics identical.

### P1-A — stable identity request boundary

- make identity input explicit without creating a large mutable God object;
- make `VariationRequest` and commit scope inspectable in host tests;
- retain the address/attempt correction already present in `93da75a6`.

### P1-B — bounded novelty

- add role-aware musical distance metrics;
- add fixed internal candidate derivation;
- distinguish committed change from valid unchanged output;
- verify deterministic corpus and resource limits.

### P2 — retained-neighbor realization

- materialize only the requested semantic role against current retained material;
- remove disposable neighboring candidates if equivalent compatibility can be preserved;
- re-audit stack/DRAM before embedded acceptance.

Each slice is independently revertible. F-03 remains a separate feature stack.

## 11. Acceptance gates

### 11.1 Host/source regression

- [ ] Full and local requests for the same tuple expose identical identity fields.
- [ ] Scope cannot affect composition/archetype selection or phrase ordinal.
- [ ] No Synth-specific address/attempt allocator re-enters the composition path.
- [ ] Synth A commit leaves Synth B and Drums byte-equivalent.
- [ ] Synth B commit leaves Synth A and Drums byte-equivalent.
- [ ] Drum commit leaves Synth A and Synth B byte-equivalent.
- [ ] Full commit remains atomic A+B+Drums.
- [ ] Accepted, cancelled and evicted attempts preserve the #254 ordinal contract.
- [ ] Reboot/reset starts a tuple again at attempt 0.
- [ ] Note-entry `G` remains a note.
- [ ] SDL plain `O` cannot invoke a release randomizer.
- [ ] Bounded-novelty tests cover sparse and dense roles without violating identity invariants.

### 11.2 SDL visual/interaction acceptance

- [ ] Synth tabs do not cover the bank/pattern selector.
- [ ] Feel/mute HUD does not cover the Synth grid or Genre text.
- [ ] Footer text fits at 240x135 in every public theme.
- [ ] Browsing Genre shows old `ACTIVE` and new `PENDING` simultaneously.
- [ ] Apply promotes pending state visibly.
- [ ] Synth B `G` reports attempt, active Genre and `COMMITTED` or `NEXT BAR`.
- [ ] A committed local change highlights changed events briefly.
- [ ] `VALID UNCHANGED`, cancellation and failure are distinguishable.
- [ ] SDL and Cardputer key-help copy describe the same commands.

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
- [ ] Apply the Genre: active updates and pending clears.
- [ ] GENRE `G` creates successive full attempts inside the active identity.
- [ ] Synth A/B `G` audibly changes only the selected voice and remains stylistically related.
- [ ] Drums and neighboring synth are unchanged after a local Synth reroll.
- [ ] During PLAY, old material continues until the complete scoped commit at `BAR_START`.
- [ ] Repeated `G` has no watchdog, reboot, freeze or progressive degradation.
- [ ] Cancellation, more than 64 tuples and reboot retain the #254 behavior.
- [ ] No tab, HUD, status or footer text overlaps on the physical 240x135 display.
- [ ] Plain `O` does not expose a hidden release randomizer.

## 12. Success metrics

The follow-up is complete only when:

1. all UI collision checks pass at 240x135;
2. every generation action reports its actual scope and lifecycle state;
3. local rerolls preserve stable identity and untouched tracks exactly;
4. a profile matrix demonstrates bounded, perceptible variation without cross-Genre/archetype drift;
5. desktop and hardware expose one release command contract;
6. the full required CI matrix and hardware checklist pass on one unchanged SHA.

## 13. Pull request scope statement

Use the following summary when opening implementation PRs derived from this contract:

> Preserve one active full-material generation identity across GENRE, Synth and Drum entry points; make request/commit scope explicit; provide bounded and observable local variation; remove SDL-only release semantics; and reserve non-overlapping 240x135 UI regions. Do not modify #254, add F-03, redesign Genre vocabulary or broaden global randomness.
