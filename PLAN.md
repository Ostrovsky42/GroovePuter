# GroovePuter product direction and execution plan

**Status:** canonical product direction and priority order  
**Last reviewed:** 2026-08-03  
**Target branch:** `dev`

This file is the single source of truth for **why GroovePuter exists, what must be built next, and what is deliberately deferred**.

Implementation documents under `docs/stages/` remain useful specifications, acceptance procedures, and historical records. They must not become competing roadmaps. When priorities change, update this file first and make stage documents link back to the relevant item here.

## Product thesis

GroovePuter is a **portable time-feel instrument for capturing, transforming, developing, and arranging musical phrases**.

It remains a self-contained groovebox. External instruments are output targets, not the definition of the product. Yamaha SEQTRAK is the first deeply tested target, but GroovePuter must remain useful without it.

The central promise is:

> Capture or generate a musical phrase, reshape how it feels in time, derive related material, and develop it into an arrangement.

The product must optimize the path:

```text
play / generate / extract
        -> Phrase
        -> shape FEEL
        -> derive variation
        -> Section
        -> Song
        -> internal playback or external output
```

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

Examples that must become the same domain object:

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

Source information is provenance. After extraction or capture, a Phrase must be independently editable and playable without reopening or rescanning its original source.

### Phrase outputs

```text
internal audition
Phrase slot
Section
Song
external MIDI send
```

External transfer is an output of Phrase. It is not the product identity.

## Entity boundaries

| Entity | Responsibility |
|---|---|
| **Step** | One position on a timing grid |
| **Bar** | Meter-relative time unit |
| **Pattern** | Events owned by one track or generator, usually cyclic |
| **Phrase** | Bounded, musically related material spanning one or more bars and tracks |
| **Section** | Arrangement role and combination of Phrases, such as intro, main, variation, break, or outro |
| **Scene** | Runtime snapshot of sound, routing, mute, FEEL, and performance state |
| **Song** | Ordered sequence of Sections |

`Scene` and `Section` must not be used as synonyms. A Section may reference a Scene, but musical form and runtime state are separate concerns.

## Interaction principles

### Routine work must be direct

The common external-send flow is:

```text
select Phrase or snapped region -> SEND
```

Target, channel, clock policy, count-in, and boundary policy are persistent device settings. A setup wizard may exist for first use, but must not appear for every transfer.

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

Phrase capabilities must enter the existing workflow rather than create another five-screen subsystem. Detailed settings remain behind a modifier or settings page; the common action stays visible and direct.

## Runtime and architecture invariants

These constraints override feature convenience:

- `MidiDispatchTask` remains the single TinyUSB writer.
- Audio, pattern, live, SMF, Phrase audition, and Phrase send must share one explicit ownership model.
- No parallel MIDI scheduler or second transport state machine.
- No dynamic allocation on audio or MIDI hot paths.
- Scheduled MIDI remains sample/deadline based; routine UI work must not perturb timing.
- Source switches invalidate stale queued events and release only notes owned by the previous source.
- Stop, seek, mute, route change, disconnect, and page transitions must not leave active notes.
- Scene and project codecs remain backward compatible.
- UI work must prefer partial redraws over repeated full-screen rendering.
- Existing standalone audio behavior must remain available while MIDI output is enabled.

## Canonical delivery order

The labels below define priority. Stage documents and PRs must map to this order rather than establish a second sequence.

### NOW — Stabilize MIDI lifecycle and ownership

Current delivery lane: PR #35, `feature/midi-lifecycle-panel-performance` on top of `dev`.

Objectives:

- [ ] Complete ownership-safe MIDI Continue and capability-gated SPP behavior.
- [ ] Complete immediate SMF track mute cleanup without global panic.
- [ ] Preserve SMF panel session state across page eviction.
- [ ] Remove routine synchronous UI logging from performance-sensitive paths.
- [ ] Cache MIDI directory enumeration outside frame rendering.
- [ ] Complete partial SMF Player redraw.
- [ ] Run host tests, SDL build, Cardputer-Adv build, and direct Cardputer-Adv -> SEQTRAK hardware acceptance.
- [ ] Merge the stabilized lifecycle line into `dev` before starting Phrase UI work.

This is the same class of problem as earlier Synth A conflicts: voice, source, clock, and transport ownership must be solved coherently rather than with independent local flags.

### NEXT — Safe bounded send

Build on the existing USB-MIDI dispatcher, clock, transport, and realtime SMF output. Do not add a new scheduler.

Minimum behavior:

- [ ] Send one bounded Phrase view or snapped SMF region exactly once.
- [ ] Start on an explicit safe boundary, normally the next bar.
- [ ] Support a persistent default count-in, initially one bar.
- [ ] Suppress every new Note On outside the send window.
- [ ] Clip active notes at the end boundary.
- [ ] Send sustain-off and scoped active-note cleanup at the end.
- [ ] Never auto-repeat in `SEND ONCE` mode.
- [ ] Show an honest `SENT`, event count, bar count, channel, and target status.
- [ ] Ask for explicit confirmation that the target is armed because this cannot be detected.
- [ ] Keep `AUDITION LOOP`, `SEND ONCE`, and `PERFORM` as separate ownership modes.

The first implementation may consume a bounded, non-persistent Phrase view over an SMF region or current pattern. It must not introduce a permanent transfer-only domain object that competes with Phrase.

### NEXT — Phrase core and slots

After safe-send ownership is proven on hardware:

- [ ] Add one canonical Phrase model with versioned persistence.
- [ ] Add Phrase slots `A / B / C / D` as working memory for the instrument.
- [ ] Allow a slot to be created from live capture, an internal pattern set, an SMF region, generated material, or another Phrase.
- [ ] Add bar-snapped SMF `IN / OUT` extraction into a Phrase slot.
- [ ] Decouple an extracted Phrase from its source file.
- [ ] Add Phrase audition without transferring ownership to Song or external send.
- [ ] Preserve provenance and parent-Phrase relationships.
- [ ] Provide safe migration for older projects and scenes.
- [ ] Avoid a separate Phrase navigation carousel; integrate slots into the existing PERFORM / PATTERN / ARRANGE workflow.

Recommended initial roles:

```text
A MAIN
B VARIATION
C BREAK
D ENDING
```

Roles are metadata, not hard-coded slot behavior.

### LATER — Retrospective live capture

This is the only near-term MIDI Keys expansion admitted before the Phrase model is mature.

- [ ] Maintain a bounded, allocation-free recent-note ring buffer.
- [ ] Capture the last `1 / 2 / 4 / 8` bars after the performance occurred.
- [ ] Normalize to a musical boundary without losing pickup intent.
- [ ] Offer `RAW` and one conservative soft-quantize mode.
- [ ] Trim leading silence when requested.
- [ ] Clip or release notes at the Phrase end.
- [ ] Store the result directly in a Phrase slot.

The feature succeeds when a player can improvise, press Capture after a good take, and receive a reusable Phrase without first arming a recorder.

### STRATEGIC — Phrase development

Once Phrase capture, extraction, playback, persistence, and safe output are reliable:

- [ ] Derive related Phrases without destroying the parent.
- [ ] Model `MAIN / VARIATION / FILL / BREAK / ENDING` relationships.
- [ ] Add phrase-aware continuation, reduction, answer, extension, and mutation.
- [ ] Apply FEEL as an independent interpretation of a Phrase.
- [ ] Compare Phrase A/B while exposing which axis changed.
- [ ] Allow a Phrase to retain musical identity across TEXTURE changes.
- [ ] Add explicit reinterpretation when applying a new GENRE corridor.

The goal is not more randomization. The goal is controlled development of recognizable material.

### STRATEGIC — Section and Song development

- [ ] Build Sections from Phrase references, repetition counts, entry rules, and transition intent.
- [ ] Keep Section form separate from Scene runtime state.
- [ ] Build Song as an ordered sequence of Sections.
- [ ] Preserve Phrase identity when moving between internal and external playback.
- [ ] Support reliable replay after save/load with the same position, ownership, FEEL, and routing state.

## Deferred backlog

These ideas are retained, but must not interrupt the delivery order above.

### Performance generators

- Chord mode.
- Arpeggiator.
- Note Repeat.
- Rhythm Gate.
- Strum.

They may return only after Phrase core is stable. Each must produce, transform, or perform a Phrase through the existing ownership model; none may introduce another isolated keyboard mode.

### Orthogonal-axis interface

- Per-axis Lock for GENRE / FEEL / GENERATOR / TEXTURE.
- A/B diff showing exactly which axes changed.
- Morph controls for FEEL, generator density, and TEXTURE.
- A compact top-level state summary exposing all four axes.

These are strategically important, but depend on clean state ownership and must not become another navigation rewrite.

### External targets and protocol depth

- Additional tested device profiles beyond SEQTRAK.
- Per-target capability declarations.
- Custom per-track SMF routing.
- Program Change, CC, Pitch Bend, Aftertouch, and carefully scoped SysEx.
- BLE-MIDI.

SEQTRAK remains the first acceptance target, not a required dependency or the product definition.

### Deeper Phrase tools

- Named Phrase library beyond the four working slots.
- Search, tags, and project-level Phrase catalog.
- Non-destructive transform history.
- Detailed note editor for Phrase internals.
- Probability, articulation, ratchet, and advanced microtiming editing.

## Explicit non-goals

- No general UI framework rewrite.
- No second roadmap parallel to this file.
- No five-step wizard for routine Phrase send.
- No claim that an external device recorded data without feedback.
- No parallel MIDI dispatcher, timer task, or Song renderer.
- No conversion of GroovePuter into a SEQTRAK-only peripheral.
- No attempt to compete with a desktop DAW through feature count.
- No new synth, effect, or generator merely to expand the feature list while Phrase and ownership remain incomplete.

## Feature admission rule

A proposed feature enters `NOW` or `NEXT` only when it does at least one of the following:

1. fixes a P0/P1 reliability or ownership defect;
2. creates, captures, extracts, plays, shapes, develops, arranges, or safely outputs a Phrase;
3. makes GENRE, FEEL, GENERATOR, or TEXTURE more independently controllable;
4. removes a repeated workflow step without adding another navigation system.

Otherwise it remains in the deferred backlog.

## Definition of done for roadmap stages

A stage is complete only when:

- behavior is covered by host/source regression tests where practical;
- SDL builds and the Cardputer-Adv firmware builds;
- hot paths remain allocation bounded;
- MIDI/audio timing and active-note cleanup are validated;
- direct hardware acceptance is documented for hardware-dependent behavior;
- user-visible status is honest about what the device can and cannot know;
- README and the relevant stage document describe the shipped capability;
- deferred ideas discovered during implementation are recorded here instead of being silently added to the active scope.
