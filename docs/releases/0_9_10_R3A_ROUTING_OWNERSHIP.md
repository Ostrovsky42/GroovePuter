# GroovePuter 0.9.10-R3a — MIDI input routing + stable NoteOff ownership

## Purpose

Prove the transport-independent input router and stable NoteOff ownership before connecting the core to the existing TinyUSB reader.

Exact stacked base:

```text
0.9.10-R2 normalized ingress
9a666c2eac634a090de8aec6a9ff98a386aded49
```

R3a is deliberately split from R3b USB adapter wiring so ownership failures can be tested without simultaneously changing the physical MIDI transport.

## Scope

R3a adds:

```text
NormalizedMidiInputMessage
        ↓
MidiInputRouter
        ↓
fixed ActiveNoteOwner[24]
        ↓
existing MusicalEventRouter
        ↓
Synth A / Synth B
```

R3a does **not** yet instantiate `MidiInputRouter` in Cardputer runtime and does not modify TinyUSB.

Not in R3a:

- USB packet adapter/wiring — R3b;
- Drums / GM input mapping — R4;
- disconnect/session lifecycle hardening — R5;
- Sustain / Pitch Bend behavior — R6;
- persistence — R7;
- UI — R8;
- recording — R10;
- implicit MIDI THRU — forbidden.

## Routing configuration

`MidiInputRoutingConfig` is a new input-domain configuration. It is not `OutputOwnership` and not `MidiDeviceProfile`.

R3a fields:

```text
enabled        false by default
channelMode    OMNI | SINGLE
channel        0..15 when SINGLE
target         SYNTH A | SYNTH B
```

Disabled-by-default preserves existing behavior until a later explicit UI/persistence checkpoint enables controller input.

Every effective config change is cleanup-first:

```text
release all retained input owners
        ↓
adopt new config
```

Therefore changing SYNTH A → SYNTH B while a key is held emits NoteOff to A before B becomes the new target. A later physical NoteOff for the old key is an orphan and is ignored; it is never misrouted to B.

## Stable source identity and routed pitch ownership

The physical input owner is keyed by:

```text
transportId
sessionId
input channel
sourceNote
```

and stores:

```text
resolved target
routedNote
```

`sourceNote` is the raw normalized MIDI key used to match the eventual physical NoteOff. `routedNote` is the pitch actually sent to the internal synth.

The current internal Synth A/B live-note contract is C1..B4:

```text
24 .. 71
```

`MiniAcid` and `ClampedLiveNoteIdentity` already use those exact boundaries. R3a duplicates only the two numeric boundaries in the transport-independent input core and source regressions lock all three definitions together. The input core does **not** include a DSP header.

This distinction matters because different physical inputs can resolve to the same sounding pitch:

```text
session 1 raw note 0  ─┐
                       ├─> routed C1 / 24
session 2 raw note 1  ─┘
```

It also happens when two sessions simply play the same in-range note.

R3a uses **newest owner wins per resolved target + routed pitch**:

```text
old owner of Synth A / pitch 60
        ↓
release old routed pitch
retire old source identity
        ↓
commit new source identity as sole owner
        ↓
publish new NoteOn
```

A later NoteOff from the retired source is an orphan and cannot stop the newer owner.

NoteOff never resolves through current routing configuration and never re-runs pitch normalization. It looks up the retained raw source identity and publishes the owner’s retained routed pitch.

The target + routed pitch are committed before synchronous publication through `MusicalEventRouter`, so a route/config callback cannot observe an unowned published NoteOn.

## Bounded capacity

R3a uses:

```text
24 active note owners
```

The existing Cardputer `PerformanceKeyboard` already supports 19 held physical notes. Twenty-four slots preserve that proven density plus small controller headroom without introducing a 16×128 table.

Sustain is outside R3a. R6 must re-measure owner requirements before adding sustain-held notes; the current capacity is not silently assumed sufficient for pedal-heavy piano use.

When all 24 owners are occupied and the new NoteOn does not replace an existing source/resolved-pitch owner:

```text
new NoteOn -> rejected
ownershipCapacityDrops++
no MusicalEvent published
```

Publishing an unowned NoteOn is forbidden because its later NoteOff could not be routed safely.

## Repeated NoteOn policy

For the same `(transport, session, channel, sourceNote)`:

```text
existing owner NoteOff
        ↓
owner slot reused
        ↓
new NoteOn
```

This gives deterministic retrigger semantics with no reference counter and no unbounded duplicate history.

A repeated exact source identity is tracked separately from replacement of a different source that resolves to the same target/pitch.

## Queue overflow recovery

R2 already increments `overflowEpoch` whenever an ingress message cannot be queued.

R3a consumer behavior:

```text
overflowEpoch changed
        ↓
discard pending queue contents
        ↓
release all active MIDI-input owners
        ↓
remember new epoch
```

This is intentionally destructive recovery. Once any queue element has been lost, continuing through a partial lifecycle can strand a missing NoteOff; clearing the whole input epoch is safer and bounded.

## Unsupported messages

R3a routes only:

```text
NoteOn
NoteOff
```

The normalized R2 vocabulary also carries CC, pressure, Program Change and Pitch Bend, but R3a counts and ignores them. They do not mutate active note ownership.

## No implicit MIDI THRU

R3a reuses the existing `MusicalEventRouter` with:

```text
source = MusicalEventSource::MidiInput
```

The existing `UsbMidiOutput` has no `MidiInput` lane. Therefore these events are consumed by the existing internal Synth A/B sink but are not echoed back to USB MIDI output.

A source regression freezes this behavior. MIDI THRU, if ever desired, must become a separate explicit policy rather than an accidental consequence of router fan-out.

## Current PLAY-state limitation

R3a does not alter `MiniAcid::liveNoteOn()`.

Current internal engine behavior still rejects live Synth A/B NoteOn while sequencer PLAY is active. R3a proves route/ownership semantics only; it does not claim controller-over-PLAY behavior.

R3b may connect stopped-transport input first. Any change allowing Pattern + external live ownership during PLAY requires its own engine-level tests and must not bypass `InternalSynthOutput`.

## Memory budget

Compile-time contract:

```text
sizeof(MidiInputRouter) <= 320 bytes
active owner slots = 24
```

The focused host test prints the exact host ABI size.

R3a still does not instantiate the router in firmware, so fixed Cardputer DRAM changes only when R3b introduces the actual queue/router runtime objects. That R3b candidate must pass the ADV fixed-DRAM gate on its exact SHA.

## Hardware list

None for R3a.

## Wiring

None.

## Build / test

From repository root:

```bash
bash tests/run_midi_input_0_9_10_r3a_tests.sh
```

The runner uses C++17 with:

```text
-Wall -Wextra -Werror -pedantic
```

## Build / flash

No firmware flash is required for R3a because routing ownership is not connected to the physical adapter yet.

Normal inherited Core/SDL/ADV/SEQTRAK CI must still remain green on the exact candidate SHA.

## Expected behavior

Focused output must end with:

```text
0.9.10 R3a source/ownership boundaries: PASS
MidiInputRouter size=<bounded value> bytes activeOwners=24 synthRange=24..71
0.9.10 R3a MIDI input routing ownership: PASS
```

Tests cover:

- disabled-by-default behavior;
- OMNI routing;
- single-channel filtering;
- velocity preservation;
- Synth A/B target resolution;
- config-change cleanup before target adoption;
- orphan old NoteOff suppression;
- repeated exact NoteOn Off→On retrigger;
- transport/session separation for distinct pitches;
- newest-owner-wins for the same resolved pitch across sessions;
- low/high source-note clamp aliases with raw NoteOff identity retained;
- 24-owner capacity fail-closed behavior;
- queue-overflow panic/discard recovery;
- unsupported CC ignored without ownership mutation;
- invalid config rejection;
- no implicit USB MIDI THRU lane.

## Troubleshooting

If a route change sends the old NoteOff to the new target, do not patch the sink. Fix the retained input owner/config transition semantics.

If an old session/raw key can stop a newer owner of the same routed pitch, inspect `sourceNote` vs `routedNote` ownership before touching `InternalSynthOutput`.

If the input C1..B4 range diverges from `MiniAcid` or `ClampedLiveNoteIdentity`, update the input contract only after auditing the engine range change; do not weaken the source regression.

If the 25th independent active note publishes despite a full owner table, treat it as a correctness failure; do not increase the capacity merely to hide it.

If a CC starts producing notes or changing ownership in R3a, the stage scope has leaked into R6/control mapping.

If `UsbMidiOutput` starts accepting `MusicalEventSource::MidiInput`, stop and define an explicit THRU product contract before allowing the change.

If the router exceeds 320 bytes, inspect owner layout and diagnostics before changing the budget.

## Acceptance checklist

- [ ] exact base is green R2 `9a666c2eac634a090de8aec6a9ff98a386aded49`;
- [ ] input remains disabled by default;
- [ ] input config is independent from OutputOwnership and DeviceProfile;
- [ ] physical identity is transport + session + channel + source note;
- [ ] resolved target + routed pitch are retained at NoteOn;
- [ ] routed Synth A/B pitch is normalized to the engine’s 24..71 live range;
- [ ] different sources resolving to one target/pitch use newest-owner-wins semantics;
- [ ] retired-source NoteOff cannot stop a newer same-pitch owner;
- [ ] config changes release owners before adopting new routing;
- [ ] repeated exact NoteOn has deterministic Off→On semantics;
- [ ] owner capacity is fixed at 24;
- [ ] full owner table never publishes an unowned NoteOn;
- [ ] overflow epoch causes discard + owner release;
- [ ] CC/pressure/program/pitch messages do not mutate ownership in R3a;
- [ ] no implicit MIDI THRU exists;
- [ ] no TinyUSB/runtime integration is added in R3a;
- [ ] focused R3a tests pass on exact candidate SHA;
- [ ] inherited Core/SDL/ADV/SEQTRAK remain green;
- [ ] no hardware behavior is claimed yet;
- [ ] R3b may wire the existing single TinyUSB RX owner only after R3a is green.
