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

GroovePuter owns its musical material, performance semantics, and transport. The Cardputer alone must support the complete core workflow; an external MIDI controller may add physical keys, velocity, and gestures, but must never be required for a primary musical function.

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

## WAVE 4 — Standalone Performance Layer

The Performance Layer makes GroovePuter playable and controllable in real time while keeping musical state inside GroovePuter. Cardputer keys, external MIDI, and future surfaces are adapters that issue the same semantic intents. This is one product workstream, not a MIDI-keyboard branch or a second roadmap.

The dependency order is:

```text
PERFORMANCE CORE (CARDPUTER + RAW MIDI)
        ↓
  ┌─────┴─────────────┐
  │                   │
SMART PLAY     PERFORMANCE CONTROL
  │                   │
  │             PERFORMANCE SCENES
  └──────────┬────────┘
             ↓
STANDALONE CARDPUTER ACCEPTANCE
             ↓
EXTERNAL MIDI ENHANCEMENT
             ↓
PERFORMANCE ↔ DEVELOPMENT
```

Performance Core discovery and host-only contracts may begin as non-blocking preparation alongside current Harmony and Development work because they do not change the DSP, harmonic owner, or audio path. Runtime integration still follows the canonical delivery order above; this wave must not displace unfinished higher-priority work without an explicit roadmap review.

### 4A — Performance Core

- [x] Add bounded `PerformanceIntent` values for note/hold/release, Smart Play configuration, semantic controls, gestures, and Scene actions. Cardputer key presses/releases and USB/UART MIDI notes use the same note-intent dispatcher; adapters do not expose USB, TinyUSB, MIDI endpoint, or key-matrix details to musical owners.
- [x] Keep one authoritative `PerformanceKeyboard`; reuse its existing chord, arp, rhythm, and note ownership behavior instead of adding parallel engines.
- [x] Track input source and session generation through note ownership. Disconnect and recovery release only notes and sustain owned by that source/session; transport, patterns, scenes, and other note owners survive.
- [x] Preserve `RAW` as the exact-note compatibility and escape mode. Smart interpretation is opt-in.
- [x] Define fixed-capacity `NotePlan` and read-only harmonic context snapshot types. No heap or sample-loop interpretation.
- [x] Add host contracts for source identity, raw MIDI dispatch, and bounded note interpretation.
- [x] Adapt the existing class-compliant MIDI ingress to the Core for RAW NoteOn/Off, velocity, sustain, generic CC and scoped cleanup. This slice did not wait for Performance Scenes.

### 4B — Smart Play

Smart Play depends on the accepted Harmony timeline and chord-tone witness contracts (H0-R1/R2). It consumes the current HarmonicEvent read-only and must not invent a second chord-quality table or modify the harmonic timeline.

- [x] Add bounded `RAW`, `BASS`, `LEAD`, `CHORD`, and `ARP` roles. RAW preserves exact external pitches.
- [x] Interpret BASS gestures as bounded harmonic functions and LEAD notes through explicit RAW / SCALE / CHORD / VOICE-LEAD modes.
- [x] Map external BASS input pitch classes to ROOT / THIRD / FIFTH / SEVENTH / APPROACH against the active HarmonicEvent; reject unsupported functions instead of silently turning them into ROOT. Host coverage includes Major7 SEVENTH mapped to the nearest bass-range note. With no harmonic context, keep the absolute-note fallback.
- [x] Keep the existing chord performer authoritative; no second chord engine was added.
- [x] Separate Smart Arp pitch sources, contour, pulse timing, and harmony-change behavior (`FOLLOW`, `HOLD`, `RESTART`). Existing transport/local fallback clocks remain authoritative.
- [x] Use one semantic `ChordQuality → chord-tone intervals` owner for generation materialization and Smart Play.
- [x] Add bounded nearest-note voice leading. Host tests cover deterministic plans and arp traces.
- [x] Apply the source P1R phrase's `GenreBehavior::allowChromatic` rule to LEAD chord/voice pools, BASS chord functions, and HARMONY/ROLE arp pools; disallowed pitch classes are filtered or fail closed.
- [ ] Extend Smart Play's read-only harmonic/genre context to material owners beyond active generated P1R Song rows. The current GenreBehavior does not express a general prohibition on harmonic motion; Smart Play still cannot mutate the HarmonicTimeline.

### 4C — Performance Control

Direct controls remain useful without scenes. Their realtime path is the admission gate for this stream.

- [x] Define semantic control IDs for filter, resonance, level, drive, reverb send, and delay send.
- [x] Apply parameter changes at the audio-block boundary without `AudioMutationGate` or per-sample semantic work.
- [x] Add bounded transient CTRL gestures with automatic cleanup: Delay Throw (`G`), Reverb Bloom (`R`), Filter Push (`F`) and Filter Close (`C`). They overlay live controls for 750 ms and never alter captured Scene state or create a Take.
- [x] Route generic MIDI CC through explicit semantic bindings. Unknown CC, including generic CC1/CC94, remains diagnostic-only; it cannot invoke destructive legacy Mod actions. Pitch-button and pitch-bend compatibility behavior is preserved. Explicit Mod-button binding is deferred.
- [ ] Keep macro mapping transparent if macros are admitted later; each displayed macro must expose its affected controls and amounts.

### 4D — Performance Scenes

Performance Scenes build on direct controls and remain separate from musical material and the existing persisted `Scene` type. Use an unambiguous type and storage name such as `PerformanceScene`.

- [x] Store eight bounded, CRC-checked performance snapshots (`S1`–`S8`) containing mutes, levels, filter, resonance, FX sends and effect enable states.
- [x] Exclude Pattern, Take, Working material, harmonic progression, and Song section contents.
- [x] Support manual control before explicit capture; capture never happens automatically.
- [x] Add immediate recall in STOP and queued recall at the next bar in PLAY. Track active slot and dirty state without overwriting a stored slot.
- [x] Add CUT and bounded 1/2/4/8-bar transitions. Continuous values interpolate; mute/effect enable changes apply at transition completion.
- [x] Keep Scene Morph as performance-state interpolation, separate from musical Development.

### 4E — Standalone Cardputer gate

This is a continuous standalone acceptance gate, not a prerequisite for beginning RAW MIDI keyboard work. The Performance Layer cannot close or claim standalone readiness until one Cardputer completes the full core workflow with no MIDI device connected:

```text
create and play groove → adjust filter/FX → mute → capture S1/S2
→ play/stop → recall → morph
```

Arp input where applicable, Scene capture/recall/morph, STOP behavior, and PLAY bar-boundary behavior must all work without waiting for a controller. Absence of a controller is a normal state and produces no error or empty workflow.

### 4F — Extended MIDI controls and hardware acceptance

The RAW keyboard path is established in Performance Core. After the standalone workflow and Core contracts are green, extend the same ingress and ownership model with optional controls and complete controller hardware acceptance:

- [x] Reuse the current MIDI dispatcher, bounded queues, `InputSource`, session generations, and `PerformanceKeyboard` bridge.
- [x] Add generic CC input without VID/PID requirements; known generic controls have semantic bindings and unknown CC is visible in diagnostics.
- [x] On disconnect, clear only that source's held-note and sustain ownership; reconnect starts a fresh generation and cannot accept stale releases.
- [x] Provide bounded diagnostics for USB host connection state, received packet count, last Note and last known/unknown CC.
- [x] Preserve the complete unsigned 14-bit value at MIDI Pitch Bend ingress.
- [x] Resolve pitch-bend behavior explicitly: new configurations use musical 14-bit Pitch Bend; legacy version-1 settings migrate to `NUDGE`, and users can switch between `MUSICAL` and `NUDGE` in MIDI input settings.
- [x] Route musical Pitch Bend through bounded control queues and existing MIDI output transports; aggregate PERFORM bend by input source/channel and return to center when the owner releases or disconnects. Pitch Bend queue overflow is droppable and never schedules a note panic.
- [ ] Validate hot disconnect during held notes, arp, sustain, scene transition, and transport playback. No global panic, stuck notes, reboot, or transport reset.
- [ ] Measure input-to-output latency and MIDI queue/audio deadline behavior under note and CC load.

External MIDI expands physical control and expression; it does not own harmony, arp, scenes, material, or transport.

### 4G — Development bridge

After the Development Grammar has an accepted semantic contract and stable runtime handoff, allow explicit performance commands such as `BUILD`, `STRIP`, `BREAK`, and `RETURN` to request a legal operation at a permitted musical boundary. These are requests to Development, not immediate mute/filter shortcuts. Development context may inform Smart Play read-only; Smart Play does not mutate Development state.

### Performance Layer deferred extensions

Keep these outside the critical path until the core and standalone acceptance pass:

- MIDI Learn and broad controller profiles;
- SPACE / TENSION / ENERGY macros;
- aftertouch and MPE;
- BLE MIDI;
- automatic scene generation;
- recording Smart Performance back into Take / Working material;
- AI inference, phrase transcription, and automatic accompaniment.

### Wave 4 implementation status

The implementation resides on branch `feature/performance-layer` in the isolated worktree `/home/gg/.codex/worktrees/7c5b/miniacid`; the main checkout is unchanged. Commit `a83066b4` merges v0.9.19 to restore its Fn+N Melody workflow and adds USB MIDI Host transfer recovery, USB-cable input-setting persistence, and a smaller pitch-bend coalescing store. The external BASS mapper derives ROOT / THIRD / FIFTH / SEVENTH / APPROACH from the current chord-quality owner, rejects unsupported pitch classes, and retains absolute-note behavior when no harmonic snapshot exists. Full `tests/run_host_tests.sh` passed on the merged tree before the final MIDI fixes; targeted MIDI ingress/repair, Performance closure, and UI navigation tests passed afterward. The FS1B USB Role build (`CDCOnBoot=default`, `scripts/build_cardputer_dynbuffers.sh --warnings all`) passed on the exact flashed source with 1,631,662 bytes flash and 191,396/191,488 bytes static DRAM. The build used the candidate FatFs archive only. Existing unrelated project/Arduino warnings remain.

CC80 turns ARP on (value 64-127) or off (0-63) and CC81 does the same for LATCH on the PERFORM MIDI input; both are absolute intents through `PerformanceKeyboard::dispatchIntent`. Saving no longer belongs to a page: `PerformanceKeyboard::configChanged()` compares the exported record with the last saved or applied one, and `GroovePuterPlatform::servicePerformanceConfigSave` (`src/platform/cardputer_performance_config_owner.cpp`) saves it from the loop after a 700 ms quiet window, so page edits and MIDI switches persist the same way. The saved record is restored at boot, before any input, so an early CC cannot save defaults over it. CC80/81 stay droppable with the other control events; the hardware flood test decides whether their latest values need separate coalescing.

Still open for Wave 4 (not done, and not verifiable without the device or a settled contract): standalone Cardputer acceptance (4E), MIDI hardware acceptance including flood, NoteOn latency and audio deadline load (4F), widening Smart Play harmonic context to other material types (4B), and the Development Bridge, which waits for an accepted Development Grammar contract (4G). Scene recall is queued through a bounded atomic command and applied by the transport owner at its tick boundary; morph endpoints use the same path. Cardputer, USB MIDI, and UART MIDI use the shared semantic input boundary for notes/controls; Smart role, lead mode, and arp configuration intents are dispatched by the single authoritative `PerformanceKeyboard`.

The performance contracts have compile-time size limits: input identity 8 bytes, intent at most 24, keyboard configuration 18, harmonic context at most 24, NotePlan at most 16, control state 7, gesture overlay at most 32, scene snapshot at most 16, and scene bank at most 144. Host ABI measurements are 4,736 bytes for `PerformanceKeyboard`, 16 for `PerformanceContext`, 10 for `NotePlan`, 12 for one `PerformanceSceneSnapshot`, and 136 for the eight-slot `PerformanceSceneBank`. One scene bank is owned by `PerformPage`, and one gesture overlay by `MiniAcid`; scene persistence is a fixed 84-byte CRC-checked record. Performance intents are stack/value events, not a persistent unbounded queue.

The normal `DISPLAY=:1` session initially produced MiniAcid windows in `Iconic` / `UnMapped` state; X11 activation failed with `BadMatch`. A separate `Xephyr :2` session provided a visible, interactive SDL window. Performance-specific standalone SDL smoke passed without a MIDI device: Cardputer-key note-on/off state was visible; the IN page reported MIDI OFF / USB OFFLINE / RX:0 and showed RAW→BASS→LEAD plus LEAD RAW/SCALE modes; CTRL Filter changed 96→98; Delay Throw displayed and cleared after its bounded duration; S1 and S2 captured distinct values (98 and 102), dirty state displayed as `S1*`, STOP recall restored each value immediately, and PLAY recall showed `QUEUED: NEXT BAR` before the active scene changed at the next bar. The transition endpoint was observed; an intermediate morph value was not captured. Test role and Lead mode were restored to RAW. SDL used dummy audio, so acoustic output was not evaluated. The broader project SDL smoke for Pattern/Phrase causality and generation was not run. Evidence screenshots and environment details are in `/tmp/gp-performance-sdl-evidence/`; the candidate SHA and both display attempts are recorded in `capture.log`. On 2026-10-11 the USB Role build from `a83066b4` was flashed to Cardputer ESP32-S3 MAC `d8:85:ac:a4:4b:ec`; esptool verified the written hashes. Application screen/audio behavior after that flash has not yet been observed. A Medeli K66 attached to the development PC exposes AudioStreaming/HID but no USB MIDI Streaming interface or ALSA MIDI port; confirmation that it is the reported Cardputer controller is pending. Physical standalone acceptance, hot disconnect/reconnect, controller flood, and measured input-to-output latency remain open.

Role, lead mode, chord mode, and Smart Arp configuration persist separately from the eight Performance Scenes. Smart Play receives a read-only runtime harmonic snapshot for generated P1R phrases in active Song rows; general HarmonicTimeline coverage for other material and genre admissibility remain open. BASS APPROACH resolves to the closest lower pitch in the active scale. MIDI Pitch Bend routes the full 14-bit value as musical control by default; version-1 input settings migrate to explicit legacy `NUDGE` mode. Reconfiguration, channel panic, disconnect, stale generations, and bounded-queue pressure have host coverage; NoteOn and continuous expression/control traffic cannot consume the NoteOff reserve, and expression overflow does not trigger note recovery.

The Development bridge remains open. Repository evidence confirms that the normative D0-B semantic contract is still OPEN/RED (`docs/0.9.14/D0_B_NORMATIVE_SEMANTIC_CONTRACT.md`, `docs/0.9.14/D0_C_SEMANTIC_ADAPTER_MAPPING.md`) and no general Development command/coordinator owner exists. D0-C passes its adapter runtime/source tests. The D0-B test-local factorization passes, while the current legacy classifier still returns RED for B8 (EXTEND capability unavailable is collapsed into genre failure) and B9 (broken contour promise is collapsed into genre failure); the D0-B runner also enforces a source firewall against its frozen base, so it exits before those tests on this feature worktree's intentional source changes. A specialized P0 generated-P1R engine action exists: the MATERIAL page `D` gesture requests a DEVELOP+BREAK cycle, or BREAK-only when DEVELOP would repeat the source, from an eligible, unedited P3 phrase. It is not a general BUILD/STRIP/BREAK/RETURN command owner, so Performance commands must not be mapped to it as if it were the full grammar. The generic `growMaterial(..., GrowthMode::Develop, ...)` path still returns a deferred result; P0's device acceptance is separately open in `docs/0.9.14/P0_MUSICAL_PLAY_SPEC.md` §13. The bridge requires an accepted command-to-grammar owner and a stable transport-boundary handoff.

The Performance Layer is complete for its first product release when the standalone Cardputer gate passes, Smart Play and direct controls use their authoritative existing owners, scene recall/morph is deterministic, and optional external MIDI can be connected and removed without losing GroovePuter-owned state.

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
