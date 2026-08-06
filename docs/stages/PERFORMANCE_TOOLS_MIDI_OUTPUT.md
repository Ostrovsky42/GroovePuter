# PERFORMANCE TOOLS MIDI output

## Purpose

Make the PERFORM tool layer a real external MIDI performance processor during
both stopped and running GroovePuter transport. Direct keyboard notes, chords,
and strums remain playable while transport runs. `ARP`, `RATCHET`, and
`EUCLIDEAN` use the same `ProjectTransportTimeline` sixteenth-note phase that
feeds outbound MIDI Clock, so an external recorder such as SEQTRAK receives the
performance at the project tempo instead of an unrelated free-running timer.

The generated notes still use the existing bounded live-event queue and the
single USB-MIDI dispatcher. This stage phase-locks step selection to the audio
transport; it does not add a second USB owner or replace the scheduled Pattern
and SMF queues.

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
7. With transport running, the step engine reads `ProjectTransportTimeline.absoluteSteps()` and advances only when the absolute sixteenth changes.
8. Euclidean phase is the absolute transport step modulo 16, so `16/16` produces one gate per project sixteenth and rotation remains bar-stable.
9. A delayed UI/control iteration emits only the current live step; it does not replay a burst of missed sixteenths.
10. Starting or stopping transport preserves physically held keys. Step-generated notes are cleaned before changing clock domains, preventing stuck notes.
11. PERFORM shows `PLAYING | ... | LIVE SYNC` for active step tools instead of `INPUT LOCK | PATTERN PLAYER ACTIVE`.

## Troubleshooting

- Direct keys work when stopped but not while transport runs: verify the build contains `liveInputAllowed()` without a `transportPlaying_` condition and that NOTE mode is enabled.
- Arp starts immediately in the middle of a step: verify the running build uses `ProjectTransportTimeline` and waits for the next absolute-step transition.
- Arp speed still differs from SEQTRAK recording: confirm GroovePuter is GP MASTER, SEQTRAK follows external MIDI Clock, and both devices show the same BPM.
- Step tools pause briefly during transport: inspect whether the project timeline is valid. The engine intentionally freezes instead of falling back to `micros()` while transport is marked running.
- A long UI or SD operation causes several notes at once: verify the no-catch-up transport path emits one current step rather than iterating all missed ordinals.
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
- [ ] `1 ARPEGGIATOR` advances once per project sixteenth during transport.
- [ ] `2 DIRECTION` visibly changes the external arpeggio order.
- [ ] `3 CHORD` produces multiple simultaneous external MIDI notes during transport.
- [ ] `4 MEMORY` captures two or more held notes and transposes their intervals from a new key.
- [ ] `5 STRUM` spreads chord Note On events in time during transport.
- [ ] `6 RATCHET` produces repeated balanced gates inside each project sixteenth.
- [ ] `7 EUCLIDEAN 16/16` produces one gate on every transport sixteenth.
- [ ] `8 ROTATE` shifts the Euclidean phase without changing transport tempo.
- [ ] Holding a note before GP Start continues into synchronized step playback without a stuck note.
- [ ] A jump across several transport steps emits one current step, not a catch-up burst.
- [ ] Stopping transport returns the step engine to the standalone BPM clock.
- [ ] Releasing all keys sends all required Note Off events.
- [ ] `X Panic`, target change, and NOTE off leave no stuck notes.
- [ ] PatternPlayer/SMF ownership sharing is not interrupted by generated-note cleanup.
