# CALL/RESPONSE phase 9: PARTNER LENGTH replaces PARTNER DELAY

Status: implemented, host tests green, not hardware-tested. 8 bars: not done (out of scope).

## Decision

- **DELAY is removed** from the menu and from the logic. Every answer (ANSWER, DUET, LOOP) starts exactly on
  the downbeat of its window. There is no safety sixteenth and no setting for it.
- **LENGTH is added** on the same PERFORM > INPUT row (12) where DELAY was: `100 %` (default) → `75 %` →
  `50 %` → `25 %` → `100 %` (`<` goes backwards). Session only, not saved, like the other PARTNER rows.

## Why

- DELAY 1/16 existed so that a person who keeps playing on the downbeat is heard before the answer and cancels
  it. In practice it made the answer late by a sixteenth, cut a CALL note on the last step, and since phase 7
  LOOP ignored it anyway (LOOP was always on the grid). Two timing behaviours for one partner were one too many.
- The audible problem left in the partner is articulation, not timing: the answer copies the CALL's held
  lengths, so a legato CALL gets a legato answer. LENGTH lets the player make the reply staccato without
  touching rhythm or pitch.

## What was removed

| Where | Removed |
|---|---|
| `call_response_planner.h` | `kAnswerDelaySteps`, `kAnswerMaxDelaySteps`, `clampAnswerDelay()`, `ResponsePlan::delaySteps`, the `delaySteps` parameter of `detail::echoLine()` and `planEchoVariant()`, `at=+N` in the `[CR-A]` log line |
| `call_response_harmony.h` | `delaySteps` parameter of `planHarmonicEcho()` |
| `call_response_development.h` | `delaySteps` parameter of `transformCall()` |
| `call_response_performance_plan.h` | `delay` parameter of `planPerformanceReply()` (and the `clampAnswerDelay` in the EVOLVE 3 shift) |
| `call_response_listener.h` | `answerDelaySteps_`, `setAnswerDelaySteps()`, `answerDelaySteps()`, `cycleAnswerDelay()`, `replyDelaySteps()`, the `plan.delaySteps` origin offset in `startAnswer()` |
| `GroovePuter.ino` | `replyDelaySteps()` in the harmony window start and in `planPerformanceReply()` |
| `perform_page.cpp` | the DELAY row (`0 GRID` / `1/16`, N/A in LOOP), its LOOP guard and its two hints |

The echoLine guard "a note outside the window is dropped; a lone one is answered on the last step" stays: with
no delay a captured phrase always fits, so it only protects against a phrase that breaks the capture contract
(tested with a 16-step window and onset 20).

## What was added

- `AnswerLength { Full, ThreeQuarters, Half, Quarter }`, `answerLengthPercent()`, `scaledAnswerNoteLength()`,
  `applyAnswerLength()` in `call_response_planner.h` (pure, constexpr where possible).
- Listener: `answerLength_` (1 byte, in the slot of the old `answerDelaySteps_`), `setAnswerLength()`,
  `answerLength()`, `answerLengthPercent()`, `cycleAnswerLength()`.
- `startAnswer()` applies LENGTH to the plan right before `ResponsePlayer::start()`. Every reply of every mode
  goes through it (ANSWER via `answerPhrase`, DUET/LOOP via `answerPhrase` or the `.ino` path), including every
  LOOP replay, which is replanned from the saved loop each window. The plan is modified in place, so the
  `[CR-A]` log shows the lengths that actually sound; the line now ends with `len=<percent>`.
- PERFORM: row 12 ` LENGTH  100 %`, active whenever the partner is on; hint `</> ANSWER LENGTH: 100/75/50/25 %`
  (33 characters).

## How LENGTH changes the notes

`length' = max(1, ceil(length × p / 100))`, p ∈ {100, 75, 50, 25}. Onsets, pitches and velocities are untouched;
the new length is never longer than the old one, so the line stays monophonic and inside the window.

| planned | 75 % | 50 % | 25 % |
|---|---|---|---|
| 1 | 1 | 1 | 1 |
| 2 | 2 | 1 | 1 |
| 3 | 3 | 2 | 1 |
| 4 | 3 | 2 | 1 |
| 7 | 6 | 4 | 2 |
| 8 | 6 | 4 | 2 |
| 16 | 12 | 8 | 4 |

A change takes effect from the next started reply; a sounding reply keeps its already-timed NoteOffs.

## Behaviour changes from removing DELAY (deliberate)

- ANSWER/DUET start on the downbeat (was +1/16). Answer onsets are now exactly the CALL onsets.
- A CALL note on the last step (31 / 15 / 63) is answered on its own step (was cut, or for a lone note moved
  one step earlier).
- The last answer note may now last to the window end (e.g. 24+8 = 32, was clamped to 31).
- EVOLVE 3 / DEVELOP shifts can use the last step (onset 14 → 15 in a 16-step window).
- On the free clock the main loop sees the window boundary a few ms late (one pass); the first answer NoteOn now
  goes out on that pass, i.e. a few ms late. The delay used to hide this; LOOP already behaved like this since
  phase 7 (`late=` in the log).
- In ANSWER, a person who plays on right at the downbeat (not as a pickup) can let the first answer note sound
  for one main-loop pass before their NoteOn cancels it (NoteOff at once, balanced). This is the trade-off the
  safety sixteenth used to cover.

## NoteOff / cancel

Not touched. `ResponsePlayer` and the cancel path are unchanged; LENGTH only changes the `length` of plan notes
before `start()`. Tests check balanced NoteOn/NoteOff at every LENGTH, an immediate NoteOff on a human cancel,
OFF and X (silence) during a shortened note, and a cancel between two shortened notes (nothing to release).

## Tests

New `tests/test_cr_phase9_length.cpp` (`cr_phase9` in `run_smart_play_tests.sh`):
rule for 0..64 steps at each LENGTH; cycle order and reset; plan-level apply; ANSWER and DUET at every LENGTH
against a 100 % rig (same NoteOn times, NoteOff after the scaled length); LOOP 50 % for EVOLVE 0..3 over the
reply + 5 replays; cancels.

Updated expectations (delay removed): `test_cr_response_planner` (window 32, last-step note kept, 24+8 = 8,
log line), `test_cr_partner_settings` (answer on 32/16, DELAY test → `testAnswerOnGrid`, default LENGTH),
`test_cr_answer_timing` (32 not 33, first NoteOn on the late boundary pass; transport onset 80 not 81),
`test_cr_response_cancel` (C sounds at 32, D at 36), `test_cr_single_note` (onset-31 note of a multi-note CALL
now kept), `test_cr_four_bars` (window 64, step 63, start 64), `test_cr_development` (onset 14 → 15 shift, edge
check on 15), `test_cr_loop_grid` (delay-only test removed, `windowSteps == 32` instead of `delaySteps == 0`).
Delay axes removed from sweeps (`test_cr_loop_gaps`, `test_cr_four_bars`, `test_cr_phase6_music`,
`test_cr_development`), so their counts halve (loop gaps 19 584 → 9 792 windows, clocks 4 608 → 2 304): the
delay=1 half was a duplicate for LOOP and no longer exists for ANSWER/DUET.

## Memory

| build | before | after | Δ |
|---|---|---|---|
| `build_cardputer_dynbuffers.sh` DRAM globals | 185796 | **185796** | **+0 B** |
| `build_seqtrak_midi_only.sh` DRAM globals | 186236 | **186236** | **+0 B** |

`answerLength_` replaces `answerDelaySteps_` (1 byte for 1 byte, `sizeof(CallResponseListener)` = 1080 both).
`ResponsePlan` 132 → 131 B (stack only, main loop).
