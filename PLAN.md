# GroovePuter product direction and execution plan

**Status:** canonical product direction and priority order  
**Last reviewed:** 2026-08-03  
**Target branch:** `dev`

This file is the single source of truth for **why GroovePuter exists, what must be built next, how success is measured, and what is deliberately deferred**.

Implementation documents under `docs/stages/` are subordinate specifications, acceptance procedures, and historical records. They must map to an item in this file and must not silently reorder the roadmap.

`README.md` describes the product and capabilities that are actually available on the branch. `MANUAL.md` describes current user-visible behavior. Neither file may redefine product priorities independently of this plan.

## Product thesis

GroovePuter is a **portable time-feel instrument for capturing, generating, transforming, developing, and arranging musical phrases**.

It remains a self-contained groovebox. External instruments are output targets, not the definition of the product. Yamaha SEQTRAK is the first deeply tested target, but GroovePuter must remain useful without it.

The central promise is:

> Capture or generate a musical phrase, reshape how it feels in time, derive related material, and develop it into an arrangement.

The intended path is:

```text
play / generate / extract
        -> Phrase
        -> shape FEEL
        -> derive variation
        -> Section
        -> Song
        -> internal playback or external output
```

External transfer is one output of this path. It is not the product identity and must not displace the standalone instrument, interface clarity, or musical-development work.

## Core differentiator

```text
GENRE != FEEL != GENERATOR != TEXTURE
```

These are independent musical decisions, not four names for presets.

| Axis | Responsibility | Must not silently change |
|---|---|---|
| **GENRE** | Musical language, constraints, characteristic relationships, valid corridors | Exact timing interpretation, sound engine, source mechanism |
| **FEEL** | Swing, microtiming, push/pull, gate, timebase, accent placement | Notes, phrase identity, instrument choice |
| **GENERATOR** | How events are captured, created, continued, reduced, or mutated | Genre identity, timing profile, texture |
| **TEXTURE** | Synth engine, kit, processing, drive, space, degradation | Notes, rhythm structure, phrase boundaries |

Changing one axis must preserve the other three unless the user explicitly chooses a destructive operation such as **reinterpret** or **regenerate**.

This rule must become visible in the data model and interaction model, not remain only a contributing guideline.

## Missing musical object: Phrase

The intended hierarchy is:

```text
step -> bar -> phrase -> section -> song
```

The missing level is **Phrase**. It is both:

1. the unit of musical development; and
2. the unit of capture, extraction, audition, storage, and external transmission.

Examples that must eventually become the same domain concept:

```text
SMF file, bars 5-8             -> Phrase A
last four bars of live playing -> Phrase B
current internal pattern set   -> Phrase C
variation derived from A       -> Phrase D
```

A Phrase initially targets bounded material of `1 / 2 / 4 / 8` bars, while the architecture must not make eight bars a permanent storage limit.

### Phrase sources

```text
LIVE_CAPTURE
INTERNAL_PATTERN
SMF_REGION
GENERATED
DERIVED
```

Source information is provenance, not five mutually exclusive object layouts.

### Phrase outputs

```text
internal audition
Phrase slot
Section
Song
external MIDI send
```

### Minimal Phrase contract and memory gate

The first Phrase implementation must not become a union containing every field required by every source. Before production code is written, the implementation PR must document:

- the exact metadata struct and `sizeof`;
- the number of resident instances;
- the backing store for note events;
- the maximum event capacity per Phrase;
- the total static RAM cost;
- the persistence delta per project or scene;
- the flash delta after the Cardputer build.

Initial target:

```text
Phrase metadata: <= 16 bytes per slot
Working slots:    4
Metadata RAM:     <= 64 bytes total
Dynamic alloc:    forbidden in playback paths
```

Phrase metadata should contain only common identity and bounded playback information, such as an ID, backing-store reference, length, role, flags, parent relationship, and provenance. Event data must reuse an existing fixed-capacity pattern/event store or one explicitly budgeted fixed-capacity pool.

An SMF region may initially be a non-persistent Phrase view. A captured or extracted Phrase that promises independence from its source must own normalized events in an approved backing store; it must not depend on rescanning the original file.

No Phrase schema is accepted on elegance alone. It is accepted after arithmetic.

## Entity boundaries

| Entity | Responsibility |
|---|---|
| **Step** | One position on a timing grid |
| **Bar** | Meter-relative time unit |
| **Pattern** | Events owned by one track or generator, usually cyclic |
| **Phrase** | Bounded, musically related material spanning one or more bars and tracks |
| **Section** | Arrangement role and combination of Phrases, such as intro, main, variation, break, or outro |
| **Scene** | Persisted/runtime snapshot of sound, routing, mute, FEEL, generator and performance state according to the actual codec |
| **Song** | Ordered sequence of Sections or current legacy song rows during migration |

`Scene` and `Section` must not be used as synonyms. A Section may reference a Scene, but musical form and runtime state are separate concerns.

## Product acceptance metrics

The plan is not complete unless the product can be evaluated numerically. Phase 0 of every wave records the current baseline before implementation. These are initial targets and may be tightened after reproducible measurement.

### Interface trust

- **Unlabelled silent alphabetic keypresses:** `0` across the documented page/input test matrix.
- **Persistent mutation visibility:** `100%` of inventoried persistent mutation gateways set the dirty state.
- **Dirty false positives:** `0` for navigation, focus, held live notes, transient playheads, and other codec-confirmed runtime-only actions.
- **Status correctness:** `100%` agreement between visible source/state/clock/output/key-mode tokens and their actual state owners in host tests where observable.
- **Status redraw:** no full-page redraw caused solely by a step tick or status update.

### Time to usable music

- **Time to first groove:** no more than `90 seconds` from boot for a user familiar with the key sheet, using only the device.
- **Return to last saved state:** one load operation and no ambiguous unsaved-state loss.
- **Mode prediction:** a user can predict whether an alphabetic key means `NOTE`, `CMD`, `LOCAL`, or `LOCK` before pressing it on every tested page.

### Musical identity and development

- **Blind genre-family recognition:** at least `70%` correct over a fixed test set containing at least six implemented genre families and at least ten randomized trials; no family may score below chance in two consecutive runs.
- **Four-bar direction:** at least `80%` of generated four-bar phrases in enabled cadence presets must contain a deterministic, audible fourth-bar operation while preserving bars 1-3 except where the selected operator explicitly requires earlier setup.
- **Root-motion determinism:** identical seed, genre, scale, root-motion preset, and generator parameters produce identical pitch output.
- **Legacy identity:** old scenes without new fields produce byte-for-byte identical generated note output until a new feature is explicitly enabled, where practical to test.

### Realtime safety

- **Audio underruns:** no sustained increase during the standard Wave 1 hardware acceptance run compared with the recorded baseline.
- **Heap growth:** no monotonic heap loss during 30 minutes of navigation, generation, playback, save/load, and MIDI activity.
- **Stuck notes:** `0` after stop, mute, route change, source change, page eviction, disconnect, or panic in the acceptance matrix.

## Interaction principles

### State must be visible before it is editable

The user must be able to see:

```text
source / transport state / bar / clock / output / effective key mode / unsaved state
```

The status layer must derive a read-only snapshot from existing owners. It must not create a second transport, input, routing, or persistence state.

### Routine work must be direct

The common external-send flow is:

```text
select Phrase or snapped region -> SEND
```

Target, channel, clock policy, count-in, and boundary policy are persistent settings. A setup wizard may exist for first use, but must not appear for every transfer.

### Honest status

MIDI output has no recording acknowledgement from SEQTRAK. GroovePuter may state:

```text
SENT · 4 BARS -> CH8 · CHECK TARGET
```

It must not claim `RECORDED` or `TRANSFER COMPLETE` when it only knows that local transmission finished.

### Separate playback intentions

The UI and ownership model must distinguish:

```text
AUDITION LOOP  - repeat locally or externally for selection
SEND ONCE      - transmit one bounded pass and stop output
PERFORM        - continuous live ownership
```

### Musical boundaries by default

- Region selection snaps to bars by default.
- Pickup notes use pre-roll rather than negative transport positions.
- Lead-in notes default to retrigger at the selected Phrase boundary.
- Notes crossing an external-send boundary default to clip.
- Every bounded send ends with scoped active-note cleanup.

### No new navigation system

New capabilities must enter the existing workflow rather than create another multi-screen subsystem. Detailed settings remain behind existing pages or modifiers; common actions stay visible and direct.

## Runtime and architecture invariants

These constraints override feature convenience:

- Cardputer ADV target is ESP32-S3FN8 with PSRAM disabled and `PartitionScheme=huge_app`.
- `MidiDispatchTask` remains the single TinyUSB writer.
- Audio, pattern, live, SMF, Phrase audition, and Phrase send share one explicit ownership model.
- No parallel MIDI scheduler or second transport state machine.
- No dynamic allocation on audio or MIDI hot paths.
- No new work on every audio sample for Wave 1 features.
- No string formatting in `AudioTask`.
- No UI drawing under an audio lock.
- No scene scan, pattern scan, structural hash, serialization, or storage traversal in the frame loop.
- Scheduled MIDI remains sample/deadline based; routine UI work must not perturb timing.
- Source switches invalidate stale queued events and release only notes owned by the previous source.
- Stop, seek, mute, route change, disconnect, and page transitions must not leave active notes.
- Scene and project codecs remain backward compatible through actual codec conventions and missing-field defaults.
- UI work must prefer bounded dirty regions over repeated full-screen rendering.
- Existing standalone audio behavior remains available while MIDI output is enabled.
- No general framework rewrite, new UI engine, new persistence system, new transport task, new MIDI dispatcher, or second Phrase architecture.

## Canonical delivery order

The labels below define priority. Stage documents and PRs map to this order rather than establish a second sequence.

### IN FLIGHT — Close the accepted MIDI lifecycle lane

Current delivery lane: PR #35, `feature/midi-lifecycle-panel-performance` on top of `dev`.

This work already exists and should be completed without expanding its product scope:

- [ ] Complete ownership-safe MIDI Continue and capability-gated SPP behavior.
- [ ] Complete immediate SMF track mute cleanup without global panic.
- [ ] Preserve SMF panel session state across page eviction.
- [ ] Remove routine synchronous UI logging from performance-sensitive paths.
- [ ] Cache MIDI directory enumeration outside frame rendering.
- [ ] Complete partial SMF Player redraw.
- [ ] Run host tests, SDL build, Cardputer-Adv build, and direct Cardputer-Adv -> SEQTRAK hardware acceptance.
- [ ] Merge the stabilized lifecycle line into `dev`.

Closing an already active reliability PR is not a product-priority change. After this merge, the primary product lane is Wave 1 below.

## WAVE 1 — Interface trust and musical direction

Wave 1 addresses the two original product weaknesses:

1. the current state and effective controls are not always predictable;
2. generated material does not provide enough direction above one 16-step bar.

It deliberately does **not** fix the P0 behavior where live note keys are consumed while transport runs. It makes that state visible as `LOCK` and leaves the ownership correction to its own later reliability task.

### Mandatory discovery gate

Before each Wave 1 implementation branch:

```text
git fetch origin
git switch dev
git pull --ff-only
```

The first task is repository reconnaissance, not production code. It must report:

- exact `dev` head SHA;
- relevant files, types, functions, state owners, codecs, tests, and redraw paths;
- differences between this plan and actual code;
- existing mechanisms that will be reused;
- exact RAM and estimated flash cost;
- realtime risks and forbidden integration points;
- whether the task changes scene schema;
- complexity classification `S / M / L`.

No API may be invented in advance. If a required extension point does not exist, the PR must propose the smallest new abstraction and justify it.

### Branch and PR order

Each item is a small PR into `dev`:

```text
A1 -> A2 -> B -> D -> C
```

Recommended branches:

```text
feature/ui-status-chrome
feature/scene-dirty-revision
feature/effective-key-mode-indicator
feature/root-motion-spine
feature/phrase-cadence-operators
```

Do not keep several broadly diverging Wave 1 branches open. Start the next branch from updated `dev`, or explicitly document a stacked PR with base/head SHA.

### A1 — Common status chrome

Add one compact common status region, preferably `12-16 px` high inside an existing reserved header/footer area. Do not shift every page manually.

Minimum tokens, derived from real state owners:

```text
SRC   PATTERN / SONG / SMF / LIVE
STATE PLAY / PAUSE / STOP / ARMED
BAR   current bar position
CLK   INTERNAL / FILE / EXTERNAL
OUT   INTERNAL / SEQTRAK / BOTH
KEYS  NOTE / CMD / LOCAL / LOCK
DIRTY persisted-state difference marker
```

Example:

```text
PAT ▶ B3/4 INT BOTH NOTE *
```

Constraints:

- no duplicated UI-owned transport or routing state;
- no temporary string pointers in the snapshot;
- redraw only when the bounded snapshot changes;
- bar token changes only at a bar boundary;
- no per-step full redraw;
- no `snprintf` in every frame;
- A1 does not change input, transport, MIDI, audio, Scene, or persistence behavior.

Acceptance includes all pages remaining readable, rapid navigation without flashes, correct state tokens, no full redraw from status-only changes, and unchanged audio/MIDI behavior.

### A2 — Scene revision and dirty tracking

A global manually maintained boolean is not sufficient. Introduce the smallest revision tracker compatible with actual persistence gateways:

```text
dirty = currentRevision != persistedRevision
```

Required semantics:

- persistent user mutations advance the current revision;
- runtime-only focus, cursor, page, toast, held-note, and playhead state do not;
- successful save aligns persisted revision;
- failed save does not clear dirty;
- successful full load aligns both revisions;
- failed or partial load does not clear dirty;
- reset/new-project semantics are explicit;
- draw code never decides persistence state;
- no Scene hashing, copying, or serialization in frame or playback loops.

The discovery report must classify at least:

```text
Genre
Texture
Feel
Generator parameters
Synth parameters
Song
Track mutes
NOTE mode
Performance scale/root/octave
MIDI route
Clock source
SMF runtime state
```

Classification follows the actual codec, not assumptions in this document.

Acceptance target: every inventoried persistent mutation gateway marks dirty, all listed runtime-only actions remain clean, and save/load failure paths preserve truthful state.

### B — Effective alphabetic-key mode

Do not merely display the global NOTE toggle. Display the effective meaning of alphabetic keys after page-first-refusal and transport ownership are considered:

```text
NOTE   performance keyboard will emit notes
CMD    legacy command layer is active
LOCAL  current page owns alphabetic commands
LOCK   NOTE is enabled but existing transport ownership suppresses NoteOn
```

Constraints:

- do not change page-first-refusal;
- do not change mappings, event routing, PatternPlayer ownership, or suppression behavior;
- no blocking animation or full-screen flash;
- use the existing theme palette;
- badge updates immediately when effective mode changes.

Acceptance target: zero unlabelled silent alphabetic keypresses in the documented page matrix. The user must be able to predict the result before pressing the key.

### D — Root Motion spine

This is not a chord engine and must not be presented as one. It moves the tonal center of existing Synth A and Synth B material by scale degrees across bars. Drums are never transposed.

Target model:

```text
length:  1..8
steps:   signed scale-degree offsets
preset:  stable ID, not UI index
enabled: explicit
RAM:     <= 16 bytes per persisted instance
```

Initial stable presets:

```text
OFF
AUTO
STATIC          [0]
i-VI            [0, 5]
i-VI-III-VII    [0, 5, 2, 6]
i-iv-VI-V       [0, 3, 5, 4]
i-VII-VI-VII    [0, 6, 5, 6]
```

Orthogonality rules:

- `AUTO` asks Genre for a recommendation through a pure function;
- `MANUAL` survives Genre changes;
- `OFF` produces old behavior;
- Genre never overwrites a manual root-motion choice;
- old scenes missing the field default to `OFF`, preserving previous sound.

Implementation rules:

- precompute effective degree/semitone offset at a bar boundary where possible;
- note scheduling performs only bounded integer work and register wrapping;
- no sample-renderer integration;
- REST values remain REST;
- negative degree shifts work;
- register limits use octave wrapping rather than flattening many notes to one clamp boundary;
- chromatic-input policy is chosen once during discovery and covered by deterministic tests.

Place the control in an existing scale/generator group if one exists. Do not create a new page.

### C — Phrase cadence operators

Wave 1 must produce audible multi-bar direction before a general Phrase object is introduced.

Extend the existing `1B / 2B / 4B / 8B` generation and Song mechanisms. Do not create a parallel runtime-fill engine.

V1 operators are bounded generation-time transformations over existing pattern data. At minimum provide deterministic roles for:

```text
HOLD       no phrase-level change
LIFT       increase activity or register near the phrase end
REDUCE     remove bounded events near the phrase end
FILL       apply existing drum step-FX/retrig capabilities in a bounded final-bar window
CADENCE    bias the final bar toward a stable tonal/rhythmic landing
```

The first implementation may use only a final-bar rule when phrase length is at least four bars. It must:

- preserve bars 1-3 unless an operator explicitly declares earlier setup;
- reuse existing generators, scale tables, step FX, and schedulers;
- operate at generation time or a bar-boundary precompute point, never per sample;
- remain deterministic for a fixed seed and parameters;
- avoid a new event format or mutation log;
- expose the effective operator and phrase length in the existing generator UI;
- default old scenes to `HOLD` or equivalent legacy behavior.

Acceptance target: at least 80% of generated four-bar phrases in enabled cadence presets have an audible, test-detectable fourth-bar difference while preserving structural identity with the first three bars.

### Wave 1 explicit exclusions

- fixing the live-note transport P0;
- changing actual input priority;
- simultaneous live and PatternPlayer ownership of Synth A;
- voice stealing;
- NOTE toggle-to-hold redesign;
- removing legacy commands or `Fn+1..0`;
- replacing navigation with a new `STEP/BAR/PHRASE/PART/SONG` system;
- anchor-step system;
- `VARY 0..100` metric;
- global rhythmic contract between tracks;
- energy arc;
- seed/operator log as the main storage format;
- chord voicing or full progression engine;
- new USB-MIDI architecture;
- SMF transport refactor;
- unrelated framework cleanup.

## WAVE 2 — Phrase transport safety and extraction

Wave 2 starts only after Wave 1 acceptance and after the in-flight MIDI lifecycle line is stable on hardware.

### Safe bounded send

Build on the existing USB-MIDI dispatcher, clock, transport, and realtime SMF output. Do not add a new scheduler.

Minimum behavior:

- [ ] Send one bounded current-pattern view or snapped SMF region exactly once.
- [ ] Start on an explicit safe boundary, normally the next bar.
- [ ] Support a persistent default count-in, initially one bar.
- [ ] Suppress every new Note On outside the send window.
- [ ] Clip active notes at the end boundary.
- [ ] Send sustain-off and scoped active-note cleanup at the end.
- [ ] Never auto-repeat in `SEND ONCE` mode.
- [ ] Show honest `SENT`, event count, bar count, channel, and target status.
- [ ] Ask for explicit confirmation that the target is armed because this cannot be detected.
- [ ] Keep `AUDITION LOOP`, `SEND ONCE`, and `PERFORM` as separate ownership modes.

The first implementation may consume a bounded, non-persistent Phrase view. It must not introduce a transfer-only domain object that competes with the later Phrase core.

### Phrase slots and SMF extraction

After bounded ownership is proven:

- [ ] Add Phrase working slots `A / B / C / D` using the approved byte-budget contract.
- [ ] Add bar-snapped SMF `IN / OUT` extraction into a slot.
- [ ] Add current-pattern capture into a slot.
- [ ] Decouple extracted normalized events from the source file when the UI claims independence.
- [ ] Add Phrase audition without transferring ownership to Song or external send.
- [ ] Preserve bounded provenance and parent relationship metadata.
- [ ] Avoid a separate Phrase navigation carousel.

Recommended initial role labels:

```text
A MAIN
B VARIATION
C BREAK
D ENDING
```

These are metadata, not hard-coded slot behavior.

### Retrospective live capture

This is the only near-term MIDI Keys expansion admitted before the Phrase model is mature:

- [ ] bounded allocation-free recent-note ring buffer;
- [ ] capture last `1 / 2 / 4 / 8` bars after performance;
- [ ] musical-boundary normalization with explicit pickup handling;
- [ ] `RAW` and one conservative soft-quantize mode;
- [ ] optional leading-silence trim;
- [ ] note clipping/release at Phrase end;
- [ ] direct storage into a Phrase slot.

The feature succeeds when a player can improvise, press Capture after a good take, and receive a reusable Phrase without arming a recorder first.

## WAVE 3 — Phrase development, Sections, and Song

Once Phrase capture, extraction, playback, persistence, and safe output are reliable:

- [ ] Derive related Phrases without destroying the parent.
- [ ] Model `MAIN / VARIATION / FILL / BREAK / ENDING` relationships.
- [ ] Add phrase-aware continuation, reduction, answer, extension, and mutation.
- [ ] Apply FEEL as an independent interpretation of a Phrase.
- [ ] Keep musical identity across TEXTURE changes.
- [ ] Add explicit reinterpretation when applying a new GENRE corridor.
- [ ] Build Sections from Phrase references, repetition counts, entry rules, and transition intent.
- [ ] Keep Section form separate from Scene runtime state.
- [ ] Build Song as an ordered sequence of Sections while preserving compatibility with current Song behavior.
- [ ] Preserve reliable replay after save/load with the same position, ownership, FEEL, and routing state.

Wave 1 cadence operators are the first thin slice of this direction, not a disposable prototype.

## Existing capabilities that must not be reclassified as deferred

The following already exist in some form and must remain available:

- Pattern and Drum step editors;
- current Song dual slots and split compare;
- current `1B / 2B / 4B / 8B` generation/Song behavior;
- existing FEEL and TEXTURE controls;
- current NOTE mode and scale-aware keyboard;
- current USB-MIDI dispatcher and accepted output paths on `dev`;
- current SMF playback and routing behavior.

Future work may extend these capabilities, but must not hide, disable, or describe the existing versions as unimplemented.

## Deferred backlog

These ideas are retained, but must not interrupt the delivery order above.

### Future performance generators

- Chord mode and chord voicing.
- Arpeggiator.
- Note Repeat.
- Rhythm Gate.
- Strum.

They may return after Phrase core is stable. Each must produce, transform, or perform a Phrase through the existing ownership model; none may introduce another isolated keyboard mode.

### Future orthogonal-axis controls

- Per-axis Lock for GENRE / FEEL / GENERATOR / TEXTURE.
- Cross-axis A/B diff beyond the existing Song split compare.
- Morph controls for FEEL, generator density, and TEXTURE.
- A compact four-axis summary beyond the Wave 1 operational status chrome.

These are strategically important but must not become another navigation rewrite.

### External targets and protocol depth

- Additional tested device profiles beyond SEQTRAK.
- Per-target capability declarations.
- Deeper custom per-track SMF routing.
- Program Change, CC, Pitch Bend, Aftertouch, and carefully scoped SysEx.
- BLE-MIDI.

SEQTRAK remains the first acceptance target, not a required dependency or the product definition.

### Deeper Phrase tools

- Named Phrase library beyond four working slots.
- Search, tags, and project-level Phrase catalog.
- Non-destructive transform history.
- Dedicated Phrase-internal editor beyond the existing Pattern and Drum step editors.
- Probability, articulation, ratchet, and advanced microtiming editing beyond current controls.

## Explicit non-goals

- No general UI framework rewrite.
- No second roadmap parallel to this file.
- No five-step wizard for routine Phrase send.
- No claim that an external device recorded data without feedback.
- No parallel MIDI dispatcher, timer task, or Song renderer.
- No conversion of GroovePuter into a SEQTRAK-only peripheral.
- No attempt to compete with a desktop DAW through feature count.
- No new synth, effect, or generator merely to expand the feature list while interface trust and musical direction remain incomplete.

## Feature admission rule

A proposed feature enters an active wave only when it does at least one of the following:

1. fixes a P0/P1 reliability, trust, or ownership defect;
2. makes the current state or effective control behavior more predictable;
3. creates audible musical direction above one bar without creating parallel architecture;
4. creates, captures, extracts, plays, shapes, develops, arranges, or safely outputs a Phrase;
5. makes GENRE, FEEL, GENERATOR, or TEXTURE more independently controllable;
6. removes a repeated workflow step without adding another navigation system.

It must also be more important than the unfinished items earlier in the canonical delivery order. Otherwise it remains deferred.

## Definition of done for roadmap stages

A stage is complete only when:

- numerical acceptance targets are recorded with a reproducible procedure;
- behavior is covered by host/source regression tests where practical;
- SDL builds and the Cardputer-Adv firmware builds;
- new structures have documented `sizeof`, instance count, total RAM, and flash delta;
- hot paths remain allocation bounded;
- MIDI/audio timing and active-note cleanup are validated where affected;
- direct hardware acceptance is documented for hardware-dependent behavior;
- user-visible status is honest about what the device can and cannot know;
- old scenes load with safe defaults and unchanged legacy sound until new behavior is enabled;
- `README.md`, `MANUAL.md`, and the relevant stage document agree with shipped behavior and this plan;
- deferred ideas discovered during implementation are recorded here instead of entering active scope silently.
