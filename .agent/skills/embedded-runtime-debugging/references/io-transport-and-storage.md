# I/O Transport And Storage

## Contents

- Layered transport state
- USB host behavior
- Backpressure and recovery
- Composite interfaces and initialization
- Storage and file browsing
- Integration test matrix

## Model Transport State By Layer

Do not expose one `connected` flag for a multi-layer transport. Track at least:

```text
controller started
device configured by host
class/interface mounted
bus suspended
endpoint write accepted
endpoint continuously draining
application consumer active
```

For every write, classify the outcome:

1. interface not mounted;
2. mounted but endpoint/FIFO full;
3. write accepted fully;
4. partial write, if the API permits it.

Some APIs return zero for both unmounted and full. Check mount state separately.

## Understand USB Host Polling

A host can enumerate and bind a USB MIDI/audio/serial interface without polling
its device-to-host endpoint continuously. Linux commonly submits IN requests only
after a user-space application opens the ALSA MIDI endpoint. A firmware FIFO can
therefore accept roughly one buffer of packets and then reject every write while
the device remains mounted.

Test this deliberately:

1. enumerate the device and leave the consumer closed;
2. send until firmware reports endpoint backpressure;
3. open `amidi`, `aseqdump`, a serial reader, or the equivalent consumer;
4. verify that traffic recovers without reboot;
5. close and reopen the consumer to test repeated recovery.

If reopening recovers, the firmware observed normal host backpressure. If only a
device reboot recovers, investigate latched application/driver state.

Use descriptor and kernel evidence on a PC:

```bash
lsusb -t
lsusb -v -d VID:PID
dmesg -w
amidi -l
amidi -p hw:X,0,0 -d
```

Adapt commands to the operating system and interface class.

## Implement Explicit Backpressure Policy

Define behavior for each producer: realtime clock, cleanup, live input, sequencer,
and bulk playback may need different policies.

| Condition | Required behavior |
|---|---|
| Not mounted | Do not write; bound/drop queued output and release logical ownership |
| Mounted but full | Retry only within a bounded deadline or defer in a bounded queue |
| Accepted | Commit ownership/state only after successful physical write |
| Recovery edge | Emit cleanup first, then require explicit resume when replay would surprise the user |

Never busy-spin indefinitely. Do not let retries hold an audio guard, bus mutex,
or high-priority task runnable. Keep cleanup (`NoteOff`, all-notes-off, resource
release) ahead of new material after recovery.

Do not add a token bucket from guessed constants. First measure:

- sustained accepted packets per second;
- endpoint burst capacity;
- receiver-specific channel/event limits;
- latency tolerance of note-on, note-off, clock, and control messages.

Pacing prevents overload only when the receiver is servicing the interface. It
cannot repair bad descriptors, suspend, unmount, or an unbound class driver.

## Preserve Composite USB Contracts

- Register every class before starting USB or finalizing descriptors.
- Verify interface numbers, IADs, endpoint directions, packet sizes, and total
  descriptor length.
- Distinguish the normal composite profile from diagnostic or class-only profiles.
- Preserve a recovery/upload path before removing CDC.
- Verify endpoint/FIFO allocation and internal-memory requirements.
- Test descriptors on a PC before treating a black-box host as the oracle.

Track mount up/down and suspend/resume edges. A class-mounted interface with a
non-draining endpoint is different from bus suspend and different from re-enumeration.

## Model Storage State By Layer

Track storage as:

```text
driver initialized -> card mounted -> path exists -> directory opened
-> entries iterated -> scan complete
```

`SD.cardType()`, `exists()`, and `open()` answer different questions. Directory
open and file wrappers can allocate heap. Under fragmentation, the card can remain
fully mounted while `open()` fails and the UI appears empty.

Report precise user-facing failures:

- `SD unavailable`: card/driver is absent;
- `folder not found`: mount works but path is absent;
- `folder open failed`: path exists but allocation/I/O failed;
- `scan incomplete`: iteration started but did not finish;
- `no supported files`: scan completed successfully with zero matches.

Log free and largest heap blocks on open failure. Never auto-create directories
after every ambiguous `exists()` failure without first confirming the card remains
mounted; an allocation failure can masquerade as a missing path.

## Keep Storage Work Bounded

- Read sector-aligned windows when the medium uses 512-byte sectors.
- Skip `seek()` when the file cursor is already at the requested offset.
- Give each concurrent stream a bounded cache or divide one fixed pool by active
  stream count.
- Keep directory windows fixed-size instead of retaining every filename.
- Close each iterated file entry immediately.
- Keep long scans out of realtime callbacks and critical audio sections.
- Serialize filesystem access if the library or shared bus is not concurrency-safe.

## Test I/O Interactions, Not Only Components

Exercise these combinations:

1. receiver open before playback;
2. receiver opened after endpoint stall;
3. receiver closed/reopened during playback;
4. cable disconnect/reconnect;
5. long dense stream;
6. storage browsing during active streaming;
7. repeated page enter/exit while a large file remains loaded;
8. cold boot and warm reset;
9. black-box target after PC behavior is understood.
