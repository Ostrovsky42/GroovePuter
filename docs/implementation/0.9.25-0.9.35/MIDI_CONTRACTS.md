# MIDI contracts at the 0.9.24 baseline

Base commit: `28918d9b78e9a527ce2b5ce201a465af5126f76a`

This records current source behavior for the 0.9.25 audit. Source review is not a claim of Cardputer or SEQTRAK hardware acceptance.

## Owners and route state

- `MidiDeviceProfileRuntime` owns the applied `MidiOutputSettings` snapshot and profile identity. `projectMidiOutputRoutes()` and `projectMidiPerformanceRoutes()` derive bounded route values; they do not own settings.
- `MidiPatternStartupRouteRuntime` publishes one combined pattern/performance snapshot. `UsbMidiOutput::begin()` consumes it while configuring lanes. The snapshot contract explicitly says later publication does not mutate started lanes. A safe live route-change transaction is not exposed by this boundary yet; route changes while notes are held need an explicit release/rebind contract before the roadmap can claim runtime route switching.
- The profile projections are intentionally explicit. SEQTRAK, GM and Generic MIDI have defined defaults; Custom has no complete Performance target table and keeps `complete == false` instead of guessing channels or drum assignments. Device profile and physical channels remain separate from stored musical material.
- `cardputer_usb_midi_transport.cpp` owns one `UsbMidiOutput` above one `TeeMidiTransport` (USB primary, UART/DIN secondary). It shares musical note ownership and retry policy rather than creating separate output engines. The MIDI dispatch task enables DIN at startup; the tee can therefore report mounted with no USB host when DIN is available.

## Note lifetime and overload

- `MidiEndpointOwnershipTable` stores fixed-capacity live `(channel, note)` cells. The current capacity is 128 cells per `UsbMidiOutput`, under a compile-time size ceiling of 640 bytes. A cell separates aggregate wire owners from SMF owners; lane-local state retains source/target identity for generated material.
- NoteOn reserves a cell before transport send. If the table is full, the NoteOn is refused and never reaches the wire. Release paths use `peek()` and cannot consume capacity or lose a cell while cleaning up.
- When SMF overlaps a note already owned by Pattern/Performance, it joins the existing wire note without duplicating NoteOn. Scoped SMF release subtracts SMF owners and leaves other wire owners sounding. Repeated SMF-only NoteOn retriggers with NoteOff first.
- Failed NoteOff writes remain represented as pending releases. The dispatcher retries on a bounded 5–20 ms backoff. UART transport separately reserves queue capacity for release traffic, records deferred NoteOffs and retries them before ordinary traffic.
- Stop/panic paths release notes by source/target and can request scoped channel cleanup. `All Notes Off` is a recovery tool, not the normal per-note ownership mechanism. `TeeMidiTransport` tracks USB cleanup debt when DIN accepted cleanup while USB was stalled; before USB regains authority it sends channel CC123 cleanup.
- On overall transport disconnect, SMF ownership is abandoned with pending channel cleanup retained, then active in-memory state is reset. With DIN enabled, USB unplug alone does not mean overall transport disconnect; tee failover remains active. USB MIDI mounted state checks TinyUSB's MIDI interface state, not only the CDC composite state.
- USB MIDI FIFO reads/writes assert that the caller is the MIDI dispatch task. The input dispatcher tracks source/session generations; queue recovery and routing configuration changes release owned notes before discarding/re-routing sessions.

## Clock and transport

The code has one `TransportClockRuntime` source selection (internal GroovePuter or external SEQTRAK), an external clock tracker/follower, and bounded scheduled transport queues. Host tests cover follower behavior under multiple tempos, jitter and small source clock skew. That evidence does not verify real USB/DIN timing or clock behavior on a device.

## Existing relevant host tests

- `tests/test_midi_note_ownership_table.cpp`: sparse capacity, invalid address rejection, overlap arithmetic, pruning and full-table release safety.
- `tests/test_usb_midi_output.cpp`, `tests/test_usb_midi_pending_release.cpp`, `tests/test_usb_midi_cutoff.cpp`: output routing, same-note sharing, scoped releases, pending release retries, and controller behavior.
- `tests/test_tee_midi_transport.cpp`: identical traffic on both wires, USB backpressure, DIN-only operation, stall demotion and USB cleanup-debt recovery.
- `tests/test_cardputer_uart_midi_transport.cpp`: NoteOff reserve/priority and deferred release retry under a saturated queue.
- `tests/test_midi_transport_sync.cpp`, `tests/test_external_midi_clock_follower.cpp`: transport event ordering and simulated external clock following.
- The complete local result of `env TMPDIR=build/tmp-host-suite bash tests/run_host_tests.sh` is PASS (exit 0); the canonical log is `build/host-suite-home-tmp-rerun.log`. This is host evidence only.

## 0.9.26 audit gaps

1. Prove output route/profile changes while held notes are active either release the old physical route before applying the new snapshot or are rejected until a safe boundary. Add a fake-transport test for exact old-channel NoteOff/new-channel NoteOn ordering.
2. Test endpoint-specific loss when both tee wires are enabled, including USB-only note ownership, DIN-only cleanup and rejoin behavior. Overall mounted/unmounted transitions alone are insufficient to prove each physical receiver's note state.
3. Exercise queue/table overflow through the integrated dispatcher path, not only component tests, and verify NoteOff remains deliverable after rejected NoteOn bursts.
4. Confirm stop, project change and Song-section transition each reach the scoped ownership cleanup path and preserve unrelated owners on a shared channel.
5. Audit route persistence migrations and old project fixtures before adding fields. The current profile projection itself documents the incomplete Custom Performance target table.
- The profile enum currently lists SeqtrakNative, GeneralMidi, GenericMidi and Custom; Internal is represented through route enable/target state rather than a device profile. Define the roadmap's Internal semantics without conflating output availability with musical role.

Status: source contracts documented and the 0.9.24 host suite passes locally. The 0.9.26 audit gaps above remain open; runtime claims are host-only; hardware status **NOT TESTED**.
