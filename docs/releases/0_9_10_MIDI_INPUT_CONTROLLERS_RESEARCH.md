# GroovePuter 0.9.10 — MIDI Input / Controllers research

## Purpose

Define the production boundary for turning GroovePuter into a MIDI-controlled instrument/sequencer without coupling the core input architecture to USB, SEQTRAK, a future DIN/TRS module, BLE MIDI, or another bridge.

Research baseline:

```text
0.9.7 FINAL integration
9f80cb179f530089bd46f27e03bdde0f7684ba72
```

This checkpoint is research/docs/tests only. It does not change production runtime, persistence, UI, audio, MIDI output, OutputOwnership, DeviceProfile, sampler, transport, or Pattern mutation behavior.

## Research verdict

**GO**, with one architectural correction to the initial sketch: physical transport normalization must be independent from logical input ownership from the first production checkpoint.

Canonical boundary:

```text
physical adapter
  USB device MIDI RX
  future UART/TRS/DIN
  future BLE MIDI
  future bridge
        |
        v
MidiInputTransport
        |
        v
NormalizedMidiInputMessage
        |
        v
bounded input queue
        |
        v
MidiInputRouter + stable note ownership
        |
        +--> Synth A
        +--> Synth B
        +--> logical Drums
        +--> later Pattern capture
        `--> later explicit control mapping
```

The core input owner must not include TinyUSB, Arduino USBMIDI, UART, BLE, SEQTRAK, filesystem, Preferences/NVS, UI, or device-profile headers.

## Release-state audit

### IMPLEMENTED — 0.9.6 Output Ownership

`INTERNAL / MIDI / LAYER` is authoritative for Synth A, Synth B and DRUMS. MIDI Input configuration must never mutate it.

The current `output_ownership.h` deliberately leaves `MusicalEventSource::MidiInput` outside the 0.9.6 source classes and labels MIDI Input as a later roadmap item. 0.9.10 must close that boundary explicitly rather than silently treating input routing as output ownership.

### IMPLEMENTED — 0.9.7 Device Profiles

SEQTRAK / General MIDI / Generic MIDI / Custom describe external output-device mapping and capabilities. They do not select the internal target of incoming MIDI.

A future input drum-map choice may use a GM-style mapping, but that input map must be an input-domain policy, not `MidiDeviceProfile` reuse.

### PARTIAL — 0.9.8 Undo / Safe Editing

0.9.8-R1 is merged after the 0.9.7 baseline and establishes the bounded receipt primitive. R2 is still a separate evolving production checkpoint at the time of this audit.

Live MIDI input does not need Undo and may be developed independently. Sequencer recording/capture must wait for the accepted authoritative 0.9.8 mutation owner and must not create a second Pattern history or recorder-specific Undo stack.

### RESEARCH — 0.9.9

0.9.9 remains a research/design track around musical material lifecycle. Live input is orthogonal. If later MIDI recording participates in live arrangement/activation, it must use the accepted persistent mutation and activation contracts rather than inventing another transaction system.

## Archaeology inventory

| Component | Current state | Owner | Used? | Reusable? | Problem / boundary |
|---|---|---|---:|---:|---|
| TinyUSB MIDI device RX | IMPLEMENTED | `CardputerUsbMidiTransport` / `MidiDispatchTask` | yes | yes, adapter only | RX currently accepts packets but note/controller messages are ignored by the application path |
| TinyUSB callbacks | NOT USED for MIDI RX | Arduino/TinyUSB core | no custom RX callback | yes: keep polling model | no `tud_midi_rx_cb` application owner found; FIFO access is intentionally centralized in dispatcher |
| USB packet reader | IMPLEMENTED | `CardputerUsbMidiTransport::readPacket()` | yes | yes | asserts that `MidiDispatchTask` is sole TinyUSB MIDI FIFO owner |
| MIDI realtime input | IMPLEMENTED | external transport parser/queue + transport runtime | yes | preserve | Clock/Start/Continue/Stop is a separate domain and must not enter controller mapping |
| Channel-note MIDI input | MISSING in current runtime | none | no | planned seam exists | non-realtime USB-MIDI packets currently increment ignored diagnostics |
| `MusicalEventSource::MidiInput` | PARTIAL / ORPHAN SEAM | `MusicalEvent` model | yes as enum, not producer | yes | downstream type already exists; do not create a competing post-route note event |
| `MusicalEventRouter` | IMPLEMENTED | fixed sink fan-out | yes | yes after target resolution | it is output fan-out, not an input target resolver |
| `MusicalEventQueue` | IMPLEMENTED | Pattern scheduled output | yes | no for inbound queue | tied to AudioTask block/frame scheduling; incoming live control needs its own small bounded ingress queue |
| `PerformanceKeyboard` | IMPLEMENTED | Cardputer performance input | yes | concepts only | fixed held-note/panic logic is useful precedent, but physical-key/chord/arp/scale semantics must not become MIDI-controller semantics |
| Internal Synth A/B live path | PARTIAL | `InternalSynthOutput` -> `MiniAcid::liveNoteOn/Off` | yes | yes, with limitation | current engine rejects `liveNoteOn()` while transport is playing; this blocks full live-monitoring goal during PLAY |
| Internal Drums live path | IMPLEMENTED for Performance sources | `InternalSynthOutput` | yes | yes | already triggers logical drum voice plus optional sampler layer; `MidiInput` is explicitly excluded by 0.9.6 boundary |
| Sampler input target | NOT NEEDED | DRUMS internal source/layer | n/a | reuse DRUMS path | do not create a separate sampler MIDI target |
| Output active-note ownership | IMPLEMENTED | `UsbMidiOutput` wire owners | yes | preserve | inbound note ownership is a different logical responsibility; no automatic MIDI THRU in P0 |
| Input note ownership | MISSING | none | no | new bounded owner justified | NoteOff must remember NoteOn's resolved target despite later UI/route changes |
| Pattern note entry | IMPLEMENTED UI path | Pattern Editor / SceneManager | yes | later through mutation owner | currently UI-driven direct mutation on 0.9.7 baseline; MIDI recorder must not call UI page code |
| Pattern recording subsystem | MISSING as canonical MIDI capture owner | none | no | defer | implementation waits for frozen 0.9.8 mutation contract |
| CC output | PARTIAL | USB output/device-specific control | yes | not input mapping | current CC use is primarily output/vendor control, not generic controller input |
| Sustain CC64 input | MISSING | none | no | later | requires input-note ownership semantics; do not bolt onto synth directly |
| Pitch Bend input | MISSING | none | no | conditional later | engine support/ownership must be audited per synth before exposure |
| Program Change / Pressure | MISSING input | none | no | later | outside first production vertical slice |
| Historical note-controller implementation | NOT FOUND | n/a | no | n/a | history shows device-side output and transport RX; no recovered hardware-tested NoteOn controller input implementation was found in this audit |

## Existing implementation to recover/reuse

### 1. Existing device-side USB RX

Cardputer already has a working physical RX path because `MidiDispatchTask` polls `tud_midi_packet_read()` through `CardputerUsbMidiTransport::readPacket()`.

Current flow:

```text
TinyUSB packet RX
    -> MidiDispatchTask
    -> drainIncomingMidiPackets()
    -> parse realtime Clock/Start/Continue/Stop
    -> ExternalMidiTransportEventQueue
```

Non-realtime packets are currently counted as ignored. 0.9.10 must extend this edge with a USB **adapter** that publishes normalized musical/control messages while leaving realtime transport on its existing path.

Do not create a second TinyUSB task or callback owner.

### 2. Existing logical event vocabulary

`MusicalEvent` already supports:

```text
NoteOn / NoteOff / AllNotesOff
source = MidiInput
target = SynthA / SynthB / Drums / Dx
channel / note / velocity
```

After incoming message routing resolves a logical target, P0 should publish the existing event rather than introduce a second synth/drum note protocol.

### 3. Existing DRUMS layer behavior

`InternalSynthOutput` already translates a logical DRUMS lane into the registered local drum voice and, when enabled/assigned, the sampler pad on the same lane.

This is exactly the required sampler boundary:

```text
incoming physical drum note
  -> input drum mapper
  -> logical lane 0..7
  -> existing DRUMS runtime
  -> synth drum + optional sampler layer
```

Only the current source admission needs deliberate 0.9.10 integration; sampler identity/storage is out of scope.

### 4. Existing performance ownership precedent

`PerformanceKeyboard` uses fixed arrays, retains held-note state and panics before target changes. Reuse the **bounded ownership principle**, not the class itself.

External MIDI ownership identity should be transport/session + channel + note and must retain the resolved logical target in the active entry.

## Important ownership conflicts

### Input target is not OutputOwnership

Changing:

```text
MIDI INPUT Target: SYNTH A -> SYNTH B
```

must not change:

```text
Synth A OutputOwnership: INTERNAL/MIDI/LAYER
Synth B OutputOwnership: INTERNAL/MIDI/LAYER
```

### Input target is not DeviceProfile

SEQTRAK/GM/GENERIC/CUSTOM never choose the internal destination of an incoming note.

### P0 does not imply MIDI THRU

Incoming MIDI should not automatically echo to `UsbMidiOutput`. This avoids feedback loops and keeps P0 focused on controlling GroovePuter. Explicit MIDI THRU may be designed later as a separate user-visible policy.

### Input ownership is distinct from output wire ownership

Existing `UsbMidiOutput` ownership protects outbound physical channel+note lifetimes shared by Pattern/PERFORM/SMF. Incoming ownership instead protects:

```text
input transport/session + input channel + input note
        -> resolved logical target/lane
```

It must be bounded and must not duplicate the outbound table.

## Note ownership contract

Minimum active entry concept:

```text
source/session identity
input channel
input note
resolved target
resolved logical drum lane when applicable
key released / sustain-deferred state later
```

Rules:

1. NoteOn resolves target once and records it before publication succeeds.
2. NoteOff looks up the recorded owner; it never asks the current UI target.
3. Route/channel configuration changes release affected existing owners before adopting new configuration, or leave existing owners bound until their NoteOff. Production choice must be explicit and tested.
4. Disconnect releases all notes owned by that transport session.
5. Stop/Scene/project lifecycle hooks must clear owned notes without changing input configuration.
6. `NoteOn velocity=0` normalizes to NoteOff.
7. Repeated NoteOn for an already-owned `(session,channel,note)` requires a deterministic replace/retrigger policy. Do not use an unbounded reference count.
8. Sustain is layered on this owner later; no synth-specific sustain state in the transport adapter.

A full `16 x 128` target table should not be assumed. R2/R3 must measure a small fixed active-note slot count against the real engine/polyphony requirements and ADV DRAM budget.

## Current synth limitation discovered

`MiniAcid::liveNoteOn()` currently returns immediately while the sequencer is playing. `liveNoteOff()` releases only the currently retained live note for that synth.

Therefore P0 can reuse the existing path for STOP-state live playing, but the complete product goal of playing over an active sequence requires a separate, carefully tested internal-voice ownership decision. Do not hide this by routing directly into a synth implementation.

R3 must first determine whether the existing mono synth voice can safely share Pattern + external live ownership during PLAY. If not, 0.9.10 should explicitly scope first hardware acceptance to stopped-pattern live input and introduce PLAY monitoring in a later bounded checkpoint rather than corrupt Pattern ownership.

## Drum input mapping

Input drum mapping is an input-domain policy.

Proposed first map:

```text
GM 36 -> Kick      logical 0
GM 38 -> Snare     logical 1
GM 42 -> ClosedHat logical 2
GM 46 -> OpenHat   logical 3
GM 43 -> MidTom    logical 4
GM 47 -> HighTom   logical 5
GM 37 -> Rim       logical 6
GM 39 -> Clap      logical 7
```

The exact table must be executable-test data. It must not be selected by `MidiDeviceProfile`; an input map setting may be called `GM DRUMS`/`GENERIC` only after UI terminology audit.

## CC / controller inventory

Current tree has no canonical inbound CC router. Existing Control Change support belongs primarily to MIDI output/device controls. No current CC64 sustain or Pitch Bend input path was found.

Keep two domains:

```text
MUSICAL INPUT
  NoteOn / NoteOff / velocity
  later Pitch Bend
  later Sustain ownership

CONTROL MAPPING
  explicit CC -> parameter/action mappings
  Program Change / pressure / transport policies later
```

Start/Stop/Continue/Clock remain in the existing transport domain and never fall through a generic CC/action switch.

## Hardware transport reality

### Current proven firmware topology

GroovePuter currently runs the ESP32-S3 TinyUSB OTG peripheral as a **USB MIDI device**. Its device-side OUT endpoint already receives MIDI packets from a USB host; this is how external MIDI Clock/Start/Continue/Stop reaches the existing transport follower.

That makes device-side RX a valid first physical adapter when the other endpoint is a host (for example a computer or an instrument that acts as USB host).

### Typical USB MIDI controller direct connection

A normal USB MIDI keyboard/controller is usually a USB **device**. Device-to-device does not create a USB host, so direct plug-and-play into the current GroovePuter USB-device mode is not promised by 0.9.10 R1.

ESP32-S3 itself supports USB Host mode, but Cardputer-Adv direct-host VBUS/power/role behavior has not been accepted in this repository. A USB Host adapter is therefore a separate hardware/transport checkpoint, not the core architecture.

### Future UART/TRS/DIN adapter

Cardputer-Adv exposes UART pins on EXT 2.54-14P (`G13 UART_RX`, `G15 UART_TX`) plus power rails. This makes a future electrically-correct MIDI DIN/TRS interface a natural transport adapter without changing routing/ownership/UI core.

The electrical MIDI interface itself (opto-isolation/current-loop/TRS standard/power) is outside R1 and must be specified/tested before hardware claims.

## Recommended production series

The original R1-R9 split is adjusted in one important way: **stable NoteOff ownership moves into the first playable Synth checkpoint**, because a NoteOn/NoteOff implementation without ownership is not safe enough to exist as a production intermediate state.

### R1 — archaeology + ownership contract

Docs/tests only. This checkpoint.

### R2 — transport-independent normalized ingress

Add only core data contracts:

```text
MidiInputTransport identity
NormalizedMidiInputMessage
fixed MidiInputQueue
```

No TinyUSB headers in core. No synth/UI/persistence behavior.

USB device RX becomes one adapter that publishes channel messages; realtime transport remains on its existing parser/queue.

### R3 — routing + stable note ownership + Synth A/B

Add:

- enabled/channel filter (`OMNI` or one channel initially);
- fixed target (`Synth A` / `Synth B`) and audited AUTO semantics if safe;
- bounded active input-note owner;
- NoteOn/NoteOff/velocity including velocity-zero normalization;
- route-change and target-change ownership tests;
- no MIDI THRU.

This checkpoint must also decide the PLAY-state internal synth monitoring limitation before claiming full live performance.

### R4 — logical Drums input

Add pure incoming note -> logical voice map and reuse current local DRUMS + sampler-layer runtime.

No sampler redesign and no output-device-profile dependency.

### R5 — lifecycle hardening

- repeated NoteOn policy;
- disconnect/reconnect session cleanup;
- Stop/project/Scene cleanup hooks;
- input channel/route transition cleanup;
- queue overflow recovery;
- All Notes Off input semantics;
- diagnostics for queue depth/drop/active ownership.

### R6 — conditional Sustain / Pitch Bend

Only if R3-R5 ownership is clean and engine support is explicit.

Sustain comes first because it is an ownership policy. Pitch Bend must be normalized as musical input but applied only through explicit target capability.

### R7 — persisted input configuration

Persist only configuration:

```text
input enabled
channel policy
target
input drum map
```

Never persist active notes, transport session identity, sustain-held state or queue contents.

### R8 — UI/UX

Prefer an existing MIDI/HUB/PROJECT surface after audit. Keep separate labels for:

```text
DEVICE PROFILE     external output semantics
MIDI INPUT         incoming controller routing
```

Minimum product surface should be understandable without project history and show transport availability/status, channel and target clearly.

### R9 — live-input hardware acceptance

Freeze the live controller feature on one exact SHA with host, SDL, ADV/fixed-DRAM, SEQTRAK MIDI-only and supported physical adapter hardware acceptance.

### R10 — sequencer capture / recording

Recording deliberately follows the live-input freeze and the accepted 0.9.8 mutation owner. It reuses the Pattern persistent mutation boundary and one-level Undo; no recorder-specific history.

This extra checkpoint is preferred over hiding recording inside R7/R8 because recording has a substantially larger mutation/regression surface than configuration/UI.

## Parallel work

Safe in parallel:

- R1 archaeology/docs/tests;
- R2 pure normalized-message/queue host model;
- USB-device adapter parsing tests against R2 contract;
- hardware research for USB Host and UART/TRS/DIN adapter;
- CC/Pitch Bend/Sustain engine-capability inventory;
- UI information architecture/mock/help audit without production binding;
- recording archaeology and 0.9.8 handoff design without production recorder code.

Must remain serialized:

- physical adapter -> shared input queue ownership;
- input router + active note owner;
- target/channel changes + cleanup;
- Drums admission into `InternalSynthOutput`;
- any change to `OutputOwnership` handling of `MidiInput`;
- PLAY-state live synth ownership;
- Pattern recording + 0.9.8 Undo/mutation owner;
- persistence schema changes;
- final UI binding;
- any USB Host role change on the ESP32-S3 OTG PHY.

## Memory/realtime budget

R2/R3 must report:

```text
sizeof(NormalizedMidiInputMessage)
sizeof(queue)
queue capacity
sizeof(active ownership entry)
active owner capacity
fixed DRAM delta
max observed queue depth
input drop count
```

No dynamic containers or heap-backed callback dispatch are permitted in input core.

## R1 build/test

```bash
bash tests/run_midi_input_0_9_10_r1_tests.sh
```

R1 requires no hardware.

## R1 acceptance checklist

- [ ] exact base is accepted 0.9.7 integration `9f80cb179f530089bd46f27e03bdde0f7684ba72`;
- [ ] production source unchanged;
- [ ] existing TinyUSB MIDI RX owner identified;
- [ ] existing realtime transport RX preserved as a separate domain;
- [ ] `MusicalEventSource::MidiInput` recorded as reusable partial seam;
- [ ] Synth A/B downstream live path recorded with PLAY-state limitation;
- [ ] current DRUMS local+sampler layer path recorded for reuse;
- [ ] no historical hardware-tested controller-note input is falsely claimed;
- [ ] transport-independent core boundary frozen;
- [ ] OutputOwnership and DeviceProfile orthogonality frozen;
- [ ] no implicit MIDI THRU;
- [ ] recording explicitly waits for accepted 0.9.8 mutation owner;
- [ ] USB Host remains hardware-unproven for Cardputer-Adv until a dedicated adapter/topology test;
- [ ] R2 may start without waiting for 0.9.8 because it changes no persistent musical mutation behavior.
