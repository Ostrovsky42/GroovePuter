# PERFORMANCE TOOLS MIDI output

## Purpose

Make the PERFORM tool layer a real external MIDI performance processor. `ARP`,
`DIRECTION`, `CHORD`, `MEMORY`, `STRUM`, `RATCHET`, `EUCLIDEAN`, and `ROTATE`
must emit transformed Note On/Note Off events through the existing musical-event
router and the single USB-MIDI dispatcher. Internal synth sound is optional and
is not the acceptance source of truth.

## Hardware list

- M5Stack Cardputer-Adv.
- USB-C data cable.
- Yamaha SEQTRAK, a computer MIDI monitor, or another class-compliant USB-MIDI target.

## Wiring

No GPIO or PORT.A wiring is required.

- Connect Cardputer-Adv USB-C to the host or USB-MIDI target with a data-capable cable.
- Synth A uses MIDI channel 8, Synth B channel 9, and DX channel 10.
- Performance transforms are melodic and are not enabled for the seven native drum lanes.

## Build / Flash steps

```bash
git fetch origin
git checkout agent/performance-tools-midi-output
./tests/run_host_tests.sh
./scripts/build.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

Flash the generated Cardputer-Adv firmware using the normal project workflow.

## Expected behavior

1. Stop Pattern/SMF transport and enable NOTE mode.
2. Select Synth A, Synth B, or DX as the PERFORM target.
3. Press `Tab` to open `PERFORMANCE TOOLS`.
4. The screen shows the complete labels without `ARP/DIR/CHD/MEM/STR/RAT/EUC/ROT` abbreviations:
   - `1 ARPEGGIATOR`
   - `2 DIRECTION`
   - `3 CHORD`
   - `4 MEMORY`
   - `5 STRUM`
   - `6 RATCHET`
   - `7 EUCLIDEAN`
   - `8 ROTATE`
5. Transformed notes appear on the selected external MIDI channel even when the internal synth is muted or not useful.
6. Chord, memory, and strum can keep several generated notes active simultaneously.
7. Arpeggiator, ratchet, and Euclidean gates produce balanced Note On/Note Off pairs.
8. Releasing physical keys, changing a tool, changing target, disabling NOTE mode, starting transport, or pressing Panic leaves no stuck external notes.

## Troubleshooting

- Direct keys reach MIDI but tools do not: verify the build contains the bounded `Arpeggiator` USB ownership path in `usb_midi_output.cpp`.
- Only the last chord note arrives: verify generated MIDI uses the fixed polyphonic bitsets rather than a monophonic `MidiVoiceLane`.
- Notes remain stuck after changing a tool: verify a `PerformanceKeyboard` target panic also calls `releaseGeneratedTarget()`.
- No external events arrive: verify the USB cable carries data, the target port is open, and the selected channel is 8, 9, or 10.
- Internal sound differs from external MIDI: use the external MIDI monitor as the acceptance source; internal engines remain monophonic and are not required to reproduce the complete transformed chord.
- Tools are silent while transport runs: this is expected; PERFORM live input remains locked while PatternPlayer owns transport.

## Acceptance checklist

- [ ] `./tests/run_host_tests.sh` passes.
- [ ] SDL build passes.
- [ ] Cardputer-Adv firmware build and fixed-DRAM gate pass.
- [ ] Tool overlay shows all eight full labels without abbreviations.
- [ ] Synth A emits transformed MIDI on channel 8.
- [ ] Synth B emits transformed MIDI on channel 9.
- [ ] DX emits transformed MIDI on channel 10.
- [ ] `1 ARPEGGIATOR` produces an external note sequence.
- [ ] `2 DIRECTION` visibly changes the external arpeggio order.
- [ ] `3 CHORD` produces multiple simultaneous external MIDI notes.
- [ ] `4 MEMORY` captures two or more held notes and transposes their intervals from a new key.
- [ ] `5 STRUM` spreads chord Note On events in time.
- [ ] `6 RATCHET` produces repeated balanced gates.
- [ ] `7 EUCLIDEAN` changes which sixteenth-note gates are emitted.
- [ ] `8 ROTATE` shifts the Euclidean phase audibly/visibly in the MIDI monitor.
- [ ] Releasing all keys sends all required Note Off events.
- [ ] `X Panic`, target change, NOTE off, and transport start leave no stuck notes.
- [ ] PatternPlayer/SMF ownership sharing is not interrupted by generated-note cleanup.
