# 0.9.6 Output Ownership Research

Status: **RESEARCH ONLY / DO NOT MERGE AS PRODUCTION**  
Release theme: **Unified Output Ownership**  
Working UX hypothesis: **INTERNAL / MIDI / LAYER**  
Frozen research baseline: `d3db4e48ebc08862bdaf9f62532414f009839192`

This document is an evidence audit of the frozen 0.9.4 runtime. It does not introduce production behavior. Production 0.9.6 must wait for 0.9.5 FINAL, record its exact SHA, and repeat the delta audit described below before any implementation branch is created.

---

## 1. Executive summary

The frozen 0.9.4 tree does **not** have one canonical owner for the question "where does this track's musical event go?". Instead, output behavior is a consequence of event source, direct DSP calls, USB availability, static route configuration, and several independent lifecycle owners.

The most important current split is:

- Pattern/Song Synth A/B trigger the internal `SwappableSynthVoice` directly **and** publish the same logical note to the Pattern MIDI queue. With USB MIDI available, this is effectively `LAYER`.
- Pattern drums trigger the internal drum synth, may trigger the internal sampler layer, and publish an external MIDI drum event through `PatternPublishingDrumVoice`. With USB MIDI available, one drum event can therefore feed internal drum synth + internal sample + external MIDI.
- PERFORM Synth A/B/DX is deliberately **external-MIDI-only**. `InternalSynthOutput` rejects `PerformanceKeyboard`, `PerformanceKeyboardPoly`, and `Arpeggiator` sources.
- Sampler-page manual pad preview is internal-only and bypasses the generic musical-event router.
- SMF Player has a separate, mature output-route/mute/note-lifetime subsystem. It is not a generic GroovePuter track-output owner.

Therefore sequencer and manual/performance semantics are currently different by design/history, not by an explicit track setting.

The research confirms that **INTERNAL / MIDI / LAYER is sufficient as the user-facing output-intent axis** for Synth A, Synth B, and logical Drums, provided it is kept strictly orthogonal to mute, route, device profile, connection, clock, transport, and drum sound-source state. No fourth user-facing output state is justified by the frozen code.

`Sampler` should **not** receive an independent generic MIDI output mode in 0.9.6. It is currently an internal sound layer of the Drums musical path. `samplerEnabled`, pad assignment, choke, reverse, pitch, and future 0.9.5 kit state remain sampler/source concerns. Drums output intent decides whether the logical drum event has an internal side, an external side, or both.

The recommended production implementation is a small authoritative per-logical-track output state that reuses existing queues, target generations, wire ownership, scoped cleanup, and `AudioMutationGate`. It must not introduce a second MIDI scheduler, another active-note table, or a generic routing framework.

The largest architecture defect is **source-dependent ownership**: Pattern/Song and PERFORM can produce different destinations for the same logical Synth A/B track with no visible track-level intent. A second defect is a partially disconnected historical configuration model: `MidiOutputSettings` persists source enable/channel/route data, but the Cardputer settings session currently applies only profile/clock/follow state while the active `UsbMidiOutput` is built from a separate static `UsbMidiRouteConfig`.

A migration caveat is P0: no single legacy enum default can preserve every current source behavior. `LAYER` preserves Pattern/Song A/B/Drums behavior when MIDI is available but would make old PERFORM A/B newly local; `MIDI` preserves PERFORM but kills old local Pattern sound; `INTERNAL` preserves standalone Pattern sound but stops old external Pattern output. Production must resolve this explicitly and visibly instead of pretending there is a neutral default.

**Recommendation: GO — INTERNAL/MIDI/LAYER confirmed, but production is WAITING on 0.9.5 FINAL plus a mandatory routing/sampler delta re-audit and an explicit legacy-migration decision.**

---

## 2. Frozen research baseline

Research source of truth:

```text
repository: Ostrovsky42/GroovePuter
release:    0.9.4 FINAL
SHA:        d3db4e48ebc08862bdaf9f62532414f009839192
```

`dev_0.9.4` was verified to point to this exact SHA when this research branch was created.

This is **only the research baseline**.

Production 0.9.6 must not branch from 0.9.4. Before production:

1. wait for 0.9.5 FINAL;
2. record the exact 0.9.5 FINAL SHA;
3. compare 0.9.5 FINAL against `d3db4e48...` for every file/symbol named in this document;
4. repeat focused audits of Drums/Sampler trigger paths, Scene sampler state, MIDI routing, note cleanup, and UI integration;
5. update this design if 0.9.5 changed any assumption;
6. create the first production branch only from 0.9.5 FINAL.

No compatibility shim should be built between this research model and an unfinished 0.9.5 sampler API.

---

## 3. Current Output Architecture Map

### 3.1 Pattern / sequencer / Song

Current Synth A/B flow:

```text
Pattern/Song step
  -> MiniAcid::triggerSynthStep_()
     -> internal synthVoices_[A/B]->startNote(...)
     -> MiniAcid::publishPatternNoteOn_()
        -> MusicalEventQueue
        -> ScheduledMusicalEventQueue
        -> MidiDispatchTask
        -> UsbMidiOutput
        -> physical MIDI route
```

Current Drums flow:

```text
Pattern/Song drum step
  -> MiniAcid::triggerDrumVoice_()
     -> PatternPublishingDrumVoice::triggerX()
        -> underlying internal DrumSynthVoice::triggerX()
        -> publishPatternDrumTrigger()
           -> MusicalEventQueue -> MidiDispatchTask -> UsbMidiOutput
     -> if sampleStore present:
        DrumSamplerTrack::triggerPad()

same audio frame
  -> internal drum rendering
  -> DrumSamplerTrack::processFrame()
  -> master mix
```

Evidence owners:

- `src/dsp/miniacid_engine.cpp`
  - `triggerSynthStep_`
  - `publishPatternNoteOn_`
  - `publishPatternNoteOff_`
  - `publishPatternAllNotesOff_`
  - `triggerDrumVoice_`
  - Song uses the same active pattern trigger path rather than a second MIDI renderer.
- `src/dsp/pattern_drum_event_tap.h`
  - `PatternPublishingDrumVoice`
  - `publishPatternDrumTrigger`
- `src/input/musical_event_queue.h`
- `src/midi/scheduled_musical_event_queue.h`
- `src/platform/cardputer_usb_midi_transport.cpp`
- `src/midi/usb_midi_output.cpp`

Consequences:

- Pattern A/B currently has internal sound independent of the router.
- MIDI publication is additive to the direct internal trigger.
- Pattern Drums currently has at least two internal sound layers (drum synth plus optionally sampler) and an additive MIDI path.
- `Song` does not own an output model; it selects material that reaches the same trigger functions. This is correct and should be preserved.
- Phrase material that is inserted/generated into normal Song/Pattern storage inherits the same playback path. Phrase should not gain a separate output model.

### 3.2 PERFORM / manual input

Current flow:

```text
Cardputer keys / Performance Tools
  -> PerformanceKeyboard
  -> MusicalEventRouter
     -> InternalSynthOutput
        -> rejects PerformanceKeyboard / PerformanceKeyboardPoly / Arpeggiator
     -> bounded USB MIDI control sink
        -> MidiDispatchTask
        -> UsbMidiOutput
```

`src/input/internal_synth_output.cpp` explicitly makes PERFORM external-MIDI-only for Synth A/B/DX. `PerformanceVoiceMode::Mono/Poly` is receiver allocation policy, not local/output ownership.

`src/ui/pages/perform_page.cpp` exposes:

- NOTE on/off;
- target Synth A/B/Drums/DX;
- MIDI channel/SEQTRAK drum lanes;
- receiver MONO/POLY;
- velocity and generated performance tools.

It does **not** expose track output intent.

### 3.3 Sampler manual path

`src/ui/pages/sampler_page.cpp` calls `DrumSamplerTrack::triggerPad()` under the audio guard for pad keys/prelisten. This path does not publish a generic `MusicalEvent` and therefore does not send external MIDI.

The page label `LAYER: ON/OFF` means **internal sample layer enabled**, not output layering. Future output UX must not reuse this label ambiguously.

### 3.4 SMF Player

SMF is a separate external-MIDI playback subsystem:

```text
SMF producer/lookahead
  -> ScheduledSmfMidiEventQueue
  -> per-SMF-track mute
  -> per-SMF-track destination + route revision
  -> bounded track note ownership
  -> MidiDispatchTask / UsbMidiOutput wire owner
```

Key owners:

- `src/midi/smf_track_output_route.h`
- `src/midi/smf_track_note_ownership.h`
- `src/midi/smf_track_mute.h`
- `src/midi/scheduled_smf_midi_event_queue.h`
- `src/ui/pages/sequencer_hub_page_midi.cpp`

This is mature evidence for safe live transitions, but it is **not** the object that should become the GroovePuter A/B/Drums output model.

### 3.5 Router and physical transport

`MusicalEventRouter` is a small fan-out. It has no output-intent policy; it simply sends a `MusicalEvent` to every registered sink.

`MidiDispatchTask` is the sole mutable TinyUSB/`UsbMidiOutput` owner. That ownership is correct and must remain.

`UsbMidiOutput` owns physical-note/reference-count cleanup and connection status. It is not the correct owner for musical user intent because it operates after source/target decisions have already been made.

---

## 4. Track Ownership Matrix

| Track | Internal engine owner | Internal mute owner | MIDI send owner | MIDI target/route owner | NoteOn/NoteOff owner | Sequencer path | Manual/performance path | Persistence | Current UI | Tests / evidence |
|---|---|---|---|---|---|---|---|---|---|---|
| Synth A | `MiniAcid::synthVoices_[0]`, direct `triggerSynthStep_` | `MiniAcid::mute303` mirrored through SceneManager | Pattern: `publishPatternNoteOn_/Off_` -> scheduled queue; PERFORM: `PerformanceKeyboard` -> control queue | Pattern/perf physical channel currently `UsbMidiRouteConfig`; historical `MidiOutputSettings::synthAChannel` exists but is not applied by Cardputer session | Internal gate owned by MiniAcid voice; external Pattern generation + `UsbMidiOutput` lane/wire owners; PERFORM physical/generated ownership in `PerformanceKeyboard` + `UsbMidiOutput` | current effective LAYER when MIDI READY | external-MIDI-only | synth/mute in Scene; MIDI companion config in NVS; no OutputMode | Sequencer + PERFORM target/CH, no OUT state | `test_pattern_midi_source_regressions.py`, `test_usb_midi_output.cpp`, `test_performance_keyboard.cpp` |
| Synth B | same, `synthVoices_[1]` | `mute303_2` | same B path | static B channel / dormant settings field | same target-scoped lifecycle | current effective LAYER when MIDI READY | external-MIDI-only | same split | same | same |
| Drums | `PatternPublishingDrumVoice` wraps internal `DrumSynthVoice`; sampler is additional internal layer | eight `MiniAcid` drum mute flags | `publishPatternDrumTrigger` -> scheduled queue; bounded drum-gate scheduler creates NoteOff | static native SEQTRAK mapping in `UsbMidiOutput`; dormant `MidiOutputSettings::drumRoutes[8]` | per-lane/wire owner counts + `PatternDrumGateScheduler`; target generation for Drums | internal drum + optional sampler + external MIDI when READY | PERFORM Drums is external MIDI; no internal drum alias through `InternalSynthOutput` | drum mutes/engine/sample state in Scene; route config NVS model exists but runtime application is incomplete | Sequencer tracks; PERFORM native CH1-7 | pattern MIDI source regressions + USB output tests |
| Sampler | `DrumSamplerTrack` + `SamplerPool` | `samplerEnabled` is source-layer enable; drum voice mute also gates sequenced trigger | no independent sampler MIDI producer | none; sequenced external event is the logical Drums event | sample-voice/choke owner internally; external lifetime belongs to Drums MIDI path | triggered beside internal drum when pad assigned + layer enabled | SamplerPage pad/prelisten is internal-only | Scene pad state + `samplerEnabled`; 0.9.5 will own reliability/kit changes | SamplerPage `LAYER`, pads, sample params | sampler regressions from 0.9.3/0.9.4; re-audit after 0.9.5 |

Additional current owners:

- **SMF Player**: external-only file playback with its own per-track mute/route/note ownership; do not force it into the groove-track OutputMode unless a separate requirement asks for local SMF rendering.
- **Voice**: Song has a Voice lane, but Voice output ownership is not required for the first 0.9.6 slice. Treat as explicitly deferred until a concrete internal/external producer path is demonstrated.

Hardware evidence for this research branch: **none newly collected**. Historical PRs contain hardware acceptance claims for specific MIDI features, but this document does not promote those claims to a new 0.9.6 hardware result.

---

## 5. Hidden / historical functionality

| Classification | File / symbol / history | Status on 0.9.4 | Relevance |
|---|---|---|---|
| RECOVERY + CONSOLIDATION | PR #11, `src/midi/midi_companion_settings.h`, `MidiOutputSettings` | merged historical model still exists | Already models master/source enables, A/B channels, drum routes, profiles and persistence. Do not invent another device-route model without understanding why runtime integration drifted. |
| RECOVERY finding | `src/platform/cardputer_midi_settings_session.cpp` | live but partial | Loads the full settings record, then applies only profile to transport capabilities and clock/follow to `TransportClockRuntime`. Pattern/live/drum enables and channels are not bound to the active `UsbMidiOutput`. |
| CURRENT OWNER | merged PR #158; `SmfTrackOutputRouteState` | present in frozen code | Mature immediate live reroute: destination revision, stale lookahead drop, scoped old-route release, no transport pause/global panic. Reuse lifecycle principles, not the full SMF framework. |
| HISTORY / RECOVERY | PR #244 | open recovery PR, code behavior present in current ancestry | Documents that merged #158 behavior was once lost on a divergent line and had to be restored. Shows route ownership is vulnerable to ancestry/UI drift. |
| CURRENT OWNER | merged PR #162; `PerformanceKeyboard` + `InternalSynthOutput` | present | Deliberately external-only PERFORM, receiver-owned MONO/POLY, exact physical key lifetime. This source-specific behavior is the main conflict a unified track OutputMode must resolve. |
| HISTORY / RECOVERY | PR #245 | open recovery PR, code behavior present in current ancestry | Documents recovery of #162's performance slice after ancestry drift. |
| CURRENT OWNER | `ScheduledMusicalEventQueue` | present | Fixed 128-slot SPSC queue, target generations, target-scoped pending cleanup. This should remain the Pattern MIDI lifecycle primitive. |
| CURRENT OWNER | `UsbMidiOutput` wire ownership | present | Shared physical channel/note reference ownership prevents one logical producer from releasing another producer's note. Must be reused. |
| CURRENT OWNER | `SmfTrackMuteState` | present | Strong evidence that mute and route can be separate state; mute suppresses new NoteOn but still permits cleanup NoteOff. |
| CURRENT OWNER | `DrumSamplerTrack::enabled_` / Scene `samplerEnabled` | present | This is sampler sound-source/layer state, not MIDI output intent. |

### Historical lesson from PR #11

PR #11 is the closest old attempt at a generalized MIDI companion model. It intentionally kept device/global routing outside Scene and included:

- source enables;
- Synth A/B channels;
- per-drum routes;
- profile presets;
- bounded persistence;
- target-scoped cleanup ideas.

However, the current frozen runtime no longer treats that structure as the active route owner. Therefore #11 should **not** simply be "restored" wholesale. Its useful pieces are the route/profile separation and compact persistence. Its source-specific enable flags are not a replacement for a canonical `INTERNAL/MIDI/LAYER` track intent.

---

## 6. Current failure modes

These are code-supported architecture risks. They are not all hardware-observed failures.

| Failure mode | Current evidence | Assessment |
|---|---|---|
| Stuck note after output/route transition | SMF solved this with revision + scoped release; generic Pattern output has no live OutputMode transition at all | HIGH production risk if new mode is added naively |
| NoteOff sent to wrong destination after route A -> B | Pattern A/B route is currently static, so there is no accepted generic live route transition contract | HIGH once route becomes mutable |
| Unexpected local sound | PERFORM is explicitly external-only while Pattern is local+MIDI | ALREADY a semantic split; naive unification can create surprise |
| Accidental local+MIDI double trigger | Pattern intentionally does both when MIDI is available | ALREADY current behavior, but not represented as user intent |
| Route state lost/not applied | `MidiOutputSettings` loads values not bound into `UsbMidiOutput` runtime | CURRENT architectural defect |
| Mute changes routing | no evidence that mute rewrites route; Pattern mute gates trigger and releases current Pattern note | NOT current defect; preserve orthogonality |
| Connection changes intent | there is no explicit intent; `UsbMidiOutput::accepts()` depends on mounted status and disconnect clears active wire state | availability affects audibility but should not become state mutation |
| Song vs Pattern mismatch | Song uses the same MiniAcid trigger functions | no independent output mismatch found |
| Phrase vs Pattern mismatch | Phrase material reaches normal Song/Pattern playback | no independent output owner found; keep it that way |
| PERFORM vs sequencer mismatch | `InternalSynthOutput` explicitly filters PERFORM while sequencer directly sounds internal voice | CONFIRMED main defect |
| Sampler vs sequencer/manual mismatch | sequenced drum can trigger sample + MIDI; SamplerPage preview triggers sample only | CONFIRMED, but sampler should remain internal source rather than gain independent MIDI architecture |
| Global panic used for normal reroute | SMF explicitly avoids it; Pattern queue has target-scoped barriers | must remain prohibited for normal OutputMode/route transitions |

---

## 7. Proposed ownership model

### 7.1 User-facing axis

The code evidence supports exactly three settled user intents:

```cpp
enum class TrackOutputMode : uint8_t {
    Internal,
    Midi,
    Layer,
};
```

The exact type/name is a production detail; the semantic contract is the important result.

For each logical groove track:

**INTERNAL**

```text
logical musical event
  -> internal side only
  -> no external NoteOn publication for this track
```

**MIDI**

```text
logical musical event
  -> external MIDI side only
  -> internal engine/source state remains configured but does not sound
```

**LAYER**

```text
one logical musical event
  -> internal side
  + external MIDI side
```

For Drums, "internal side" means the current internal drum sound graph: drum synth plus sampler layer if `samplerEnabled` and the pad is assigned. OutputMode must not decide which internal drum source is selected.

### 7.2 What is not an OutputMode state

Do not add these as hidden fourth/fifth output modes:

- muted;
- MIDI unavailable/disconnected;
- SEQTRAK vs General MIDI;
- route/channel selection;
- sampler layer ON/OFF;
- synth engine TYPE;
- receiver MONO/POLY;
- stopped/playing;
- clock master/slave;
- SMF AUTO/CH1..CH10 route;
- track selection.

### 7.3 Canonical owner location

Use one authoritative compact runtime owner per logical groove track. Do not store one mutable copy in MiniAcid and another in USB MIDI settings.

Preferred production shape:

- one small control/runtime state object for Synth A, Synth B, Drums;
- realtime read is allocation-free and bounded;
- MiniAcid trigger path asks whether the internal side is enabled;
- MIDI publication path asks whether the external side is enabled;
- control transitions coordinate internal release + existing queue invalidation/scoped external cleanup.

Do **not** move TinyUSB ownership out of `MidiDispatchTask`.

### 7.4 Candidate evaluation

| Candidate | Maturity | Reuse | Value | ADV RAM | RT risk | Note risk | UI/migration | Verdict |
|---|---|---|---|---|---|---|---|---|
| Add one `midiEnabled` bool and infer local as inverse | early | LOW | MEDIUM | LOW | LOW | MEDIUM | HIGH | REJECT: cannot represent LAYER and encourages mute/route conflation |
| Make existing `MidiOutputSettings` source flags canonical | partial/substantial persistence | MEDIUM | MEDIUM | LOW | MEDIUM | HIGH | HIGH | REJECT AS-IS: device-global, source-specific, mixed with profile/clock and partially disconnected |
| Small per-track INTERNAL/MIDI/LAYER owner + reuse current route/lifecycle primitives | partial foundation / near-production primitives | HIGH | HIGH | LOW | LOW-MEDIUM | MEDIUM until transitions locked | MEDIUM | RECOMMENDED |
| Transplant SMF per-track routing/profile framework into groove tracks | substantial but wrong domain | MEDIUM | MEDIUM | MEDIUM-HIGH | MEDIUM | MEDIUM | HIGH | REJECT: unnecessary framework and duplicated state |

Classification of recommended work:

```text
CURRENT OWNER reuse  -> queues, USB owner, wire ownership, MiniAcid engine/mutes
RECOVERY             -> learn from #11 settings and #158/#162 historical contracts
CONSOLIDATION        -> one track OutputMode instead of source-dependent behavior
HARDENING            -> live transitions / scoped note cleanup
EXTENSION            -> tiny authoritative output-state representation
NEW ARCHITECTURE     -> NOT REQUIRED
```

---

## 8. Mute relationship

Decision:

```text
OutputMode ⟂ Mute
```

Mute is a temporary sounding gate. It does not express destination preference.

Required semantics:

```text
INTERNAL + MUTED -> silent, intent remains INTERNAL
MIDI     + MUTED -> silent, intent remains MIDI
LAYER    + MUTED -> silent, intent remains LAYER
```

On mute:

- suppress new internal triggers;
- suppress new external NoteOn;
- release current internal active note/voice ownership where applicable;
- invalidate/release current external target ownership using existing scoped lifecycle primitives;
- still allow cleanup NoteOff paths to complete;
- never rewrite route or OutputMode.

This is consistent with current SMF mute design and current MiniAcid mute behavior, where mute is stored separately from routing.

---

## 9. Route relationship

Decision:

```text
OutputMode answers WHETHER external MIDI participates.
Route answers WHERE that external side goes.
```

The route may include:

- MIDI channel;
- per-drum native destination;
- current device-specific mapping.

The route must not decide whether local audio is enabled.

Current migration issue:

- Pattern/performance routes are effectively defined by static `UsbMidiRouteConfig` in `cardputer_usb_midi_transport.cpp`.
- `MidiOutputSettings` has A/B/drum channels/routes but the Cardputer settings session does not apply them to `UsbMidiOutput`.
- SMF has a separate live route owner with revisions.

0.9.6 should consolidate only what is required to make groove-track external routing safe and authoritative. It should **not** build Device Profiles 2.0.

---

## 10. Note lifecycle

### 10.1 Core invariant

```text
NoteOff follows the owner(s) created by the corresponding NoteOn.
```

A mode/route transition is a lifecycle boundary, not permission to send the later NoteOff to a newly selected destination.

### 10.2 Transition policy

Preferred minimal safe policy: transitions are immediate control changes, but they **do not synthesize replacement NoteOn events on newly added destinations for notes that were already held/sounding**.

This avoids unmatched NoteOff, artificial re-attacks, and cross-source restoration behavior.

| Transition | Required behavior |
|---|---|
| INTERNAL -> MIDI | release internal active owner; invalidate stale external target generation before accepting future external events; do not create a mid-note MIDI NoteOn |
| MIDI -> INTERNAL | invalidate queued external events; scoped release at the original external destination; enable internal side for future NoteOn only |
| MIDI -> LAYER | keep valid existing external owner; enable internal side for future NoteOn; no synthetic local re-attack |
| LAYER -> INTERNAL | scoped release external owner + stale external queue; keep internal owner that is already valid; future events internal only |
| LAYER -> MIDI | release current internal owner; keep valid external owner; future events MIDI only |
| route A -> B | increment/invalidate route generation/revision, release active external owner at A, discard stale A lookahead, future NoteOn uses B; no synthetic NoteOn at B |
| mute | terminate active owners on all currently participating sides; preserve OutputMode and Route |
| stop | use existing Pattern target-scoped AllNotesOff/generation barrier; no global channel panic as normal path |

For physical PERFORM held keys, changing OutputMode should terminate owners removed by the transition and wait for the next genuine key NoteOn/retrigger before creating a new owner. Do not re-create the historical "restore held note" behavior.

### 10.3 Existing mechanisms to reuse

Pattern:

- `ScheduledMusicalEventQueue::invalidateTarget()`;
- per-target generations for Synth A/B/Drums;
- `takePendingAllNotesOffMask()`;
- existing bounded control queue cleanup;
- `UsbMidiOutput` lane ownership and `wireOwners_` reference counting.

SMF design precedent:

- destination revision captured at producer scheduling boundary;
- stale old-route events rejected at dispatch;
- scoped track release;
- cleanup event survives a second rapid route change.

Do not introduce a second MIDI dispatch task or duplicate active-note matrix.

### 10.4 Internal-side cleanup

Synth A/B mode transitions that remove the internal side must release the current internal voice under the accepted audio mutation boundary without changing synth TYPE/parameters.

Drums are mostly one-shot, but sampler pads can loop. A Drums transition that removes the internal side must stop any internal sample/drum ownership required to guarantee silence while preserving pad assignments and sampler configuration. Exact sampler stop APIs must be re-audited after 0.9.5 FINAL.

---

## 11. Persistence decision

### 11.1 Proposed owners

**Output intent:** Scene-level musical state is the preferred final owner, because the choice changes the reproducible sounding behavior of Synth A/B/Drums and should follow Song/Pattern playback.

**Mute:** keep current Scene ownership. Do not encode mute inside OutputMode.

**Route / device profile / physical connection preferences:** keep outside Scene in device/global MIDI settings unless a later product requirement proves project-specific routing is necessary.

**SMF per-file route overrides:** keep in the SMF/profile owner; do not migrate them into groove-track Scene output state.

### 11.2 P0 legacy migration finding: there is no lossless single enum default

Frozen 0.9.4 behavior is source-dependent:

```text
Pattern/Song A/B: internal + MIDI when MIDI available
PERFORM A/B:      MIDI only
```

Therefore an old Scene with no OutputMode key cannot be mapped to one of INTERNAL/MIDI/LAYER while preserving both source behaviors:

- `INTERNAL` loses external Pattern output and breaks external PERFORM;
- `MIDI` loses local Pattern audio;
- `LAYER` preserves Pattern playback but introduces local PERFORM sound.

This is a real migration conflict, not a naming problem.

Production must not silently hide it. Before 0.9.6-F ships, choose and test one explicit migration policy. Candidate policies:

1. visible one-time migration choice per old project/Scene;
2. a documented versioned legacy-compatibility decode phase that is **not** exposed as a fourth settled OutputMode and is retired on explicit user choice/save;
3. intentionally choose one canonical old behavior and accept/document the change only after hardware/product review.

Research does **not** select one of these without product/hardware evidence.

For brand-new 0.9.6 projects, the safest default is `INTERNAL`: connection must not cause a new project to start sending MIDI. This new-project default does not solve migration of old projects.

---

## 12. UI proposal

No new top-level page is justified.

Preferred interaction surfaces:

1. existing track/sequencer detail for Synth A/B/Drums shows a compact always-readable status, e.g. `OUT: INT`, `OUT: MIDI`, `OUT: LAYER`;
2. existing MIDI/route UI edits destination details, not musical output intent;
3. PERFORM shows the same selected track OutputMode rather than implying that target/channel alone defines intent;
4. mute indication remains independent.

Do not overload the SamplerPage label `LAYER: ON/OFF`. That label means sample source layer. If production keeps the word `LAYER` for OutputMode, the sampler UI should clarify its label (for example `SAMPLE: ON/OFF` or another existing-convention-safe label) only if needed to remove ambiguity; do not redesign the page.

Unavailable external destination:

```text
OUT: MIDI  [UNAVAILABLE]
OUT: LAYER [MIDI UNAVAILABLE]
```

Do not silently rewrite the mode to INTERNAL.

---

## 13. Proposed 0.9.6 scope

Release theme: **Unified Output Ownership**.

### P0

- one explicit output-intent owner for Synth A/B/Drums;
- INTERNAL/MIDI/LAYER semantics shared by Pattern/Song and PERFORM;
- OutputMode orthogonal to mute/route/connection/profile;
- target-scoped note lifecycle for live mode/route changes;
- no stale queued NoteOn after external side is removed or rerouted;
- internal active-note/sample cleanup when internal side is removed;
- no mutation of synth TYPE/parameters or sampler assignments when output changes;
- no silent connection-driven intent changes;
- regression lock before semantic changes;
- persistence only after migration policy is explicit.

### P1

- compact output indicator/control in existing UI;
- hardware validation with real SEQTRAK;
- diagnostics sufficient to distinguish `intent=MIDI` from `connection=WAIT/OFF`;
- Save/reboot/Load once persistence stage is reached.

### Explicitly deferred

- Device Profiles framework/generalization;
- second device/FM1;
- arbitrary MIDI input/controller mapping;
- SMF local rendering;
- sampler architecture changes;
- any new audio engine/source architecture.

---

## 14. Proposed production stages

Production branch creation for every stage below waits for 0.9.5 FINAL or an accepted descendant of it.

### 0.9.6-A — Ownership regression lock + delta audit

Purpose:

- repeat this audit on exact 0.9.5 FINAL;
- encode behavioral tests before semantic changes.

Production behavior: **NONE**.

Required tests:

- Pattern Synth A/B currently produce internal trigger + external publication;
- PERFORM A/B currently external-only;
- Pattern Drums internal synth + sampler source + MIDI publication topology;
- SamplerPage manual trigger is internal-only;
- current target-scoped Pattern AllNotesOff generation behavior;
- SMF live reroute old-destination NoteOff/stale revision behavior;
- mute does not rewrite route;
- disconnect does not rewrite a persisted intent (once intent exists in later stages; in A lock current connection state only).

Hardware: no new hardware behavior required, but confirm 0.9.5 sampler smoke baseline still matches before proceeding.

Dependency: exact 0.9.5 FINAL.

### 0.9.6-B — Canonical output contract

Purpose:

- add the smallest authoritative `Internal/Midi/Layer` state for Synth A/B/Drums;
- no hierarchy, no new scheduler, no new queue.

Boundary:

- state + pure policy/helpers + lifecycle hooks only;
- no persistence/UI yet;
- no sampler storage changes.

Tests:

- truth table `internalEnabled/externalEnabled` for all three modes;
- one authoritative mutable copy;
- mute/route/connection cannot mutate mode;
- fixed RAM delta measured.

Hardware: compile + DRAM baseline; behavior can remain inaccessible except tests until vertical migration begins.

### 0.9.6-C — Synth A/B vertical migration

Purpose:

- make Pattern/Song and PERFORM A/B read the same output intent.

Required behavior:

- INTERNAL local only;
- MIDI external only;
- LAYER both;
- synth TYPE/params untouched;
- current PERFORM MONO/POLY/velocity remains receiver-specific and independent;
- held-note mode changes follow section 10 lifecycle.

Tests:

- sequencer/manual equivalence per mode;
- Song same as Pattern;
- active-note transitions;
- same physical channel/note sharing does not cut other owners.

Hardware: focused Synth A/B + SEQTRAK smoke before moving to Drums.

### 0.9.6-D — Drums / Sampler vertical migration

Purpose:

- apply logical Drums OutputMode without redesigning sampler.

Required behavior:

- INTERNAL = current internal drum graph only;
- MIDI = external drum route only;
- LAYER = both;
- `samplerEnabled`, pad mapping, choke, reverse, pitch stay independent;
- output switch performs no SD work and does not reload samples;
- drum mute remains per-voice mute;
- sampler manual page does not invent a separate MIDI target.

Tests:

- eight drum voices;
- retrig/flam/roll gate safety;
- sampler enabled/disabled cross-product with three output modes;
- loop/choke cleanup when removing internal side;
- no sample allocation/load caused by output change.

Hardware: ADV memory/audio + SEQTRAK drums + loaded 0.9.5 kit.

### 0.9.6-E — Live transition and route hardening

Purpose:

- make all output/route transitions safe while transport and notes are active.

Reuse:

- Pattern target generations;
- bounded control queue;
- `UsbMidiOutput` wire ownership;
- SMF route-revision/scoped-release design principles.

Do not transplant the entire SMF routing subsystem.

Tests:

- INTERNAL -> MIDI;
- MIDI -> INTERNAL;
- MIDI -> LAYER;
- LAYER -> INTERNAL/MIDI;
- route A -> B;
- rapid route changes;
- mute/unmute;
- stop/scene/Song boundary;
- stale event rejection;
- no global panic for normal change.

Hardware: live switches while playing and held-key tests.

### 0.9.6-F — Persistence + UX

Purpose:

- persist output intent only after runtime semantics are stable;
- expose state in existing track UI;
- resolve legacy migration explicitly.

Tests:

- missing legacy key;
- valid new key;
- invalid/corrupt value -> safe transactional failure/default policy;
- Save/reboot/Load;
- mute and route persist independently according to their owners;
- unavailable MIDI retains intent;
- no automatic change from cable connection/device profile.

Hardware: reboot/load + cable disconnect/reconnect.

### 0.9.6-G — ADV + SEQTRAK release acceptance

Automated:

- full Core host suite;
- SDL;
- Cardputer ADV normal + fixed DRAM;
- Cardputer ADV SEQTRAK MIDI-only;
- focused Output Ownership regressions;
- sampler reliability regressions inherited from 0.9.5.

Hardware:

- exact candidate SHA;
- Synth A/B/Drums INTERNAL/MIDI/LAYER;
- sampler kit retained;
- live transitions;
- mute;
- route switch;
- Pattern/Phrase/Song;
- PERFORM velocity/MONO/POLY;
- Save/reboot/Load;
- 30-minute soak with heap/largest block/underrun/queue diagnostics.

---

## 15. Memory/performance budget

Principle: **HEADROOM WON, NOT AUTOMATICALLY SPENT.**

Recommended state cost:

- one tiny mode per logical Synth A/B/Drums;
- preferably one packed bounded atomic/control word or equivalently small static state;
- no duplicated active-note table;
- no second event queue;
- no retained route strings;
- no new audio buffer;
- no sampler-pool growth.

Budget assessment:

| Resource | Assessment | Requirement |
|---|---|---|
| Fixed internal RAM | LOW | target is only a few bytes / <= one small control object; exact delta UNKNOWN UNTIL MEASURED |
| Dynamic RAM | LOW | zero new event-path heap allocation; no persistent dynamic routing tables |
| Flash | LOW-MEDIUM | policy + tests + small UI/persistence handling; measure final binary |
| Active-note storage | NO NEW TABLE | reuse current MiniAcid/Performance/USB ownership; extend minimally only if route-original identity cannot otherwise be preserved |
| Event queues | NO GROWTH PLANNED | reuse bounded Pattern/control queues |
| Audio CPU | LOW | one bounded mode test around trigger/render decision; no extra DSP |
| MIDI dispatch CPU | LOW | existing generation/currentness and owner checks; possible small mode/route revision check |
| Realtime risk | LOW-MEDIUM | risk is lifecycle ordering, not compute cost |
| MIDI timing risk | LOW if no queue/scheduler changes | hardware acceptance still mandatory |

No fixed-RAM claim is accepted until the ADV DRAM job and runtime heap diagnostics run on the exact production candidate.

---

## 16. Compatibility

### Scene

- currently owns synth/drum mutes and musical engine/sample state;
- proposed owner for new output intent;
- migration of missing output fields is unresolved P0 because current behavior is source-dependent.

### Project

- no evidence that a second Project-level output owner is needed if Scene remains the saved musical setup;
- do not duplicate Scene OutputMode in project metadata.

### Settings

- keep device/profile/route/clock environment concerns in MIDI settings;
- repair only the runtime binding needed by Output Ownership; do not use this release to create a profile registry.

### Song

- already uses the same MiniAcid pattern trigger path; should inherit track OutputMode automatically.

### Phrase

- should inherit through normal Song/Pattern material; no separate output state.

### Pattern

- primary migration target; direct internal trigger and MIDI publication must become two sides controlled by one track intent.

### Sampler

- 0.9.5 owns WAV/kit/SampleRef/transactional loading/relink/memory work;
- 0.9.6 only gates whether the logical Drums event has an internal side;
- output switch must never load/relink a sample.

### MIDI Hub / SMF Player

- retain SMF track mute/route ownership;
- reuse safe lifecycle ideas, not data model.

### SEQTRAK

- use as hardware receiver;
- channels/CC26 remain device semantics, not OutputMode.

### MIDI Import

- imported material becomes normal musical material; generation/import must not choose output destination.

### Old projects

- must not silently lose local sound, begin unexpected external send, or silently fall back from MIDI to INTERNAL;
- because no lossless default exists, migration policy must be an explicit release decision with regression tests.

---

## 17. Explicit non-goals

0.9.6 does **not** include:

- Generic MIDI / SEQTRAK Device Profile registry;
- second MIDI device integration;
- FM1 integration;
- BLE MIDI;
- ESP-NOW MIDI;
- USB Host redesign;
- MIDI Learn;
- arbitrary controller mapping;
- new MIDI input architecture;
- sampler WAV loader changes;
- sampler canonical kit redesign;
- SampleRef changes;
- sampler streaming;
- slicing;
- recording;
- waveform editor;
- round-robin;
- sampler-pool expansion;
- new synth engines;
- Tape recovery;
- Voice recovery;
- Recorder recovery;
- generation rewrite;
- Genre/Feel redesign;
- Texture return;
- new Song/Phrase architecture;
- local SMF synth rendering;
- generic plugin framework;
- large routing-framework rewrite;
- a second MidiDispatchTask;
- a second transport scheduler;
- a duplicate active-note matrix.

If a production implementation appears to require one of these, stop and split the release instead of silently expanding scope.

---

## 18. Open risks

Only unresolved issues that can still change production design are listed.

1. **Legacy migration policy.** No one canonical enum value preserves both old Pattern and old PERFORM behavior. This must be an explicit product decision before persistence ships.
2. **0.9.5 Drums/Sampler delta.** Transactional kit load and sampler reliability work may change the exact safe internal-side stop/voice lifecycle. Re-audit before 0.9.6-D.
3. **Mutable generic route implementation.** Frozen Pattern/performance routes are mostly static while the live mutable route solution is SMF-specific. Production must determine the smallest route-state extension that preserves original-destination NoteOff without duplicating SMF infrastructure.
4. **Sampler terminology.** SamplerPage already uses `LAYER` for source enable. A visible OutputMode `LAYER` may require a narrow label clarification.
5. **Held-note UX.** Research recommends no synthetic transfer NoteOn when a mode gains a new side. This needs hardware/musical acceptance on PERFORM before freeze.
6. **Disconnect/reconnect.** Current USB output clears active state on disconnect. New intent must survive the disconnect while wire ownership recovery remains bounded and no stale note is replayed on reconnect.

---

## 19. Go / No-Go recommendation

**GO — INTERNAL/MIDI/LAYER confirmed as the user-facing output-intent model.**

Implementation should be a **reduced ownership consolidation**, not a routing rewrite.

Execution status is simultaneously:

```text
GO   on architecture direction
WAIT on production branch until exact 0.9.5 FINAL + delta re-audit
```

Why GO:

- the user problem is real and visible in code: source-dependent output semantics;
- all three desired intents already exist implicitly in combinations of current paths;
- mature realtime/note-lifetime primitives already exist;
- mute, route, connection and device semantics can remain independent;
- no framework rewrite is required;
- expected RAM/runtime cost can remain very small.

Why WAIT before implementation:

- 0.9.5 owns active sampler reliability/kit work;
- Drums/Sampler lifecycle must be revalidated against its final exact SHA;
- the legacy Scene migration default is not losslessly derivable and must be resolved explicitly.

### First production PR after 0.9.5 FINAL

`0.9.6-A — Ownership regression lock + 0.9.5 delta audit`.

It should contain **tests/docs only and no semantic production change**. Its purpose is to freeze the exact 0.9.5 behavior of:

- Pattern local+MIDI A/B;
- PERFORM external-only A/B;
- Drums internal/sample/MIDI topology;
- sampler manual internal trigger;
- target-scoped Pattern generation cleanup;
- SMF live reroute/stale event/scoped NoteOff;
- mute/route separation;
- disconnect owner cleanup.

Only after that exact-head gate is green should 0.9.6-B introduce canonical output state.

### Mandatory 0.9.5 FINAL re-audit list

Compare these exact areas against this research baseline:

```text
GroovePuter.ino
src/dsp/miniacid_engine.{h,cpp}
src/dsp/pattern_drum_event_tap.h
src/sampler/drum_sampler_track.*
src/sampler/sample_store.*
src/sampler/sample_index.*
src/input/musical_event*
src/input/internal_synth_output.*
src/input/performance_keyboard.*
src/midi/usb_midi_output.*
src/midi/scheduled_musical_event_queue.h
src/midi/midi_companion_settings*
src/platform/cardputer_usb_midi_transport.cpp
src/platform/cardputer_midi_settings_session.cpp
src/ui/pages/perform_page.cpp
src/ui/pages/sampler_page.cpp
src/ui/pages/sequencer_hub_page_midi.cpp
scenes.{h,cpp}
Scene/Settings codecs and tests
```

Re-answer after the delta:

1. Does a drum hit still trigger internal drum + optional sample + MIDI at the same logical point?
2. Is Sampler still an internal Drums layer rather than an independent MIDI producer?
3. Does output switching require any new sampler stop/lifecycle API?
4. Did 0.9.5 add/change Scene sampler fields or load/apply ordering relevant to output persistence?
5. Did any 0.9.5 change touch `AudioMutationGate`, event queues, memory budget, or UI navigation?
6. Is the legacy migration conflict still exactly Pattern=LAYER-like vs PERFORM=MIDI-only?

If those answers materially change, update this contract before production. Do not add a shim to preserve a research-only API.
