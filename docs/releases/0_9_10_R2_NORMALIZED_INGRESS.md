# GroovePuter 0.9.10-R2 — Transport-independent normalized MIDI ingress

## Purpose

Add the smallest production core needed for future MIDI controller input without binding routing, note ownership, recording or UI to USB.

Stacked development baseline:

```text
0.9.10-R1 research
research/0.9.10-midi-input-controllers
a550148849cafe3e5a4f1af0542538c30223fe72
```

R1 itself is one docs/tests commit above accepted 0.9.7 integration `9f80cb179f530089bd46f27e03bdde0f7684ba72`.

R2 intentionally does **not** depend on 0.9.8 Undo for live input. Future sequencer capture must delta-audit/rebase onto the accepted 0.9.8 mutation owner before recording is implemented.

## Scope

R2 adds only:

```text
MidiInputTransportId / MidiInputSessionId
        ↓
NormalizedMidiInputMessage
        ↓
MidiInputQueue
```

No physical adapter is connected yet.

No changes to:

```text
TinyUSB / USB MIDI runtime
UART / BLE
MidiDispatchTask
Clock / Start / Continue / Stop
MusicalEventRouter
Synth A / Synth B / Drums
OutputOwnership
DeviceProfile
Scene / persistence
UI
Pattern recording
Undo
```

## Core contract

### Transport identity

`MidiInputTransportId` is an opaque non-zero `uint8_t`.

It does **not** encode `USB`, `UART`, `BLE`, `SEQTRAK` or another physical kind. A future adapter owns an ID.

`MidiInputSessionId` is an opaque non-zero `uint16_t`. Reconnect/replacement must use a new session so R3 NoteOff ownership cannot confuse notes from an old physical session with a new one.

### Normalized message

`NormalizedMidiInputMessage` is a compact 12-byte trivially-copyable value carrying:

```text
timestampMicros
sessionId
transportId
type
channel 0..15
data1
data2
```

Supported R2 message classes are MIDI 1.0 channel voice only:

```text
NoteOff
NoteOn
PolyPressure
ControlChange
ProgramChange
ChannelPressure
PitchBend
```

`NoteOn velocity=0` is canonicalized to `NoteOff` during normalization.

System Common, SysEx and System Realtime are rejected by this channel-voice normalizer. Existing GroovePuter MIDI Clock/Start/Continue/Stop ownership remains untouched.

`timestampMicros` is an adapter-supplied monotonic 32-bit microsecond timestamp. R2 does not interpret it. Carrying the arrival time now prevents later sequencer recording from having to infer event time from delayed consumer dispatch.

### Bounded queue

`MidiInputQueue` is a fixed SPSC ring:

```text
storage slots: 64
usable capacity: 63
heap allocation: none
blocking: none
```

The queue uses the existing `MidiRealtimeWord` acquire/release primitive. It is deliberately **single producer / single consumer**.

R2 does not promise simultaneous direct publication from USB + UART + BLE callbacks. Future transports must be selected/serialized through one producer owner, or a later explicit fan-in design must be justified. The core message format itself remains transport-independent either way.

### Overflow safety foundation

Queue-full behavior is fail-closed:

```text
tryPush() -> false
droppedOverflow++
overflowEpoch++
```

The queue also exposes:

```text
highWaterMark
rejectedInvalidCount
discardPendingFromConsumer()
```

R3 must observe overflow epoch changes before claiming live NoteOff safety. On an overflow boundary the future consumer can discard pending ingress and release all active input-note owners, avoiding a permanently stuck note if a NoteOff was lost.

## Memory budget

Compile-time contracts:

```text
sizeof(NormalizedMidiInputMessage) == 12 bytes
sizeof(MidiInputQueue) <= 800 bytes
```

R2 adds the queue **type**, but does not instantiate it in production runtime. Therefore R2 itself adds no persistent queue object to global/internal DRAM.

The focused host test prints the exact host ABI `sizeof(MidiInputQueue)`. Actual ADV fixed-DRAM delta is measured only when a production owner is instantiated in a later checkpoint.

## Hardware list

None for R2. This checkpoint is host/source-contract only.

## Wiring

None.

No direct USB-controller, UART/TRS/DIN or BLE hardware behavior is claimed by R2.

## Build / test

From repository root:

```bash
bash tests/run_midi_input_0_9_10_r2_tests.sh
```

The runner compiles with:

```text
C++17
-Wall -Wextra -Werror -pedantic
```

and runs both source-boundary and executable queue/normalization tests.

## Build / flash

No firmware flash is required for R2 because the new queue/message types are not connected to runtime.

Normal project CI must still remain green on the exact PR candidate SHA before the checkpoint is considered clean.

## Expected behavior

Focused output must end with:

```text
0.9.10 R2 source/ownership boundaries: PASS
NormalizedMidiInputMessage size=12 bytes
MidiInputQueue size=<bounded value> bytes capacity=63
0.9.10 R2 normalized MIDI ingress: PASS
```

The executable tests verify:

- channel extraction;
- timestamp/source/session retention;
- velocity-zero NoteOn -> NoteOff canonicalization;
- Pitch Bend 14-bit reconstruction;
- Program Change one-data-byte normalization;
- rejection of realtime/system status through this path;
- invalid source/session rejection;
- FIFO ordering;
- exact bounded capacity;
- wrap-around;
- overflow counter + epoch;
- high-water diagnostics;
- invalid-message counter;
- consumer discard recovery primitive.

The source regression verifies that existing production runtime still contains no `MidiInputQueue` / `NormalizedMidiInputMessage` integration.

## Troubleshooting

If the source regression reports an existing runtime reference, do not weaken the test merely to make R2 green. Either the integration belongs in R3, or the R2 scope has accidentally expanded.

If `sizeof(NormalizedMidiInputMessage)` changes from 12 bytes, inspect field layout before accepting the increase.

If `MidiInputQueue` exceeds the 800-byte budget, do not increase the limit without a measured reason.

If realtime `0xF8/0xFA/0xFB/0xFC` messages begin passing this normalizer, restore the transport-domain separation rather than routing clock messages through controller input.

## Acceptance checklist

- [ ] branch is stacked exactly on R1 candidate `a550148849cafe3e5a4f1af0542538c30223fe72`;
- [ ] normalized core contains no TinyUSB/Arduino/UART/BLE/UI/persistence dependency;
- [ ] transport identity is opaque, not a USB enum;
- [ ] session identity is explicit;
- [ ] arrival timestamp is retained;
- [ ] only MIDI 1.0 channel-voice messages normalize here;
- [ ] zero-velocity NoteOn canonicalizes to NoteOff;
- [ ] message remains 12 bytes;
- [ ] queue remains fixed SPSC, capacity 63, no heap/no blocking;
- [ ] queue overflow increments a recovery epoch;
- [ ] no production runtime integration exists yet;
- [ ] focused R2 tests pass on exact candidate SHA;
- [ ] inherited Core/regression CI is green on exact candidate SHA;
- [ ] no hardware acceptance is claimed for R2;
- [ ] R3 is the first checkpoint allowed to add input routing + stable NoteOff ownership + Synth A/B behavior.
