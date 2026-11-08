# GroovePuter 0.9.10 — UI Constitution V1

Status: architecture/product contract for the dedicated UI workstream.

Historical branch base at creation:

`465a1b1189ecb94fdc82a66ca0ec02de248609e2`

Branch:

`feature/20260906-04-0.9.10-ui-constitution-v1`

The SHA above is provenance, not a permanently valid verification pin. Every implementation checkpoint must re-read the actual remote HEAD and publish fresh evidence.

## Purpose

UI Constitution V1 is not a cosmetic redesign and is not an attempt to make GroovePuter look like another groovebox.

The goal is to make the existing interface a predictable embedded musical instrument in which presentation, navigation, engine truth, renderer lifetime, geometry and realtime rendering have explicit owners and cannot accidentally change one another's semantics.

The current GroovePuter UI is **not disposable legacy presentation**. It already contains useful visual identity, spatial habits, keyboard muscle memory, workflow organization and successful local interaction grammars.

The Constitution primarily replaces accidental architecture underneath those behaviours. It does not grant permission to replace the product form without a demonstrated UX reason.

## Hardware precedent

BKLVA Pocket DAW on Cardputer ADV is used as a hardware and interaction-discipline precedent, not as a visual specification.

The relevant lessons are:

- 240x135 is enough for a serious musical instrument when interaction grammar is strict;
- one musical object can have overview/focus representations without becoming separate applications;
- richer desktop-like views are not automatically better on a tiny screen;
- display-transfer bandwidth is part of the realtime audio/SD budget;
- playhead/cursor feedback must remain cheap;
- continuity and persistent state identity matter more than adding views;
- functionality should be omitted from a representation when it makes the device harder to use.

GroovePuter has a more complex musical state model than a conventional small DAW, so it needs at least the same degree of discipline.

## Preservation contract

**EXISTING PRODUCT FORM IS PRESUMED VALUABLE UNTIL PROVEN OTHERWISE.**

For early semantic/shell/continuity checkpoints:

**NO INTENTIONAL VISUAL REDESIGN UNLESS REQUIRED TO CORRECT A PROVEN SEMANTIC OR USABILITY DEFECT.**

Architectural cleanup alone is not justification for changing typography, tabs, field order, colour identity, theme identity, shortcuts, PERFORM organization, Synth local navigation or other established product form.

Before substantial changes, a major surface is classified as one of:

- `PRESERVE` — form and interaction are considered useful;
- `PRESERVE BEHAVIOR` — mental model/interaction must survive, later appearance changes may be considered separately;
- `KNOWN DEFECT` — a specific defect is proven, targeted change is permitted;
- `PROTOTYPE` — current implementation is not a product contract.

### Strong preservation targets

PERFORM:

- KEY / CHORD / ARP / RHYTHM local contexts;
- fixed context positions;
- remembered row per context;
- contextual disabled/N/A states;
- explicit important side effects;
- direct performance-keyboard semantics.

Synth A/B:

- distinct target identity;
- `NOTES / KNOBS / MORE` local structure;
- established local navigation unless a concrete conflict is proven.

FEEL / GENRE:

- field-list mental model;
- useful field grouping/order;
- recognizable product form;
- distinction between immediately audible and next-generation intent.

Product identity:

- Synth A / Synth B / Drums entity identity;
- existing theme character;
- useful compact visual vocabulary.

### Known defects / prototypes

The following are not preservation targets:

- semantic context inferred from presentation strings;
- semantic side effects from geometry propagation;
- PAT/PHR source mismatch;
- multiple owners of the same header/HUD/body pixels;
- continuity coupled to renderer residency;
- inaccessible Phrase bars 3–8;
- Phrase `eventIndex % rows` spatial layout;
- theme-dependent disappearance of mandatory information;
- the current experimental Phrase timeline as a final product renderer.

## Three kinds of heritage

Migration must separately protect:

1. **Visual heritage** — theme character, spacing, tabs, meters, waveforms, entity colours and recognizable GroovePuter identity.
2. **Interaction heritage** — arrows, Enter, Escape, Tab, transport, workflow shortcuts, Synth navigation, PERFORM interaction and Pattern editing gestures.
3. **Spatial heritage** — useful remembered positions and field order. A generic grammar is not allowed to rearrange product surfaces merely because it can.

Shared abstractions should live below product design. Prefer common focus/command/continuity semantics with product-specific renderers over one visual template for every field list or timeline.

**DISCIPLINE DOES NOT REQUIRE VISUAL UNIFORMITY.**

## Existing musical contracts remain authoritative

UI migration must preserve:

- Synth A/B Pattern/Phrase source semantics;
- one Phrase note = one runtime event;
- real cross-bar duration;
- Phrase length 1/2/4/8 bars;
- Pattern subdivision / Phrase length / GRID distinction;
- 96 PPQN and 384 ticks/bar;
- explicit one-way MAKE PHRASE;
- existing AudioMutationGate / atomic mutation discipline;
- accepted Undo ownership;
- Pattern compatibility firewall;
- transport/lifetime barriers;
- live/MIDI note separation.

The UI does not become a second musical authority.

## Semantic location

`page` is not the domain model for where the user is.

Runtime UI location should distinguish at least:

- `Workflow` — what class of task the user is doing;
- `Target` — which product/musical object is being worked on;
- `Surface` — which representation of that target is open.

Do not invent new product workflow names solely to satisfy the architecture.

Sequenced source is **not** navigation state. For Synth A/B, PATTERN/PHRASE comes from the authoritative engine source.

Transport/playback ownership is also not forced into the same enum as sequenced source. Song, SMF, Pattern and Phrase may describe different relationships and scopes.

**A SEMANTIC VALUE HAS EXACTLY ONE AUTHORITY.**

UI may copy engine truth into a snapshot. UI may not reconstruct engine truth from titles, page indices, workflow guesses, song-mode proxies, themes or other presentation state.

## Layout must be semantically pure

`setBounds(rect)` and equivalent geometry propagation may change geometry only.

They must not publish or mutate:

- current page/location;
- title-derived semantic context;
- workflow;
- target;
- source;
- global chrome state;
- navigation state.

Presentation text is not an API. Renaming a page/title must not change product behaviour.

Legacy persisted identities may enter through deserialization/migration, but canonical runtime UI should not continuously carry legacy aliases as its domain model.

## One frame = one truth

One displayed frame must derive from one coherent semantic state.

Chrome, body, performance strip and footer must not independently reconstruct live truth at different times in a way that permits a hybrid frame.

A small bounded semantic snapshot should contain identity/observability state, not a copy of all musical data. Surface material must also have a coherent read-only view/revision and a safe lifetime for composition.

Do not hold audio mutation locks during drawing or display transfer.

If a coherent new view is unavailable, preserving the previous coherent visual frame is preferable to combining new chrome with stale body data.

Input mutation still re-validates against current engine state; a displayed object is not permission to mutate stale state.

## Pixel ownership

Standard geometry is frozen as:

| Region | Y inclusive | Height | Owner |
|---|---:|---:|---|
| Global chrome | 0..15 | 16 | shell |
| Surface body | 16..108 | 93 | active surface |
| Performance strip | 109..118 | 10 | shell |
| Command footer | 119..134 | 16 | shell |

A standard field-list body has no more than seven full 12-pixel rows.

A normal surface must receive a genuinely clipped/bounded body rendering capability, not an unrestricted display pointer plus a comment telling it where not to draw.

Header/footer/performance-strip ownership belongs to the shell. Page/surface code supplies models, not a second competing global renderer.

Expanded/immersive modes are explicit capabilities for demonstrated needs, not hidden rectangle exceptions.

## Continuity is independent of renderer lifetime

Renderer/page eviction is a valid embedded-memory strategy. User context must not disappear because the renderer object was reclaimed.

A small bounded continuity state should retain only justified UX state such as:

- local surface selection;
- focused field;
- Pattern cursor/viewport;
- Phrase cursor/viewport/selection;
- PERFORM context row;
- target-specific continuity.

Size is measured, not guessed.

Renderer-eviction continuity and persistence across reboot are different contracts.

**RENDERER MAY DIE. USER CONTEXT MUST NOT DIE WITH IT.**

## Musical selection is not a buffer index

Mutable event-buffer position is not musical identity.

An unrelated edit must not move focus to another musical object merely because vector/array positions shifted.

Adapters must define deterministic selection reconciliation, including coincident events, selected-event edits, deletion, full material replacement and stale references.

Do not add persistent event IDs to the musical model without demonstrated need and measured cost; a revision-aware UI-local mapping is acceptable if it satisfies the behavioural invariant.

## Input is a semantic command system

Preserve current physical input normalization and bounded hold acceleration.

Above it, define commands with stable semantics:

- Up/Down — focus or explicitly spatial navigation;
- Left/Right — value change or spatial cursor movement;
- Enter — primary action;
- Tab — peer/local representation change;
- Escape — collapse/back;
- Space — transport;
- the actually verified workflow shortcut — workflow change.

Do not replace the real hardware shortcut with a theoretical one without checking all input backends.

One physical event performs one semantic command. Modal/local/global dispatch precedence is explicit.

Footer/help and input must use the same command authority. If the footer advertises a key, that key must execute that exact currently available command.

## Surface grammars, not feature mini-apps

Target a small number of repeated interaction grammars, for example:

- field-list grammar for GENRE/FEEL/parameters/settings;
- timeline grammar for melodic Pattern/Phrase editing;
- step/lane grammar for drums/discrete pattern work;
- performance grammar for direct playing.

This is direction, not permission to build a generic framework in advance. Introduce a reusable abstraction only when there is a real second consumer.

Shared grammar may own focus, commands, disabled semantics, viewport laws, selection laws and footer semantics while product-specific renderers preserve useful visual identity.

## Pattern/Phrase NOTES

Synth A/B remain one instrument target with the preserved local structure:

`NOTES / KNOBS / MORE`

PATTERN and PHRASE are authoritative sources of that target, not separate applications.

The common NOTES contract owns time orientation, cursor, viewport, selection grammar, playhead, common commands and footer semantics.

Pattern/Phrase adapters expose observable material, legal operations/read-only projections and existing authoritative mutation paths. They do not become new owners of validation, Undo or note lifetime.

Representation change does not change source. Source change does not implicitly convert material. MAKE PHRASE remains explicit.

Pattern and Phrase may have different visual representations while obeying the same interaction laws.

## Phrase visual language

The current prototype with a maximum two-bar window and `eventIndex % rows` is not a final design.

For Phrase:

- X means musical time;
- span width means real duration;
- onset differs visibly from continuation;
- a cross-bar note remains one musical object;
- bar boundaries are visible;
- continuation entering/leaving the viewport is represented;
- Y is used only when it has a defensible musical or deterministic visual meaning.

Do not add a desktop piano roll merely because it is familiar.

All 1/2/4/8 bars must be reachable. Prefer whole-object overview plus focused detail. Playback-follow behaviour is explicitly separate from edit-focus behaviour.

## NOW / NEXT TAKE / LAST TAKE

Generative surfaces should distinguish ownership in time:

- `NOW` — what is actually applied/sounding;
- `NEXT TAKE` — intent for the next generation action;
- `LAST TAKE` — the last accepted generated result when provenance is truthful.

Requested settings, preview/rejected candidates, accepted material and currently sounding material must not be conflated.

`TAKE` is initially product language, not automatically a new persisted domain entity/history system.

## Musician-facing vocabulary

Engineering vocabulary may remain in code, tests, logs and architecture docs. Product controls should describe decisions a musician would make.

Approved semantic direction:

| Internal | Product wording |
|---|---|
| generation request | NEXT TAKE |
| P1 canonical | CORE |
| P2 variation | VARIANT |
| P3 transformation | REWORK |
| materialize | MAKE TAKE / NEW TAKE |
| materialize + BPM | NEW TAKE + TEMPO |
| currently applied | NOW |
| accepted generated result | LAST TAKE |

This is not a mechanical global string replacement. Each label must match the real effect of the operation.

## Chrome and observability

Chrome answers primarily:

1. target + applicable source;
2. transport/position/tempo;
3. meaningful exceptions.

Normal/default state may be visually quiet only when the default is unambiguous and inspectable. External clock, lock/error/armed state or unusual output ownership must remain observable.

Critical semantic tokens are never silently clipped. Overflow priority is explicit.

The first architectural chrome migration should preserve recognizable visual identity where possible; truth and ownership come before redesign.

## Themes and colour

Entity identity is stable across surfaces. Synth A Pattern and Synth A Phrase are still Synth A.

Semantic roles are centralized: entity, focus, selection, disabled, warning/error, page/axis accent.

Colour is never the only carrier of source, selection, disabled state or warning.

Theme may change appearance. Theme may not change information.

CARBON/CYBER/AMBER must all preserve mandatory source, representation, selection, warning and transport truth.

Existing theme identity is a preservation target.

## Density

240x135 is not a debug console. Each surface has one primary object of attention.

Do not give inactive/default processors the same persistent visual weight as active or exceptional state merely to prove that the feature exists.

PERFORM is a preservation target; density changes there require actual 1:1/hardware usability evidence, not theory.

Readability is checked at physical 1:1 scale and then on Cardputer ADV. Enlarged desktop screenshots are not usability proof.

## Realtime/rendering

Keep the existing bounded DirtyTileTracker and other proven mechanisms.

Composition cost and transfer cost are separate budgets.

Desired direction:

semantic invalidation -> bounded region composition -> dirty tracking -> display transfer

Do not optimize blindly. Measure body redraws, dirty tiles, partial runs, full refreshes, composition time, transfer time and their relationship to audio/SD/MIDI workloads.

No arbitrary FPS target substitutes for realtime evidence.

Do not solve continuity by retaining every page, coherent frames by holding audio locks through drawing, or rich timelines by adding large persistent buffers without memory proof.

Steady-state UI should remain bounded and embedded-first.

## Failure behaviour

Navigation, theme changes, viewport movement and representation switching are not musical mutations.

Musical operations continue to use existing prepare/validate/commit semantics.

On failure:

- live material remains correct;
- Undo does not record a fictitious success;
- UI returns to authoritative state;
- source/navigation do not drift;
- user receives a short understandable reason.

## Validation

Behavioural evidence is required for behavioural risks.

At minimum prove:

- title rename does not change semantic location;
- `setBounds()` does not publish semantic state;
- Synth PAT/PHR display matches engine truth;
- a frame does not mix semantic revisions;
- material read view has a safe lifetime;
- renderer eviction preserves defined continuity;
- one input event executes one command;
- footer is derived from the effective command binding;
- unrelated edits preserve musical selection;
- Phrase bars 3–8 are reachable;
- failed mutation preserves material and Undo;
- navigation does not change source;
- all themes preserve mandatory information;
- body rendering cannot escape its owned region.

For preserved surfaces, compare old/new behaviour. Pixel-perfect equality is not required when correcting a semantic defect, but every intentional visual delta must be named and justified.

Hardware acceptance includes interaction while audio is running, Synth A/B PAT/PHR, a long cross-bar Phrase, PERFORM, a rejected edit, relevant Song/SMF/MIDI paths and SD workload where available.

Lo-Fi is the cross-cutting musical verification scenario, but a separate dense stress scenario is required for worst-case load.

## Memory/performance evidence

Substantial checkpoints must explain static DRAM, retained heap, construction peak, largest free internal block, relevant task HWM, composition cost, transfer behaviour and audio timing impact using existing contracts/baselines.

UI host/SDL green does not imply hardware-ready. R0 memory workstream remains independent evidence and may constrain UI decisions.

Memory pressure is not permission to silently degrade continuity or established UX; measure first, then choose a bounded solution.

## Migration philosophy

No big-bang rewrite. No permanent double ownership.

Temporary adapters are allowed; permanent competing semantic owners, header owners or source authorities are not.

Introduce universal abstractions only after a repeated contract is demonstrated.

Prefer:

`CURRENT PRODUCT FORM + NEW ARCHITECTURAL OWNER`

not:

`NEW ARCHITECTURE = NEW PRODUCT DESIGN`

Visual/product polish is a later explicit checkpoint after semantic architecture is stable.

## Definition of done

UI Constitution V1 is implemented when the rules above are architectural/tested properties rather than comments:

- semantic truth has explicit owners;
- geometry is semantically pure;
- one frame is coherent;
- pixel ownership is bounded;
- continuity is independent of renderer residency;
- Pattern/Phrase use one NOTES interaction contract;
- Phrase represents real time/duration and all bars are reachable;
- input/footer command truth is unified;
- themes preserve information;
- product language is musical rather than implementation-facing;
- render/realtime cost is measured;
- musical semantics are preserved;
- strong existing visual/interaction identity survives unless a concrete UX reason justifies change;
- final hardware evidence belongs to the exact final candidate.

After each substantial boundary, the implementation report answers:

- who owns the state now;
- what its lifetime is;
- what is authoritative;
- what failure does;
- which test prevents regression;
- what memory/realtime cost changed;
- which existing product behaviour was preserved;
- why any visual behaviour changed.

Do not call the UI ready while an obligatory hardware gate is pending.

The intended result is **the same recognizable GroovePuter, but behaving as if it had originally been architected correctly for 240x135 and Cardputer ADV constraints.**
