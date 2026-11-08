# CALL / RESPONSE partner — phase 1 (answer from a template)

Status: implemented on `genre/cell-source`, host-tested, Cardputer build PASS, **not hardware-tested**.
Builds on phase 0 (`CALL_RESPONSE_PHASE0.md`): capture, clock and `[CR]` log are unchanged.

## What phase 1 does

PERFORMANCE TOOLS > INPUT > `PARTNER` (row 9) now cycles `OFF / LISTEN / ANSWER` (ENTER, `<`/`>`).
Not persisted, OFF at boot.

- `LISTEN` = phase 0 exactly (log only, no sound).
- `ANSWER`: after every two-bar CALL window that had notes, if the person is quiet, the partner plays
  an answer in the next two bars on the **PERFORM DX MIDI route** (USB + DIN), and logs it:

```
[CR]   bar=1 mask=0b1000100010000000_0000000000000000 contour=[0,1,2] last=3 n=3 reg=60-64 vel=100 oos=0 clk=FREE drop=0
[CR-A] bar=3 at=+1 n=3 cut=0 notes=0:60:80:4,4:62:80:4,8:60:80:8
[CR-A] cancel played=2 human=1
```

`notes` = `step:note:vel:len`, step counted from the answer start (window start + `at`).
`cut` = CALL notes that did not fit after the delay. `cancel` = the answer was cut short
(`played` notes had started; `human` = total cancels caused by the person playing).

## Decisions

| Question | Decision | Why |
|---|---|---|
| Windows | CALL = a closed non-empty 2-bar window; ANSWER = the next 2-bar window on the same clock (PLAY bars or PERFORM free clock). Every window is still captured, so a window in which the person plays is the next CALL | owner decision; no new clock, no new grid |
| Answer delay | the answer starts **1 sixteenth** after the ANSWER window boundary (`kAnswerDelaySteps = 1`); the whole figure is shifted, so its rhythm is the CALL rhythm | a person who keeps playing on the downbeat can be up to ½ step late and still be onset 0. Waiting one sixteenth means such a note is seen (and cancels the answer) before the first answer note sounds, instead of a blip on the downbeat. One sixteenth (125 ms at 120 BPM) is the smallest grid shift ≥ ½ step; it reads as a reply that comes in just after the bar line. The window close is also detected up to one loop pass late; the delay absorbs that too |
| Cost of the delay | a CALL note on the last sixteenth (onset 31) cannot be answered (would cross into the next CALL window); it is dropped and counted (`cut=`) | the ANSWER window must end at the boundary so it never overlaps the next CALL |
| When there is no answer | LISTEN mode; or the person already has a note in the ANSWER window when the CALL closes (pickup in the last 30% of the step, or a note on/after the boundary); or the clock skipped windows (stalled loop) | the person is still playing: it is one longer CALL, not a question |
| Transform | **ECHO_VARIANT** only: same onsets, same lengths (trimmed to a monophonic line), CALL pitches snapped down to the key, and the **ending changes**: CALL ends away from the tonic → answer ends on the tonic; CALL ends on the tonic → answer ends on the fifth (nearest scale tone to the fifth when the scale has none, e.g. Locrian → b5). Ending note = nearest instance to the CALL's last note, tie goes down | the person must recognise their own phrase coming back and hear that it *answers* (antecedent → consequent: half/open ending → full ending). Rhythm identity is what makes it a reply. CONTOUR_REPLY (inversion / fifth shift) changes the phrase more, can leap awkwardly and needs range/voice-leading rules we cannot judge without listening; it is deferred, not implemented |
| Key | Smart Play harmony key (`PerformanceContext.rootMidi` + `scalePitchClasses`) when the project provides harmony (`hasHarmony`), otherwise the PERFORM ROOT/SCALE | without harmony `PerformanceContext` is chromatic C (defaults), which would not be "in the key"; with harmony it is what PERFORM itself sounds in |
| Register | answer keeps the CALL register; the whole line is moved by octaves into **24..71** if needed, a note still outside is folded by octaves | the answer should sit where the question was; 24..71 is the 303 range and the range given for the answer. The "48..72 relative to the last note" wording was read as "stay next to the CALL's last note": the ending is the nearest tonic/fifth to that note |
| Velocity | `answerVelocityFor(CALL mean)` = 80 %, min 1, all notes | decided in phase 0. **Superseded:** the code has used 100 % since the first commit; phase 6 makes it per note — see `CALL_RESPONSE_PHASE6_MUSIC.md` |
| Rests | none (`kAnswerRestProbabilityPercent = 0`) | decided in phase 0 |
| Voice | source `Arpeggiator` (existing generated-performance source), target **Dx** → `UsbMidiOutput` sends on the PERFORM DX channel (default MIDI ch 10 = SEQTRAK DX track; changeable in PROJECT > MIDI), USB and DIN through the tee transport. `InternalSynthOutput` ignores Dx, so internal synths (with or without a Pattern) are never touched | owner decision "answer = a MIDI channel"; no new `MusicalEventSource` → no change to `UsbMidiOutput`, `InternalSynthOutput`, output ownership (files with other tasks' edits) |
| Scheduling owner | `ResponsePlayer`: own fixed 16-note table (on/off micros, note, vel, state), serviced once per loop. Not `PerformanceKeyboard::scheduleGenerated` | the keyboard calls `clearScheduled()` on many of its own transitions (arp reconfig, poly-chord reconcile, key release, panic): answer NoteOffs living there could be dropped → stuck notes; and `discardScheduledGeneratedNoteOns()` would also hit the arp. A separate table makes "cancel the answer" touch only the answer, and the arp never touches the answer |
| Playing during ANSWER | the person's NoteOn (input tap, before Smart Play) cancels: pending answer NoteOns dropped, sounding answer notes get NoteOff **immediately** (before the person's own note is routed for built-in keys), and the note is captured as the start of the new CALL | owner decision; covered by `cr_cancel` |
| Other stops | PLAY start/stop, new transport epoch, free clock stop, PARTNER OFF, ANSWER→LISTEN, live-input epoch panic in the loop: all cancel with immediate NoteOffs | no hanging notes on any epoch change |
| Late loop | a NoteOn is sent on the first loop pass at/after its time; a note whose NoteOff time has already passed is skipped (counted `staleDrops`) | late is better than missing; zero-length notes are worse than missing |

## Code

- `src/input/call_response_planner.h` — `ResponseKey`, `ResponsePlan` (≤ 16 notes, step/len/note/vel),
  `planEchoVariant()` (pure), `formatResponseLog()` (`[CR-A]`).
- `src/input/call_response_responder.h` — `ResponsePlayer` (fixed table, `start/service/cancel`),
  `answerStepDueMicros()`.
- `src/input/call_response_listener.h` — `Mode {Off, Listen, Answer}`, owns the `ResponsePlayer`,
  cancel on person NoteOn and on every epoch change, `microsAtStep()`, `answerPhrase()`.
- `GroovePuter.ino` — router wiring; `serviceCallResponseLog()` plans + logs `[CR-A]`; cancel log;
  cancel on live-input epoch panic.
- `src/ui/pages/perform_page.cpp` — PARTNER value `OFF/LISTEN/ANSWER`, hint
  `OFF/LISTEN/ANSWER: REPLY ON MIDI DX CH` (38 chars).

Nothing in `src/audio`, nothing in the USB dispatcher, no allocation, no locks.

DRAM (`Cardputer DRAM globals`): **185292** with phase 1 vs **185084** for the same tree without it
(**+208 B**: the listener singleton grows 348 → 568 B incl. the 16-note answer table, +4 B log state).
The listener now has a non-zero default (Dx target), so it sits in `.dram0.data` (same DRAM).

## Tests

`bash tests/run_smart_play_tests.sh cr_planner cr_cancel cr_timing` (also in the default list):

- `cr_planner` — mask = CALL mask, last note differs, all in key, 24..71, never past the window,
  mono line, velocity 80 %, tonic/fifth/b5 endings, onset-31 drop, `[CR-A]` format and buffer fit.
- `cr_cancel` — person NoteOn mid-answer: one immediate NoteOff, no later answer NoteOn, the note is
  the new CALL; quiet window plays to the end; pickup → no answer; LISTEN → no sound; ANSWER→LISTEN,
  OFF, PLAY start each release the sounding note; NoteOn/NoteOff balanced; through
  `PerformanceKeyboard` the cancel leaves the arpeggiator running.
- `cr_timing` — 1/16 = 166667 / 125000 / 107143 µs at 90 / 120 / 140 BPM; answer NoteOn/NoteOff at
  CALL start + 33 steps on the free clock (also across the `micros()` wrap) and from the transport
  position under PLAY.

## Known limits / not verified

- Not hardware-tested: answer audibility on SEQTRAK DX / DIN, the feel of the 1/16 delay, cancel
  latency with a real loop period.
- If PERFORM itself targets DX, the person and the answer share the MIDI channel; `UsbMidiOutput`
  may flip SEQTRAK CC26 mono/poly between them (Arpeggiator requests poly).
- Generated-note ownership in `UsbMidiOutput` is per target, not per source: if the PERFORM arp also
  plays on DX and hits the same pitch as the answer, one NoteOff ends both (no stuck note).
- Under PLAY the answer is timed from the audio-block timeline extrapolated with `micros()` when
  the CALL closes; a tempo change during the 2-bar answer is not followed (the next answer is).
- SDL build: the row cycles, but nothing is wired (no router, no log), as in phase 0.
