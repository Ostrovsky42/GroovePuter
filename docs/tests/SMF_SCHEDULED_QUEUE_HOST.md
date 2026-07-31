# Scheduled SMF MIDI Queue Host Test

## Purpose

Validate the bounded SPSC queue that will carry polyphonic realtime SMF notes from the player path to the existing `MidiDispatchTask`.

The queue must preserve cleanup capacity and provide generation invalidation for seek/restart/stop without introducing another USB owner.

## Hardware list

No hardware is required for this host test.

Future hardware acceptance uses Cardputer-Adv, a data-capable USB-C cable and Yamaha SEQTRAK.

## Wiring

None for host validation.

Future MIDI playback uses Cardputer USB-C only. PORT.A GPIO2/GPIO1 and `Wire` are not involved.

## Build / Flash

Run:

```bash
bash tests/run_host_tests.sh
```

No flash step is required for this queue-only stage.

## Expected behavior

`test_scheduled_smf_midi_event_queue` verifies:

- fixed 128-entry storage / 127 usable capacity;
- 16 slots reserved from normal NoteOn traffic for critical NoteOff cleanup;
- invalid MIDI channel/data bytes rejected;
- NoteOn overflow is dropped without creating fake ownership;
- NoteOff can consume the reserved slots;
- extreme critical saturation increments generation and publishes a fixed panic mailbox;
- queued events from the old generation are stale after invalidation;
- new events use the new generation;
- frame bounds are validated;
- explicit seek/restart lifecycle can request panic and invalidate stale scheduled events.

## Troubleshooting

- **NoteOn fills all queue slots:** regression; normal traffic must stop at `kCapacity - kCriticalReserve`.
- **NoteOff disappears when full:** regression; critical overflow must request player panic and advance generation.
- **Old event is accepted after seek:** consumer must compare event generation with current queue generation.
- **Direct USB call appears here:** invalid architecture; this queue contains data only and `MidiDispatchTask` remains the sole USB owner.

## Acceptance checklist

- [ ] host test builds with `-Wall -Wextra -Werror`.
- [ ] storage is fixed-size and allocation-free.
- [ ] NoteOn cannot consume the critical reserve.
- [ ] NoteOff uses reserved capacity.
- [ ] critical overflow requests panic.
- [ ] generation advances on panic/seek/restart invalidation.
- [ ] stale queued events are detectable.
- [ ] new-generation events remain usable.
- [ ] no TinyUSB call is introduced.
