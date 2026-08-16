# GroovePuter 0.9.6-C — Synth A/B Output Ownership

## Purpose

Connect the canonical 0.9.6 `INTERNAL / MIDI / LAYER` owner to Synth A/B without changing synth engines, MIDI routes, receiver profiles, Song/Phrase structure, or audio buffer geometry.

This checkpoint intentionally stops before live transition cleanup, Scene persistence and UI. Those remain 0.9.6-E/F.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- Yamaha SEQTRAK for external MIDI smoke (optional for C software gate)
- USB-C data cable

## Wiring

No new wiring is introduced.

Cardputer ADV PORT.A remains unchanged and is not used by this feature:

```text
SDA GPIO2
SCL GPIO1
```

SEQTRAK remains connected through the existing USB-MIDI path. No USB Host redesign is part of this checkpoint.

## Contract

For Synth A and Synth B independently:

```text
INTERNAL  local NoteOn yes   MIDI NoteOn no
MIDI      local NoteOn no    MIDI NoteOn yes
LAYER     local NoteOn yes   MIDI NoteOn yes
```

Legacy Scenes with no explicit OutputMode retain <=0.9.5 behavior:

```text
Pattern/Song  local + MIDI
PERFORM       MIDI only
```

Cleanup rule:

```text
NoteOff / AllNotesOff are NEVER rejected by current OutputMode.
```

This is required because the output mode can change after the matching NoteOn.

### Realtime implementation

- Pattern MIDI NoteOn is rejected at `MusicalEventQueue` before it consumes the bounded scheduled queue.
- PERFORM MIDI NoteOn is rejected at `MidiControlEventQueue` before it consumes the bounded control queue.
- Pattern local NoteOn is gated by the existing Synth A/B slot wrapper.
- PERFORM local NoteOn is gated by `InternalSynthOutput`.
- `process()` and `release()` do not read OutputMode per audio sample.
- No heap allocation, mutex, filesystem access, JSON work or USB call is added to the audio callback.
- TinyUSB remains owned only by the existing MIDI dispatcher.

A mode transition that removes the internal side may leave the already-started envelope to its normal release path at this C checkpoint. 0.9.6-E owns explicit transition cleanup before UI exposes live switching.

## Build / Flash steps

Focused contract:

```bash
bash tests/run_output_ownership_tests.sh
```

Full software gates:

```bash
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Optional C smoke flash:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

There is no output-mode key binding yet, so hardware mode changes are not part of C acceptance.

## Expected behavior

### Screen

No new UI appears in C.

### Runtime

Old projects sound exactly as before because raw output state `0` preserves legacy source-dependent behavior.

Focused host output includes:

```text
Output ownership contract tests: PASS
Output ownership queue tests: PASS
0.9.6 output ownership contract gate: PASS
```

The queue regression proves that ownership suppression does not increment overflow/drop counters and that NoteOff/AllNotesOff remain deliverable after MIDI is disabled.

## Troubleshooting

### Old PERFORM suddenly plays locally

Reject the checkpoint. Legacy raw state must keep PERFORM external-only until an explicit OutputMode is restored/selected.

### INTERNAL still emits MIDI NoteOn

Check both producer boundaries. Pattern uses `MusicalEventQueue`; PERFORM uses `MidiControlEventQueue`. Do not add a second dispatcher or filter TinyUSB directly from the audio task.

### Stuck note after a policy change in host tests

Verify that only NoteOn is filtered. NoteOff and AllNotesOff must bypass the ownership gate. Full active-owner cleanup on the exact mode-switch edge is handled in 0.9.6-E.

### POLY sounds monophonic internally

Expected for 0.9.6. The existing local Synth A/B engines remain monophonic. Output ownership does not introduce a new polyphonic synth engine; external receiver MONO/POLY remains a separate concern.

## Acceptance checklist

- [ ] exact parent is the accepted 0.9.6-B head;
- [ ] Synth A INTERNAL blocks external NoteOn and keeps local NoteOn;
- [ ] Synth A MIDI blocks local NoteOn and keeps external NoteOn;
- [ ] Synth A LAYER allows both;
- [ ] same truth table independently for Synth B;
- [ ] legacy Pattern remains local + MIDI;
- [ ] legacy PERFORM remains MIDI-only;
- [ ] NoteOff/AllNotesOff remain cleanup-critical in every mode;
- [ ] suppressed NoteOn does not consume a bounded MIDI queue slot;
- [ ] suppressed NoteOn is not counted as queue overflow/drop;
- [ ] DX behavior unchanged;
- [ ] MIDI-input architecture unchanged;
- [ ] no Scene persistence change yet;
- [ ] no UI change yet;
- [ ] no synth TYPE/parameter mutation;
- [ ] no second MIDI dispatcher/scheduler/note-owner table;
- [ ] focused output ownership gate PASS;
- [ ] full host/Core PASS;
- [ ] SDL PASS;
- [ ] Cardputer ADV normal compile PASS;
- [ ] fixed DRAM gate PASS;
- [ ] SEQTRAK MIDI-only compile PASS.
