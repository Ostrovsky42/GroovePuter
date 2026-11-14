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
- On overall transport disconnect, the output snapshots every currently owned channel into its bounded pending-panic mask before abandoning SMF ownership and clearing active state. After reconnect, it sends one channel-level CC123 for each captured channel. This closes the full-transport-loss stuck-note gap for generated and SMF notes in the host fake-transport regression. With DIN enabled, USB unplug alone does not mean overall transport disconnect; tee failover remains active. USB MIDI mounted state checks TinyUSB's MIDI interface state, not only the CDC composite state.
- Runtime pattern/performance routing is still a startup snapshot. Project → MIDI blocks Device Profile and External MIDI output preview/save while `MiniAcid::isPlaying()` and displays `STOP TO CHANGE ROUTE`. Once stopped, edits are saved for next boot; they do not rebind live lanes. A live ownership-aware route transaction has not been implemented. Reboot/device lifecycle behavior remains subject to hardware acceptance.
- USB MIDI FIFO reads/writes assert that the caller is the MIDI dispatch task. The input dispatcher tracks source/session generations; queue recovery and routing configuration changes release owned notes before discarding/re-routing sessions.

## Clock and transport

The code has one `TransportClockRuntime` source selection (internal GroovePuter or external SEQTRAK), an external clock tracker/follower, and bounded scheduled transport queues. Host tests cover follower behavior under multiple tempos, jitter and small source clock skew. That evidence does not verify real USB/DIN timing or clock behavior on a device.

## Existing relevant host tests

- `tests/test_midi_note_ownership_table.cpp`: sparse capacity, invalid address rejection, overlap arithmetic, pruning and full-table release safety.
- `tests/test_usb_midi_output.cpp`, `tests/test_usb_midi_pending_release.cpp`, `tests/test_usb_midi_cutoff.cpp`: output routing, same-note sharing, scoped releases, pending release retries, and controller behavior.
- The `.35` addition in `tests/test_usb_midi_pending_release.cpp` joins `MusicalEventRouter`, `UsbMidiOutput`, and USB-primary/DIN-secondary `TeeMidiTransport`: same-pitch ownership, USB loss while DIN stays mounted, DIN-only final NoteOff, frozen startup route after a new snapshot publication, USB CC123 before the first post-reconnect NoteOn, and zero final ownership. Its focused compile/run passes. This is a deterministic host trace; it does not exercise the Cardputer dispatcher task, real USB enumeration, or physical receiver behavior.
- `tests/test_tee_midi_transport.cpp`: identical traffic on both wires, USB backpressure, DIN-only operation, stall demotion and USB cleanup-debt recovery.
- `tests/test_cardputer_uart_midi_transport.cpp`: NoteOff reserve/priority and deferred release retry under a saturated queue.
- `tests/test_midi_transport_sync.cpp`, `tests/test_external_midi_clock_follower.cpp`: transport event ordering and simulated external clock following.
- The complete local result of `env TMPDIR=build/tmp-host-suite bash tests/run_host_tests.sh` is PASS (exit 0); the canonical log is `build/host-suite-home-tmp-rerun.log`. This is host evidence only.

## 0.9.26 residual audit gaps

1. Live route/profile rebinding remains deferred. Current source freezes the route snapshot after startup; no runtime old-channel NoteOff/new-channel NoteOn transaction exists. If live rebinding is introduced, require a fake-transport ordering test before enabling it.
2. The `.35` focused trace now covers USB-primary loss/rejoin while DIN remains mounted, including shared ownership, DIN-only final release, captured-route stability and USB cleanup before resumed traffic. Still required: exercise this through the Cardputer MIDI dispatcher/queue and cover USB-only note ownership with DIN loss/rejoin; the host mock does not prove physical endpoint state.
3. Exercise queue/table overflow through the integrated dispatcher path, not only component tests, and verify NoteOff remains deliverable after rejected NoteOn bursts.
4. Confirm stop, project change and Song-section transition each reach the scoped ownership cleanup path and preserve unrelated owners on a shared channel.
5. Audit route persistence migrations and old project fixtures before adding fields. The current profile projection itself documents the incomplete Custom Performance target table.
- The profile enum currently lists SeqtrakNative, GeneralMidi, GenericMidi and Custom; Internal is represented through route enable/target state rather than a device profile. Define the roadmap's Internal semantics without conflating output availability with musical role.

Status: overall transport disconnect/reconnect cleanup is covered by a fake-transport regression; runtime route rebinding remains deliberately unavailable and its UI path is locked during playback. Residual endpoint/integrated-dispatcher gaps above remain open; runtime claims are host-only; hardware status **NOT TESTED**.

## 0.9.27 output destination

- The existing persisted `MidiOutputSettings.enabled` bit is the external-output master. OFF is presented as `INTERNAL`; ON is presented as `INT + MIDI`. The internal audio engine remains active in either state.
- Device profile and output enable are independent pending settings. Selecting Generic/GM/SEQTRAK does not turn external output back on, and selecting Internal does not erase the selected profile. The settings codec remains schema v2; no storage migration was added.
- At boot, `MidiDeviceProfileRuntime` publishes role projections from the persisted settings and `UsbMidiOutput` receives the same external-output enable. This gates generated Pattern/Performance notes, SMF note output, MIDI input-thru echo and controller traffic. The dispatcher's direct Clock/Start/Continue/Stop path is suppressed while external output is off.
- MIDI input configuration remains independent, so incoming clock/control routing is not conflated with outgoing destination selection. Profile and destination changes take effect after reboot; active USB output lanes are not rebound live.
- Host UI evidence covers preview, save feedback, profile independence and visible layout. The SDL platform stub does not exercise Cardputer Preferences persistence or physical transport output; those remain software-source/codec evidence and hardware acceptance items, respectively. Hardware status **NOT TESTED**.
