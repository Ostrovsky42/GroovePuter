# GroovePuter 0.9.6-E — Output Ownership Transition Lifecycle

## Purpose

Make live `INTERNAL / MIDI / LAYER` transitions safe without introducing a second dispatcher, scheduler, MIDI owner table, audio buffer, filesystem operation or heap allocation.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- Yamaha SEQTRAK for final external-note cleanup acceptance
- USB-C data cable
- microSD only if sampler loops are included in the hardware smoke

## Wiring

No new wiring. Cardputer ADV PORT.A remains GPIO2 SDA / GPIO1 SCL and is not used by this feature. SEQTRAK stays on the existing USB-MIDI path.

## Lifecycle contract

When a transition removes MIDI:

```text
canonical owner changes
  -> per-track MIDI-disable epoch advances
  -> existing Pattern + PERFORM queues observe epoch
  -> existing target-scoped panic mask is raised
  -> existing MidiDispatchTask / UsbMidiOutput releases owned wire notes
```

A NoteOn that entered either bounded queue immediately before the transition is re-checked at consumer pop time and discarded if MIDI is no longer owned. NoteOff and AllNotesOff are never filtered.

When a transition removes the local side:

- PERFORM Synth A/B live identity is released immediately through existing `liveNoteOff()`;
- Pattern Synth A/B receives no new local NoteOn and finishes only its current bounded gate/release tail;
- Drum synth one-shots receive no new local trigger and keep their natural tail;
- all active sampler voices are stopped, but assignments, SampleRefs, preload state and `samplerEnabled` remain unchanged.

The helper `applyModeWithLocalCleanup()` is control-side and must be called under the caller's existing AudioGuard/AudioMutationGate when UI exposes switching in 0.9.6-F.

## Build / Flash steps

Focused gate:

```bash
bash tests/run_output_ownership_tests.sh
```

Full gates:

```bash
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Final live-switch hardware testing is deferred to 0.9.6-G after the UI/persistence stage exposes OutputMode.

## Expected behavior

### Screen

No new UI yet in E.

### Host

The focused gate must prove:

- pre-switch queued NoteOn is discarded after MIDI ownership is removed;
- the existing scoped panic mask is raised exactly for the affected track;
- a second poll does not repeat the same transition panic;
- NoteOff/AllNotesOff cleanup remains deliverable;
- sampler cleanup stops voices without unloading assets.

Expected focused output ends with:

```text
Output ownership contract tests: PASS
Output ownership queue tests: PASS
Output ownership source contract: PASS
0.9.6 output ownership contract gate: PASS
```

## Troubleshooting

### External note reappears after switching to INTERNAL

Check both consumer-side stale NoteOn filters. The panic must be observed before ordinary event draining and an already-queued NoteOn must no longer pass after the ownership change.

### Switching to MIDI leaves a sampler loop audible

The local cleanup path must call `DrumSamplerTrack::stopAll()` only. Do not disable the sampler layer or clear assignments.

### Pattern Synth note has a short release tail after switching to MIDI

Expected. E prevents every new local Pattern NoteOn but does not add a private synth escape just to hard-cut the current bounded gate. PERFORM live notes are released immediately.

### USB disconnect changes OutputMode

Reject the change. Connection state is independent. Existing UsbMidiOutput disconnect cleanup remains authoritative; OutputMode must persist unchanged.

## Acceptance checklist

- [ ] removing MIDI raises existing target-scoped Pattern cleanup;
- [ ] removing MIDI raises existing target-scoped PERFORM cleanup;
- [ ] stale queued NoteOn is discarded after transition;
- [ ] NoteOff/AllNotesOff always pass cleanup;
- [ ] no direct TinyUSB call outside existing dispatcher;
- [ ] no second MIDI queue/scheduler/note-owner table;
- [ ] PERFORM Synth local note releases when local side is removed;
- [ ] no new Pattern local NoteOn after local side is removed;
- [ ] active sampler loops stop when local side is removed;
- [ ] sampler assignments/preload/enabled state survive transition;
- [ ] no per-sample/per-frame ownership read added;
- [ ] no SD/JSON/heap work in transition helper;
- [ ] focused ownership gate PASS;
- [ ] Core/host PASS;
- [ ] SDL PASS;
- [ ] Cardputer ADV compile + fixed DRAM PASS;
- [ ] SEQTRAK MIDI-only compile PASS.
