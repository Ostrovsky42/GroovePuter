# GroovePuter 0.9.6-D — Drum / Sampler Output Ownership

## Purpose

Extend the canonical `INTERNAL / MIDI / LAYER` output owner from Synth A/B to the logical Drums track while preserving the recovered sampler as an internal source layer.

The logical Drums output contract is:

```text
INTERNAL  drum synth + enabled sample layer, no external MIDI NoteOn
MIDI      no new local drum/sample trigger, external MIDI NoteOn only
LAYER     drum synth + enabled sample layer + external MIDI NoteOn
```

`samplerEnabled` remains independent. Output ownership never loads, unloads, scans or reassigns a sample.

This checkpoint still does not expose live UI switching or persist OutputMode. Transition cleanup and Scene/UI ownership remain 0.9.6-E/F.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- Yamaha SEQTRAK for external Drum MIDI smoke (optional for this software checkpoint)
- microSD with `/samples/*.wav` only if sampler hardware smoke is performed
- USB-C data cable

## Wiring

No new wiring.

Cardputer ADV PORT.A is unchanged and unused by this feature:

```text
SDA GPIO2
SCL GPIO1
```

SEQTRAK uses the existing USB-MIDI connection. No USB Host, BLE or ESP-NOW work is introduced.

## Implementation boundary

### Pattern / Song Drums

The existing `PatternPublishingDrumVoice` remains the single trigger wrapper:

```text
pattern hit
  |- local side allowed? -> existing DrumSynthVoice trigger
  |- publish normalized Drum MusicalEvent
                         -> existing MusicalEventQueue
                         -> MIDI NoteOn allowed? -> existing dispatcher
```

No second sequencer or MIDI queue is added.

### Sampler

`DrumSamplerTrack::triggerPad()` checks the same logical Drums internal permission before starting a new sample voice.

Important invariants:

- pad assignments are preserved in every OutputMode;
- `samplerEnabled` is not rewritten by OutputMode;
- no preload/SD scan occurs on an OutputMode decision;
- sampler `process()` has no per-frame OutputMode read;
- legacy raw state keeps sampler preview/internal behavior unchanged.

### PERFORM Drums

Existing normalized lanes `0..7` are reused. The existing local drum wrapper is registered as the one local owner; `InternalSynthOutput` may call it only for explicit INTERNAL/LAYER PERFORM NoteOn.

The optional sampler layer is triggered from the same normalized lane. One byte tracks only sampler pads started by this PERFORM sink so cleanup cannot stop unrelated sampler preview voices.

One-shot samples keep their natural tail on key-up. A looping sample stops on its matching NoteOff. `AllNotesOff` stops only sampler pad voices previously started by this PERFORM sink.

## Build / Flash steps

Focused software contract:

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

Optional hardware smoke before E/F:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

There is still no OutputMode key in D, so the final mode-switch acceptance belongs to 0.9.6-G.

## Expected behavior

### Screen

No new UI in D.

### Existing projects

Old Scenes still use hidden legacy raw state `0`:

```text
Pattern Drums  local drum + enabled sample layer + MIDI
PERFORM Drums  MIDI only
Sampler page   internal preview only
```

Thus opening an existing project does not silently change its source-dependent routing.

### Focused tests

Expected output includes:

```text
Output ownership contract tests: PASS
Output ownership queue tests: PASS
Output ownership source contract: PASS
0.9.6 output ownership contract gate: PASS
```

## Troubleshooting

### MIDI mode unloads or forgets samples

Reject the change. OutputMode is only a trigger/output policy. It must not call sampler scan, preload, `setEnabled(false)`, clear pad IDs, or touch SampleRef persistence.

### Drum hit still sounds locally in explicit MIDI mode

Check `PatternPublishingDrumVoice::triggerPattern()` and the PERFORM internal sink. Both new local NoteOn paths must consult `Track::Drums` ownership.

### Drum hit does not reach SEQTRAK in MIDI/LAYER

Check the existing bounded producer queues and USB status. Do not send TinyUSB directly from the drum/audio path.

### Sampler preview stops when a PERFORM key is released

Only sampler pads actually started by the PERFORM internal sink may be candidates for its cleanup. The one-byte owned-pad mask prevents an unrelated preview pad from being stopped.

## Acceptance checklist

- [ ] Pattern Drums INTERNAL = local only;
- [ ] Pattern Drums MIDI = external only;
- [ ] Pattern Drums LAYER = local + external;
- [ ] same external truth table for PERFORM Drums;
- [ ] legacy Pattern Drums remains local + MIDI;
- [ ] legacy PERFORM Drums remains MIDI-only;
- [ ] sampler follows the Drums internal side for new triggers;
- [ ] sampler assignments remain intact in MIDI mode;
- [ ] sampler enable state remains independent;
- [ ] no SD/WAV load or allocation added to mode decision;
- [ ] no per-frame/per-sample OutputMode read in Drum/Sampler processing;
- [ ] no second drum engine, MIDI queue, dispatcher or scheduler;
- [ ] looping PERFORM sampler pad releases on matching NoteOff;
- [ ] one-shot PERFORM sampler pad keeps natural tail;
- [ ] AllNotesOff cleans only sampler pads owned by PERFORM sink;
- [ ] mute behavior remains orthogonal;
- [ ] focused output ownership tests PASS;
- [ ] full host/Core PASS;
- [ ] SDL PASS;
- [ ] Cardputer ADV normal compile PASS;
- [ ] fixed DRAM gate PASS;
- [ ] SEQTRAK MIDI-only compile PASS.
