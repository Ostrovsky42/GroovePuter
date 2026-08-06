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

The same PR also adds MIDI Player `FILE MASTER` ownership for the verified
recording path:

```text
SMF FILE BPM -> GroovePuter BPM -> GroovePuter MIDI CLOCK -> SEQTRAK
```

`FILE MASTER` reuses the existing PROJECT scheduler. It does not add another
SMF scheduler, clock loop, task, USB writer, or MIDI dispatcher.

## Hardware list

- M5Stack Cardputer-Adv.
- USB-C data cable.
- Yamaha SEQTRAK, a computer MIDI monitor, or another class-compliant USB-MIDI target.
- SD card containing a Standard MIDI File for the FILE MASTER check.

## Wiring

No GPIO or PORT.A wiring is required.

- Connect Cardputer-Adv USB-C to the host or USB-MIDI target with a data-capable cable.
- Configure SEQTRAK to receive external MIDI Clock from GroovePuter.
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

### PERFORM tools

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
14. `STRUM` shows `N/A` while Arp is active or Chord is off.
15. `ROTATE` shows `N/A` for Euclidean `0/16` and `16/16`, where rotation cannot change the mask.

### MIDI Player FILE MASTER

1. Load an SMF in MIDI Player.
2. Press `T` to cycle:

   ```text
   ORIGINAL -> GP MASTER -> FILE MASTER -> ORIGINAL
   ```

3. Entering `FILE MASTER` temporarily reads the file's original BPM, restores the accepted PROJECT scheduling path, forces `GP INTERNAL`, and applies the file BPM through `AudioGuard`.
4. The normal Player view shows:

   ```text
   FILE <BPM> -> GP -> USB CLOCK RUN|STOP
   ```

5. `G` starts/stops the GroovePuter transport and therefore the outbound MIDI Clock.
6. `Space` arms/plays the SMF against that same GroovePuter timeline.
7. `C` cannot switch to SEQ MASTER while FILE MASTER is active.
8. Up/Down cannot manually drift GP BPM while FILE MASTER is active.
9. `O` reapplies the detected file BPM to GroovePuter.
10. Loading another file while FILE MASTER remains selected refreshes its BPM before playback.

FILE MASTER currently locks GroovePuter to the file tempo detected for the active
playback position. It is intended for constant-tempo files such as the supplied
120 BPM S.T.A.L.K.E.R. sample. Full automation of multiple in-file tempo changes
is not claimed by this stage.

MIDI Clock transmits tempo and transport, not the SMF time-signature map. A 3/4
file recorded into a SEQTRAK project that remains 4/4 will keep correct real-time
spacing, but its bar numbers will differ. For example, eight 3/4 bars occupy six
4/4 bars at the same BPM.

## Troubleshooting

- Direct keys work when stopped but not while transport runs: verify NOTE mode is enabled and `liveInputAllowed()` does not depend on `transportPlaying_`.
- Arp starts immediately in the middle of a step: verify the build prepares `currentOrdinal + 1` rather than emitting the current step on detection.
- Arp gradually changes feel across the bar: verify the transport block anchor advances from the shared dispatcher playback anchor instead of resetting to `micros()` on every step.
- Dense Chord Memory + Ratchet becomes silent: verify `kMaxScheduledEvents` is 112 and the next step is not prepared before half of the current step drains.
- Several delayed notes appear after an SD/UI pause: verify stale generated NoteOn events are dropped after 12 ms; cleanup NoteOff events must still be sent.
- STRUM is inaudible: turn Arp off, select a Chord mode, set Ratchet x1, then compare 0 ms and 36 ms.
- ROTATE is inaudible: use a partial Euclidean mask such as 5/16; 0/16 and 16/16 are intentional no-op masks.
- FILE MASTER remains on `READ FILE BPM` or `RESTORE GP SYNC`: stop playback, wait for the bounded player command queue to drain, and press `T` to exit/re-enter the mode.
- The S.T.A.L.K.E.R. recording still follows 160 BPM: confirm the screen says `FILE 120.0 > GP > USB CLOCK`, GroovePuter shows 120 BPM, and SEQTRAK is following external Clock.
- FILE MASTER reports the right BPM but bar counts differ: confirm the SMF is 3/4 while the SEQTRAK project remains 4/4; this is meter representation, not tempo drift.
- Step tools pause briefly during transport: inspect whether the project timeline is valid. The engine intentionally freezes instead of falling back to `micros()` while transport is marked running.
- Only the last chord note arrives: verify generated MIDI uses the fixed polyphonic bitsets rather than a monophonic `MidiVoiceLane`.
- Notes remain stuck after changing a tool: verify a `PerformanceKeyboard` target panic also calls `releaseGeneratedTarget()`.
- No external events arrive: verify the USB cable carries data, the target port is open, and the selected channel is 8, 9, or 10.

## Acceptance checklist

### Automated

- [ ] `./tests/run_host_tests.sh` passes.
- [ ] SDL build passes.
- [ ] Cardputer-Adv firmware build passes.
- [ ] Fixed-DRAM gate passes.
- [ ] Cardputer-Adv SEQTRAK MIDI-only build passes.

### PERFORM

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
- [ ] `5 STRUM` audibly separates a chord at 36 ms with Arp off.
- [ ] `5 STRUM` shows `N/A` for single-note/Arp operation.
- [ ] `6 RATCHET` produces repeated balanced gates inside each project sixteenth.
- [ ] `7 EUCLIDEAN 5/16` produces a partial gate mask.
- [ ] `8 ROTATE` audibly shifts that 5/16 mask.
- [ ] `8 ROTATE` shows `N/A` at 0/16 and 16/16.
- [ ] Eight-note Chord Memory with Ratchet x4 reaches the first boundary without an automatic panic or silence gap.
- [ ] Holding the dense pattern for at least four bars does not progressively slow near the bar end.
- [ ] A deliberate UI/SD stall causes a missing stale hit, not a delayed cluster.
- [ ] Releasing all keys sends all required Note Off events.
- [ ] `X Panic`, target change, and NOTE off leave no stuck notes.

### FILE MASTER with the supplied S.T.A.L.K.E.R. SMF

- [ ] `T` cycles ORIGINAL -> GP MASTER -> FILE MASTER -> ORIGINAL.
- [ ] FILE MASTER resolves to `FILE 120.0 > GP > USB CLOCK`.
- [ ] Selecting FILE MASTER forces GP INTERNAL and blocks `C` source switching.
- [ ] `G` starts GroovePuter at 120 BPM and SEQTRAK reports/follows the same clock.
- [ ] `Space` starts the SMF from the accepted PROJECT scheduler without a second clock.
- [ ] Recording and playback on SEQTRAK preserve the same inter-note timing.
- [ ] Eight source bars of 3/4 occupy six SEQTRAK bars of 4/4, not eight bars caused by the former 120-vs-160 mismatch.
- [ ] Up/Down cannot alter the locked FILE MASTER BPM.
- [ ] `O` restores 120 BPM after any external project change.
- [ ] Loading a second constant-tempo SMF refreshes the displayed and transmitted BPM.
- [ ] Stop, restart, source rejection, target change, and Panic leave no stuck notes.
