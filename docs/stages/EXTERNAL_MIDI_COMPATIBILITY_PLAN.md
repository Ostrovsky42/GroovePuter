# External MIDI compatibility workstream

**Canonical roadmap:** [`../../PLAN.md`](../../PLAN.md)  
**Protocol reference:** [`../reference/SEQTRAK_MIDI_PROFILE.md`](../reference/SEQTRAK_MIDI_PROFILE.md)  
**Compatibility matrix:** [`../reference/EXTERNAL_MIDI_COMPATIBILITY.md`](../reference/EXTERNAL_MIDI_COMPATIBILITY.md)  
**Accepted SEQTRAK baseline:** [`../tests/EXTERNAL_MIDI_COMPATIBILITY_ACCEPTANCE.md`](../tests/EXTERNAL_MIDI_COMPATIBILITY_ACCEPTANCE.md)

This document records the priority order inside the external-MIDI compatibility workstream. It does not replace the Wave 1 product order in `PLAN.md`.

## Current baseline

The exact release commit accepted with Yamaha SEQTRAK OS 2.00 is:

```text
b256ff180165e8db37e61be8658b13c0ae2bcd5c
```

That acceptance covers direct USB-C, Note On/Off, native routing, Clock, Start/Stop, external-master follow, and PROJECT SMF synchronization.

It also establishes a target limitation:

```text
SEQTRAK physical Stop -> Play
  sends Stop, then Start
  does not send MIDI Continue (0xFB)
  PROJECT SMF restarts from MUSIC START
```

Later `dev` commits are not automatically covered by that result.

## P0 — required before broad public compatibility claims

### Capability and connection model

- [ ] Make connection topology explicit: USB device, USB host, DIN, TRS, or host bridge.
- [ ] Keep transport capabilities independent: Clock, Start, Stop, Continue, SPP RX, and SPP TX.
- [ ] Keep note-routing capabilities independent from transport capabilities.
- [ ] Never infer direct USB compatibility from class compliance without identifying the host.

### Stable profile separation

Define stable contracts for:

```text
SEQTRAK_NATIVE_11
GM_16
GENERIC_16
```

- [ ] `SEQTRAK_NATIVE_11` keeps Yamaha CH1–11 routing and target-specific CC capabilities.
- [ ] `GM_16` keeps General MIDI channel/note semantics.
- [ ] `GENERIC_16` provides ordinary 16-channel routing without Yamaha or GM assumptions.
- [ ] Migrate the current `Custom` behavior without breaking persisted settings.
- [ ] Add host tests that prevent one profile from silently inheriting another profile's routing.

### Documentation cleanup

- [ ] Remove stale statements from README, MANUAL, and stage documents.
- [ ] Use `MIDI profile`, `channel map`, `Control Change mapping`, and `transport capability`, not `SEQTRAK MIDI API`.
- [ ] Keep public compatibility claims tied to exact hardware acceptance records.
- [ ] Separate `TESTED`, `LIKELY`, `BRIDGE REQUIRED`, and `UNSUPPORTED`.

### Combined hardware acceptance on current `dev`

Run one release-candidate matrix after the in-flight MIDI lifecycle work lands.

For every target and topology, record:

- exact GroovePuter firmware SHA;
- target model and OS/firmware version;
- cable, adapter, bridge, and physical path;
- USB host/device roles;
- selected profile;
- Note On / Note Off;
- channel routing;
- Clock;
- Start;
- Stop;
- Continue;
- SPP/seek;
- Panic and stuck-note behavior;
- reconnect and source-switch behavior.

A public compatibility statement must not say simply "works on dev".

## P1 — most important missing behavior

Several items are already part of the in-flight MIDI lifecycle lane and must be completed there rather than reimplemented.

### Ownership and transport

- [ ] Complete track-aware ownership for exact runtime SMF mute.
- [ ] Keep `sendContinue()` optional and target-capability gated.
- [ ] Gate SPP receive and transmit independently by target capability.
- [ ] Preserve source/track/channel/note ownership through mute, seek, stop, and route change.
- [ ] Maintain zero stuck notes after cleanup operations.

### SEQTRAK remote controls

- [ ] Add explicit SEQTRAK remote mute through CC23.
- [ ] Add explicit SEQTRAK remote solo through CC24.
- [ ] Keep local SMF mute separate from remote target mute.
- [ ] Do not make selecting or muting an SMF track silently alter SEQTRAK state.
- [ ] Report `SENT`, not an unobservable target-state success.
- [ ] Require Cardputer ADV -> SEQTRAK hardware acceptance.

### Runtime and UI hygiene

- [ ] Remove direct routine `Serial` writes from UI transition paths.
- [ ] Move SD enumeration out of frame rendering through caching or asynchronous bounded work.
- [ ] Use partial redraw for MIDI Player and PERFORM where state changes are bounded.
- [ ] Preserve a paused session without automatically starting playback after reboot.

## P2 — compatibility expansion

### Transport backends

- [ ] Add a DIN/TRS MIDI transport backend without creating a second musical scheduler.
- [ ] Reuse the existing event queues, ownership rules, and transport capability model.
- [ ] Add Active Sensing only for profiles and physical backends that require it.
- [ ] Define cable/electrical assumptions explicitly for TRS MIDI type and adapters.

### Hardware profiles

Add hardware profiles only after an exact acceptance record exists:

- [ ] Akai MPC standalone models with USB host ports.
- [ ] Novation Circuit Tracks.
- [ ] Novation Circuit Rhythm.
- [ ] Roland MC-707.
- [ ] Roland AIRA Compact P-6.
- [ ] Roland AIRA Compact S-1.
- [ ] Roland SH-4d.

Each profile must state:

- direct USB, DIN/TRS, or bridge-required topology;
- channel defaults;
- Clock/Start/Stop/Continue/SPP capabilities;
- panic and Active Sensing policy;
- tested target firmware version.

### Public compatibility table

Maintain a single table using only:

```text
TESTED
LIKELY
BRIDGE REQUIRED
UNSUPPORTED
```

Do not use `compatible` without one of these qualifiers.

## Explicit non-goals

- No second MIDI dispatcher.
- No second transport task.
- No protocol-specific logic in audio sample rendering.
- No assumption that a transmitted MIDI message was applied by the target.
- No undocumented Yamaha project/pattern/scene SysEx contract.
- No promotion from `LIKELY` to `TESTED` based on manuals alone.
- No compatibility expansion that displaces Wave 1 interface-trust and musical-direction work.

## Definition of done

The compatibility workstream is release-ready when:

- profiles are explicit and backward-compatible;
- connection roles are visible in docs and settings;
- current `dev` has one combined hardware acceptance record;
- all public claims point to an exact test record;
- local and remote mute are separate operations;
- Continue and SPP are capability-gated;
- no tested lifecycle leaves stuck notes;
- README, MANUAL, protocol reference, and stage docs agree.
