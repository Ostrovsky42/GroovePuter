# CALL / RESPONSE partner — phase 0 (capture + log, no sound)

Status: implemented on `genre/cell-source`, host-tested, Cardputer build PASS, not hardware-tested.
Owner spec: `docs/superpowers/plans/2026-10-09-0918-call-response-partner-spec.md` (the decisions
below supersede it where they differ).

## What phase 0 does

PERFORMANCE TOOLS > INPUT > `PARTNER` (row 9, OFF by default, not persisted) = `LISTEN`:
every two-bar window that contains at least one played note prints one line to Serial:

```
[CR] bar=3 mask=0b1000100010001000_1000000000000000 contour=[0,2,4,1] last=2 n=5 reg=60-72 vel=96 oos=0 clk=FREE drop=0
```

- `bar` — 1-based first bar of the window (project bar under PLAY, free-clock bar otherwise);
- `mask` — onsets, bit 0 first, `_` between bar 1 and bar 2;
- `contour` — scale steps of the first ≤ 8 notes relative to the first note;
- `last` — 1-based scale degree of the last note in the PERFORM key/scale;
- `n` notes, `reg` lowest-highest MIDI note, `vel` mean velocity, `oos` out-of-scale notes,
  `clk` PLAY/FREE, `drop` notes dropped by overflow.

Empty windows print nothing. No new note output exists in phase 0.

## Decisions

| Question | Decision | Why |
|---|---|---|
| Phrase length | CALL = 2 bars, fixed window aligned to even bars; answer follows (phase 1) | owner decision |
| Clock | PLAY running: project transport bars. PLAY stopped: PERFORM free clock at the PERFORM tempo, **the first note after silence is bar 1 beat 1**; stops after 2 silent windows. Flag `setFreeClockEnabled(false)` makes capture PLAY-only. Default on | owner decision ("думаю да"). Anchoring to the first note is the only bar grid a person can hear without PLAY (no metronome). This consciously departs from the spec rule "no micros()-based musical boundaries" — only when PLAY is off |
| Playing during the answer | new CALL, not ducking | owner decision. Phase 0 captures every window, so this is already true for capture |
| Answer voice | MIDI channel | owner decision (phase 1) |
| Chord in the CALL | top note of notes sharing a quantized onset | owner decision |
| Answer velocity (Q5) | answer = CALL mean velocity × 80% (`answerVelocityFor`), min 1 | a reply should sit under the question and never be louder; 20% is audible but not a different dynamic layer |
| Answer rests (Q5) | none in phase 0; `kAnswerRestProbabilityPercent = 0` reserved | rests change phrase identity; decide after listening to phase 1 answers |
| What is captured (Q7) | **input** notes: key pitch from the PERFORM key map / raw MIDI IN pitch and velocity, before Smart Play / role interpretation | the contour must describe what the person meant; Smart Play may snap or revoice (Lead/Scale moves chromatic notes), which would make a descriptor of the machine, not of the person. For the answer's *sound* (phase 1+) the sounded note after Smart Play is the right reference (harmony-correct), so both may be needed later |
| onsetMask width | uint32, not uint16 | 2 bars × 16 sixteenths = 32 bits; uint16 would lose a bar or halve the grid |
| Pickup | a note in the last 30% of the step before a window boundary belongs to the next window at onset 0; elsewhere onsets round to the nearest 1/16 | per plan; an early downbeat is the most common human timing error at a phrase start |
| Held across boundary | length clamped to the window end; the later NoteOff is ignored | spec §15; nothing can stay open |
| Overflow | 16 pending notes; the 17th is dropped and counted (`drop=`) | fixed memory |
| Drums target | not captured | not melodic |
| Epoch reset | PLAY start/stop, new transport epoch, PARTNER toggle, free clock stop | old notes must not leak into a new grid |

## Code

- `src/input/performance_input_observer.h` — read-only tap interface + external input tag.
- `src/input/call_response_phrase_capture.h` — `PhraseCapture` (fixed 16 events, 2-bar windows).
- `src/input/call_response_motif.h` — `MotifDescriptor`, `describe()`, `answerVelocityFor()`, log format.
- `src/input/call_response_listener.h` — clock (PLAY / free) + capture, singleton `callResponseListener()`.
- `src/input/performance_keyboard.*` — `setInputObserver()`, `scaleIntervals()`; tap calls in
  `keyDown/keyUp` and identity `externalNoteOn/Off` (all MIDI IN notes reach these). Output unchanged.
- `GroovePuter.ino` — observer wiring in `setup()`, `serviceCallResponseLog()` once per loop after
  external notes are drained. Main loop only; nothing in `src/audio`, nothing in the USB dispatcher.
- `src/ui/pages/perform_page.*` — INPUT row 9 `PARTNER OFF/LISTEN`.

Tests: `bash tests/run_smart_play_tests.sh cr_capture cr_motif cr_tap` (also in the default target list).

## Known limits

- Under PLAY the position is the audio-block timeline extrapolated with `micros()`; it runs ahead
  of what is heard by the output latency (a few ms). Fine for 1/16 quantization; calibrate in phase 1.
- Free clock tempo is `PerformanceKeyboard::tempoBpm()`, updated by the PERFORM page.
- SDL build shows the row but does not wire the observer (no Serial log there).
- Not hardware-tested.

## Phase 1 (next)

Implemented: see `CALL_RESPONSE_PHASE1.md` (decisions there supersede the plan below).

1. Turn-taking state machine on the same clock: LISTEN → CALL window → ANSWER window → LISTEN;
   playing during ANSWER = new CALL (answer notes released at once, NoteOff guaranteed).
2. One transform (ECHO_VARIANT or CONTOUR_REPLY) from `MotifDescriptor` → answer notes in the
   PERFORM key, velocity `answerVelocityFor()`, on the selected MIDI channel via the existing
   router/`scheduleGenerated` path (no new output owner).
3. UI states `OFF / LISTEN / ANSWER`; hardware listening test with the log lines as evidence.
