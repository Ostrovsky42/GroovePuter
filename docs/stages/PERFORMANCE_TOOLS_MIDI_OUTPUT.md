# PERFORMANCE TOOLS MIDI output

## Purpose

Make the PERFORM tool layer a real external MIDI performance processor during
both stopped and running GroovePuter transport. Direct keyboard notes, chords,
and strums remain playable while transport runs. `ARP`, `RATCHET`, and
`EUCLIDEAN` use the same `ProjectTransportTimeline` sixteenth-note phase that
feeds outbound MIDI Clock, so an external recorder such as SEQTRAK receives the
performance at the project tempo instead of an unrelated free-running timer.

The next transport step is prepared before its boundary from the audio-block
phase. This removes per-step main-loop detection drift while retaining the
existing bounded control queue and single USB-MIDI dispatcher. The local
scheduler remains static and allocation-free.

## Hardware list

- M5Stack Cardputer-Adv.
- USB-C data cable.
- Yamaha SEQTRAK, a computer MIDI monitor, or another class-compliant USB-MIDI target.

## Wiring

No GPIO or PORT.A wiring is required.

- Connect Cardputer-Adv USB-C to the host or USB-MIDI target with a data-capable cable.
- Configure SEQTRAK to follow external MIDI Clock when GroovePuter is GP MASTER.
- Synth A uses MIDI channel 8, Synth B channel 9, and DX channel 10.
- Performance transforms are melodic and are not enabled for the seven native drum lanes.

## Build / Flash steps

```bash
git fetch origin
git checkout feature/transport-synced-performance-tools
./tests/run_host_tests.sh
./scripts/build.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

Flash the generated Cardputer-Adv firmware using the normal project workflow.

## Expected behavior

1. Open `PERFORM` and keep NOTE mode enabled.
2. Select Synth A, Synth B, DX, or the native Drum target.
3. Direct Cardputer keyboard Note On/Note Off input remains active while the GP transport is running.
4. Press `Tab` to open `PERFORMANCE TOOLS`.
5. The screen shows the complete labels:
   - `1 ARPEGGIATOR`
   - `2 DIRECTION`
   - `3 CHORD`
   - `4 MEMORY`
   - `5 STRUM`
   - `6 RATCHET`
   - `7 EUCLIDEAN`
   - `8 ROTATE`
6. With transport stopped, Arp/Ratchet/Euclidean retain the standalone `micros()` clock at the current project BPM.
7. With transport running, the next absolute sixteenth is scheduled ahead from the current `ProjectTransportTimeline` block phase.
8. Euclidean phase is the absolute transport step modulo 16, so `16/16` produces one gate per project sixteenth and rotation remains bar-stable.
9. The following step is prepared after half of the current step has drained. This bounds overlap for dense Chord Memory + Ratchet combinations.
10. The fixed local queue has 112 slots. A maximum 8-note, x4-ratchet step needs 64 NoteOn/NoteOff slots and no longer consumes the entire queue.
11. A NoteOn more than 12 ms late is dropped rather than emitted as a catch-up burst. NoteOff remains cleanup-critical.
12. Starting or stopping transport preserves physically held keys. Step-generated notes are cleaned before changing clock domains.
13. PERFORM shows `PLAYING | ... | LIVE SYNC` for active step tools instead of `INPUT LOCK | PATTERN PLAYER ACTIVE`.

## Troubleshooting

- Direct keys work when stopped but not while transport runs: verify NOTE mode is enabled and `liveInputAllowed()` does not depend on `transportPlaying_`.
- Arp starts immediately in the middle of a step: verify the build prepares `currentOrdinal + 1` rather than emitting the current step on detection.
- Arp gradually changes feel across the bar: verify the transport block anchor advances from `blockSequence * blockDuration` instead of resetting to `micros()` on every step.
- Dense Chord Memory + Ratchet becomes silent: verify `kMaxScheduledEvents` is 112 and the next step is not prepared before half of the current step drains.
- Several delayed notes appear after an SD/UI pause: verify stale generated NoteOn events are dropped after 12 ms; cleanup NoteOff events must still be sent.
- Arp speed still differs from SEQTRAK recording: confirm GroovePuter is GP MASTER, SEQTRAK follows external MIDI Clock, and both devices show the same BPM.
- Step tools pause briefly during transport: inspect whether the project timeline is valid. The engine intentionally freezes instead of falling back to `micros()` while transport is marked running.
- Only the last chord note arrives: verify generated MIDI uses the fixed polyphonic bitsets rather than a monophonic `MidiVoiceLane`.
- Notes remain stuck after changing a tool: verify a `PerformanceKeyboard` target panic also calls `releaseGeneratedTarget()`.
- No external events arrive: verify the USB cable carries data, the target port is open, and the selected channel is 8, 9, or 10.
- Internal sound differs from external MIDI: use the external MIDI monitor as the acceptance source; internal engines remain monophonic and are not required to reproduce the complete transformed chord.

## Acceptance checklist

- [ ] `./tests/run_host_tests.sh` passes.
- [ ] SDL build passes.
- [ ] Cardputer-Adv firmware build and fixed-DRAM gate pass.
- [ ] NOTE-mode direct keys emit Note On/Note Off while GP transport runs.
- [ ] Starting transport does not send a blanket performance panic or clear held physical keys.
- [ ] Tool overlay shows all eight full labels without abbreviations.
- [ ] Synth A emits transformed MIDI on channel 8.
- [ ] Synth B emits transformed MIDI on channel 9.
- [ ] DX emits transformed MIDI on channel 10.
- [ ] `1 ARPEGGIATOR` starts on the next project sixteenth and keeps the same spacing through the bar wrap.
- [ ] `2 DIRECTION` visibly changes the external arpeggio order.
- [ ] `3 CHORD` produces multiple simultaneous external MIDI notes during transport.
- [ ] `4 MEMORY` captures two or more held notes and transposes their intervals from a new key.
- [ ] `5 STRUM` spreads chord Note On events in time during transport.
- [ ] `6 RATCHET` produces repeated balanced gates inside each project sixteenth.
- [ ] `7 EUCLIDEAN 16/16` produces one gate on every transport sixteenth.
- [ ] `8 ROTATE` shifts the Euclidean phase without changing transport tempo.
- [ ] Eight-note Chord Memory with Ratchet x4 reaches the first boundary without an automatic panic or silence gap.
- [ ] Holding the dense pattern for at least four bars does not progressively slow near the bar end.
- [ ] A deliberate UI/SD stall causes a missing stale hit, not a delayed cluster.
- [ ] Stopping transport returns the step engine to the standalone BPM clock.
- [ ] Releasing all keys sends all required Note Off events.
- [ ] `X Panic`, target change, and NOTE off leave no stuck notes.
- [ ] PatternPlayer/SMF ownership sharing is not interrupted by generated-note cleanup.
