# Architecture decisions for 0.9.25–0.9.35

Baseline: `feature/0.9.24` at `28918d9b78e9a527ce2b5ce201a465af5126f76a`. These decisions apply to the MIDI-first roadmap and preserve the user's implementation constraints.

## Accepted constraints

### AD-001 — Extend existing subsystem owners

**Decision:** extend the existing generation, Scene/Song/Phrase, MIDI output, note ownership, transport-clock, device-profile, storage and Undo owners. Add a new owner only when the source audit proves no current owner can safely represent the required behavior.

**Evidence:** the baseline already contains `UsbMidiOutput`, `MidiEndpointOwnershipTable`, `TransportClockRuntime`, profile route projections, Song harmonic state and codec, `PatternPagingService`, and the Scene materialization contract in `docs/architecture/GENERATION_COMPOSITION_MODEL.md`.

### AD-002 — Materialized Scene is canonical

**Decision:** generators and performance transformations prepare bounded candidates; after an explicit commit, the editable Scene is the source of truth. Playback, routing and export project that material without silently regenerating it.

**Evidence:** `GENERATION_COMPOSITION_MODEL.md` requires Scene authority after materialization and prohibits implicit changes to user edits.

### AD-003 — Musical intent and output routing stay separate

**Decision:** Genre, Rhythm, Feel, Harmony, Motif, Arrangement, Performance, Routing and Sound remain distinct responsibilities. Device profiles may select destinations and supported controls; they do not own notes or rewrite material.

**Decision:** Keep `MidiDeviceProfile` for external device identity. `MidiOutputSettings.enabled` is the independent destination switch: OFF means internal audio only; ON means internal audio plus the selected external MIDI profile. The Project → MIDI screen labels these states `INTERNAL` and `INT + MIDI`. Disabling external output gates Pattern, Performance, SMF and outbound transport messages; it does not disable internal synthesis or MIDI input. Both changes remain next-boot settings.

**Approaches considered:** Adding `Internal` to the device-profile enum would mix endpoint identity with destination enable and require special capability/profile restore rules. Adding a second persisted output-mode enum would duplicate the existing `enabled` bit and require a migration. Reusing the existing bit and exposing it as a separate UI row preserves the schema and keeps routing/profile ownership separate; this is the chosen approach.

**Evidence:** `MidiSettingsCodec` already persists the enable bit; startup route projections and `UsbMidiOutput` have output gates. The remaining direct outbound transport path is gated in the dispatcher. See `MIDI_CONTRACTS.md` and the 0.9.27 milestone entry.

### AD-004 — Real-time changes stay bounded

**Decision:** audio/MIDI critical paths use fixed-capacity state, no dynamic allocation, file I/O, blocking waits or expensive generation. Queues have explicit overflow/release policies. Measure worst-case work and memory changes at the owning boundary.

**Evidence:** existing MIDI queues and ownership tables are bounded; baseline fixed DRAM headroom is under 3.9 KiB against a provisional 191,488-byte gate. See `RESOURCE_BUDGET.md`.

### AD-005 — Storage compatibility is owned by each codec

**Decision:** version any changed persistent record; preserve an old reader or explicit migration; validate into staged state; commit transactionally; keep old fixtures. Do not silently accept partial project state.

**Evidence:** Scene loading stages before publication, Cardputer writes use temp/backup promotion, `.gpp` is versioned, and MIDI profile settings have versioned codecs. The baseline does not expose one global project schema version.

### AD-006 — Hardware status is evidence-scoped

**Decision:** milestones 0.9.25–0.9.34 are `HOST-VALIDATED / HARDWARE-UNVERIFIED`. The 0.9.35 artifact is `READY_FOR_OWNER_HARDWARE_TEST`; only owner confirmation may change that to `HARDWARE-ACCEPTED`.

### AD-007 — Performance macros are transient projections

**Decision:** Macro state produces a separate fixed-capacity playback candidate from a const phrase source. It does not access Scene, storage or Undo and is not serialized. There is no implicit or explicit permanent apply operation in 0.9.28. A future materialization action, if needed, must use the existing phrase edit/commit boundary.

**Evidence:** `src/performance/performance_macro_engine.h` validates into a local candidate and assigns the destination only after validation; host tests verify unchanged source, transactional rejection, deterministic seed behavior and zero heap allocations.

### AD-008 — Live macros extend the existing playback owner

**Decision:** publish all four session values through one packed 32-bit atomic snapshot. Apply a bounded per-event projection to the selected Pattern or Phrase onset immediately before `RuntimeSynthPlaybackState` accepts it. Keep internal audio and Pattern MIDI on the existing shared event and routing path. Song playback follows its selected material through the same path.

**Evidence:** mock-MIDI P0 runtime characterization compares the transformed Pattern event against the fake endpoint NoteOn; the P3 phrase-relative runtime test verifies the Phrase event is transformed while its source buffer remains unchanged. Firmware and SDL builds pass. No physical listening or MIDI evidence is claimed.

**Bound:** TENSION's next-onset search inspects at most 16 Pattern events or 128 Phrase events and runs only while TENSION is nonzero. The audio path does not call the batch `apply()` function or allocate a Phrase buffer.

## Decisions still required by implementation

- Whether a later user workflow should provide an explicit macro-to-phrase materialization action; 0.9.29 remains transient and does not commit the playback view.
- How existing Song/FORM data expresses INTRO, MAIN, DEVELOPMENT, BREAK, BUILD, DROP and OUTRO without adding seven storage types.
- SMF export format (Type 1 is preferred by the roadmap when compatible) and the exact boundary between saved composition and committed performance state.
- Whether live MIDI profile changes can be safely rebound immediately or must be deferred to a bar/transport boundary.

These remain open until source and user-flow evidence establish the smallest compatible extension.
