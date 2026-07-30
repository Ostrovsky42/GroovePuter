# GroovePuter Recovery Plan

Status: revised after deep source audit of `main` commit `bfbc63049cf79646217aa19c170b730e50ab3e4f`.

Detailed evidence: [`DEEP_AUDIT.md`](DEEP_AUDIT.md)  
Execution backlog: [`BUG_BACKLOG.md`](BUG_BACKLOG.md)  
Longer-term architecture backlog: [`BACKLOG.md`](BACKLOG.md)

## Objective

Recover GroovePuter as a reliable Cardputer ADV instrument before adding Yamaha SEQTRAK output. Preserve the existing engines, generator, song model and UI where possible, but stop treating current timing, persistence and cross-core state ownership as trustworthy.

## Revised diagnosis

The first audit correctly identified the undefined board/build profile and absent synchronization, but it substantially underestimated functional defects.

The deeper audit found four release-blocking clusters:

1. **The 96 PPQN clock does not dispatch events at 96 PPQN.** The counter advances every tick, but event evaluation runs only on 24-tick boundaries. Non-zero swing and microtiming offsets can therefore make events disappear.
2. **The audio task does not own mutable DSP state.** UI, keyboard, paging, storage, automation and engine-switch code can modify or replace objects while core 1 renders them.
3. **Persistence is not lossless or transactional.** Scene save omits musical fields, page paging writes raw non-versioned structs, and failures can be presented as success or can partially replace active data.
4. **The Cardputer ADV profile is internally contradictory.** Build scripts select ordinary Cardputer while firmware hardcodes ADV codec/I2S behavior, and GPIO21 is used both as amplifier enable and NeoPixel data.

The repository remains recoverable. A rewrite is not justified. The correct strategy is to fix the ownership and data contracts in small vertical slices with deterministic tests.

## Corrections to the initial report

### Performance sequence counter

`perfStats.seq` is incremented before and after a telemetry update intentionally as an odd/even seqlock-style publication protocol. The double increment is not itself a callback-count bug.

The actual defect is:

- `seq` and payload fields are `volatile`, not atomic;
- no acquire/release ordering protects payload publication;
- readers and writers run on different cores;
- I2S write failures do not increment the underrun/output-failure count consumed by adaptive FX safety.

Recovery action: preserve odd/even publication if useful, but implement it with atomics and a documented snapshot contract.

### Scene load failure

The active streaming parser uses a separate static temporary `Scene` and copies it into the active scene only after successful parsing. A syntax error does not partially mutate the active scene.

The remaining defect is different: the static temporary scene is reused and the reset helper does not initialize every optional field, so sparse/legacy scenes can inherit stale values from a previous successful load.

### WAV block size

The active global block size is 512 frames and the offline renderer also uses 512-frame blocks. There is no confirmed fixed-buffer overflow from that call.

The renderer remains unsafe because it can render the same mutable engine concurrently with the hardware audio task and it calculates duration without `patternBars`.

## Immediate P0 findings

### P0 — GPIO21 ownership conflict

`miniacid.ino` treats GPIO21 as amplifier enable. `LedManager` sends WS2812 data on GPIO21.

Until the actual ADV pin map is verified, LED writes must be disabled. Hardware pins must be declared once in a Cardputer ADV profile.

### P0 — Broken 96 PPQN dispatch

Current flow:

```text
sample loop
  -> advance currentTick_ at 96 PPQN
  -> only when tick % 24 == 0
       -> advanceTick()
       -> processSequencerEvents(currentTick_)
```

Event matching uses exact tick equality after swing and per-step timing offsets. Since matching runs only at tick 0, 24, 48, ..., offsets 1–23 are usually never observed.

Required fix:

```text
every PPQN tick
  -> dispatch all events due at this tick

only every 24 PPQN ticks
  -> advance 16-step playhead
  -> apply step-boundary automation/song logic
```

This must be characterized with tests before MIDI event extraction.

### P0 — Cross-core mutable engine ownership

The FreeRTOS audio task renders on core 1. UI/core 0 directly changes transport, BPM, mutes, patterns, scenes, paging, tape state and engine objects. `AudioGuard` is empty.

Particularly dangerous paths:

- synth engine switching allocates/replaces/destroys `unique_ptr` voices;
- drum automation can allocate a new drum engine from the audio path;
- page navigation allocates and deletes UI object graphs while audio uses the same heap;
- scene/page replacement mutates structures read during rendering.

Required model:

- audio runtime is the sole writer of active DSP state;
- scalar control changes arrive through a bounded allocation-free command queue;
- patterns/scenes are prepared and validated outside audio, then published as immutable/versioned snapshots;
- audio swaps snapshots at a documented block, step or bar boundary;
- no SD, JSON, allocation or blocking operation occurs in the audio task.

### P0 — Scene data loss

The active streaming writer omits at least:

- drum step velocity and timing;
- synth ghost, velocity and timing;
- global swing percentage and swing mask;
- generator parameters;
- active song slot.

Save → Load therefore changes the composition even when file I/O succeeds.

Required fix: one versioned schema and an exhaustive field-by-field round-trip test through the actual Cardputer writer/parser path.

### P0 — Unsafe scene commit

Current save behavior can remove the previous file before the replacement is validated, update current-scene metadata separately, and report success after lower-level failure. Scene names also need strict path sanitization.

Required fix: temp write → close → full validation/checksum → rename/promote → metadata update. Failed writes must leave the previous scene and active name unchanged.

### P0 — Destructive pattern paging

Current paging:

- dumps raw C++ structs with no magic/version/CRC;
- stores drums/synth A/synth B in separate files;
- can zero individual components on failure;
- uses `memset(0)` for synth patterns even though an empty note is `-1`;
- changes page index even after failed save/load.

Required fix: versioned ABI-independent page payload, checksum, one generation/transaction, temporary load, validation, and active snapshot swap only after full success.

### P0 — Sampler ownership defects

`RamSampleStore::preload()` can loop forever when every slot is referenced and no eviction makes progress. Eviction can also observe `refCount == 0`, then race a new audio acquisition and free PCM that the audio task is using.

Required fix: bounded no-candidate failure and an atomic slot state/claim protocol.

### P0 — Destructive MIDI overwrite

MIDI overwrite mode can clear all following patterns before the source footprint is known. Parsing also mutates/persists partial results after errors.

Required fix: parse and plan into isolated temporary state; determine exact destination footprint; commit only after complete success through the recovered page/snapshot service.

## Recovery architecture

### Hardware profile

Create one compact `CardputerAdvHardwareProfile` or equivalent source of truth containing:

- amplifier enable;
- ES8311 I2C address and bus;
- I2S BCLK/LRCLK/DOUT;
- display and keyboard ownership;
- SD/SPI ownership;
- RGB LED pin or explicit `unsupported` state;
- USB role/capability;
- PSRAM policy.

Do not support original Cardputer implicitly. Add a second profile only after separate hardware validation.

### Control ownership

```text
Keyboard / UI / Encoder / future MIDI input
                  |
                  v
        bounded EngineCommand queue
                  |
                  v
       audio-block command application
                  |
                  v
          active DSP state owner
```

Commands must be fixed-size, validated and pointer-free. Queue-full behavior must be explicit: reject, coalesce or overwrite only where semantics permit it.

### Pattern and scene ownership

```text
SD / JSON / Atlas / MIDI import
              |
              v
     prepare inactive snapshot
              |
       validate + version
              |
              v
   publish at safe musical boundary
```

Large snapshots must not be copied through the scalar command queue.

### Timing boundary

The 96 PPQN clock remains authoritative. Event scheduling must produce an output-neutral stream:

```text
ScheduledEvent {
  tick/time,
  track,
  type,
  note/control,
  velocity/value,
  duration or explicit off,
  stable ordering
}
```

Internal audio and MIDI consume the same event stream. MIDI must not implement a second sequencer clock.

## Ordered recovery phases

### Phase 0 — Prevent immediate hardware/data damage

- disable the GPIO21 LED path;
- propagate save failures;
- make scene commit transactional;
- sanitize scene names;
- capture a real Cardputer ADV baseline.

Exit gate: LED activity cannot touch amplifier enable, and failed save cannot destroy or falsely confirm a scene.

### Phase 1 — Repair and characterize timing

- add failing tests for tick offsets 1–23;
- dispatch events on every PPQN tick;
- separate PPQN dispatch from 16-step advancement;
- make FEEL changes boundary-safe;
- restore a representable song pause state;
- fix offline render ownership/duration.

Exit gate: swing and microtiming never lose events and deterministic tick/event tests pass.

### Phase 2 — Establish real-time ownership

- inventory mutable access;
- implement bounded commands;
- migrate transport/BPM/mutes/parameters;
- publish patterns/scenes as immutable snapshots;
- remove audio-task allocation from engine switching;
- replace no-op guard;
- publish race-safe telemetry, waveform and LED events;
- remove pre-setup SD access.

Exit gate: 30-minute rapid-control stress test with no reset, corruption, allocation in steady-state audio or unclassified direct mutation.

### Phase 3 — Complete persistence

- define one scene schema;
- add exhaustive Cardputer-path round-trip tests;
- reset all defaults before sparse loads;
- implement complete JSON escaping and bounds;
- make clear operations reset complete pattern state;
- remove divergent serializer paths.

Exit gate: every supported field round-trips exactly and malformed/sparse fixtures behave deterministically.

### Phase 4 — Replace page cache

- version/checksum page data;
- commit all tracks as one generation;
- load into temporary state;
- unify 16-page bounds/navigation;
- repair global free-pattern search;
- centralize page transition policy.

Exit gate: fault injection cannot mutate active state or create mixed-generation pages.

### Phase 5 — Repair sampler and MIDI import

- fix bounded eviction and slot ownership;
- fix reverse playback and RIFF parsing/accounting;
- make MIDI parse/plan/commit transactional;
- restrict overwrite to exact footprint;
- normalize Type-1 files on one global timeline;
- fix Channel Pressure/VLQ parsing and metrics.

Exit gate: sanitizer/host tests and rollback integration tests pass.

### Phase 6 — Reproducible platform and device acceptance

- pin one Cardputer ADV toolchain/FQBN;
- normalize build/upload;
- repair SDL host source list;
- add compile/unit/sanitizer CI;
- print build fingerprint;
- run full Cardputer ADV acceptance suite.

Exit gate: clean checkout builds through one command, CI uses the same configuration, and hardware passes 30-minute playback/control stress.

### Phase 7 — Output-neutral PatternPlayer and SEQTRAK

Only after earlier exit gates:

- extract scheduled-event output from recovered timing;
- adapt existing internal audio to `InternalAudioOutput`;
- implement fake output and golden event tests;
- verify Cardputer ADV USB role and SEQTRAK topology;
- add minimal `MidiOutput`;
- add `SeqtrakProfile` channel/drum/clock/panic mapping;
- load compact, validated Atlas runtime patterns.

## Non-goals

- no framework rewrite;
- no speculative SEQTRAK SysEx;
- no recorder reactivation before real-time stability;
- no on-device parsing of large Atlas research CSVs;
- no general mutex held through audio rendering;
- no original Cardputer support claim without a second validated hardware profile.

## Fast acceptance checklist

- [ ] Confirm physical device is Cardputer ADV.
- [ ] Record exact FQBN, board package, libraries and firmware commit.
- [ ] Verify GPIO21 has one owner.
- [ ] Verify 1–23 tick offsets trigger exactly once.
- [ ] Verify Save → Load preserves every musical field.
- [ ] Inject SD/page failures and confirm active data remains unchanged.
- [ ] Stress engine switching with zero audio-task allocation.
- [ ] Fill sampler pool with held references and confirm bounded failure.
- [ ] Run 1,000 page transitions without heap collapse.
- [ ] Run 30 minutes of playback and rapid controls without reset, stuck note, data loss or sustained deadline miss.
- [ ] Keep recorder explicitly disabled until separately accepted.

## Immediate implementation decision

The first coding task is **HW-001** from `BUG_BACKLOG.md`: remove the GPIO21 collision and establish a single Cardputer ADV pin profile.

The first behavior test is **TIME-001**: demonstrate that non-zero PPQN offsets are currently lost.

The first architectural slice is **RT-002/RT-003**: bounded commands for transport, BPM and mutes.

USB-MIDI is not the next change.
