# CALL / RESPONSE partner — phase 5 (LOOP on the grid, REPLACE threshold)

Status: implemented on `genre/cell-source` (on top of 6dec977b + the uncommitted phase 4), host-tested,
Cardputer build PASS, **not hardware-tested, not listened to**. Only PARTNER `LOOP` changes. ANSWER and
DUET output is byte-identical to phase 4 (checked, see below).

## 1. Loop repeats on the grid

### Problem

With `DELAY 1/16` (default) every LOOP reply was planned like an ANSWER: it started one sixteenth after
the window downbeat, and a note on the last step was cut (or, as the only note, moved one step earlier).
In LOOP the partner replays the same phrase window after window, so the loop sounded a sixteenth late for
as long as it ran, and its last-step note was missing on every pass.

### Decision

`DELAY` protects one situation only: the person may still be playing on the downbeat right after the
question, and the safety sixteenth lets them be heard first. That is true for the **first reply after a
new loop phrase** (the window where the human notes were). It is not true for a replay: that window had
no (or too few, see 2) human notes.

- `CallResponseListener::loopCycle()`: 0 = the window just became the loop; n ≥ 1 = n-th replay. Reset on
  every new loop phrase, `stopClock` (BARS change, epoch, PLAY start/stop, free-clock stop, OFF) and on
  leaving LOOP.
- `replyDelaySteps()` = 0 for a LOOP replay, else PARTNER `DELAY`. The main loop
  (`serviceCallResponseLog`) and `answerPhrase` plan with it, and the harmony lookahead starts at
  `window + replyDelaySteps()`, so the strong-beat chords of a replay sit on the real downbeats.
- Result with `DELAY 1/16`: first reply at window + 1 (last step cut, as before); every replay at
  window + 0 with all notes, including the last step (`step 31, len 1` for 2 bars).
- `DELAY 0` already planned every LOOP reply with delay 0: unchanged.

### Log

No new lines. The existing line after the reply starts gets two fields:

```
[CR-A] strategy=0 evolve=1 hold=0 loop=0 skip=0    <- new loop phrase, at=+1
[CR-A] strategy=0 evolve=1 hold=0 loop=1 skip=0    <- replay 1, at=+0
[CR-A] strategy=0 evolve=1 hold=0 loop=2 skip=1    <- replay 2, the person played 1 note (kept)
```

`loop=` is the cycle (0 also in ANSWER/DUET), `skip=` the human notes of that window that did not replace
the loop. The `[CR-A] bar= at=+…` line already shows the delay actually used. The per-replay `[CR]` and
`[CR-A]` lines existed before (unchanged in number).

## 2. REPLACE threshold

PERFORM > INPUT row 15 ` REPLACE` (under `EVOLVE`), values `3+` (default) → `HALF` → `ANY` → `3+`
(`</>` and ENTER, like the other PARTNER rows). `N/A` outside LOOP. Hint (37 chars):
`</> NEW LOOP NEEDS: ANY/3+ NOTES/HALF`. Session only, not saved (like all PARTNER settings).

| Value | A closed window replaces the saved loop when it has … |
|---|---|
| `ANY` | ≥ 1 note (phase 4 behaviour) |
| `3+` | ≥ 3 notes |
| `HALF` | ≥ ceil(loop notes / 2), at least 1 (loop of 6 → 3, of 5 → 3, of 3 → 2, of 1–2 → 1) |

- No loop saved yet (first phrase after LOOP on, BARS change, epoch): **any** window with notes becomes
  the loop, whatever the setting — otherwise a short riff could never start a loop.
- A window below the threshold: the loop is kept and replayed (cycle +1, delay 0, like an empty window).
  The human notes are captured as usual; `service(..., &heard)` hands them to the main loop, which prints
  the `[CR]` line from them (what the person played, not the loop) and `skip=<n>`. They never become the
  loop and are not used to plan the reply.
- HOLD still ignores input completely; REPLACE applies after HOLD OFF from the next complete window.

### Default: 3+

- Musically: one stray or test note (or a two-note fill over the loop) should not wipe out a 2- or
  4-bar loop; a deliberate new phrase almost always has 3+ notes. `ANY` made the loop fragile in exactly
  the situation LOOP is for — playing over it.
- `HALF` scales with the loop, but against a dense loop (16–32 notes in 4 bars) it demands 8–16 notes, so
  a simpler new idea cannot replace a busy one; against a 1–2-note loop it is `ANY`. It stays an option.
- Cost of `3+`: a loop can only be replaced by a phrase of 3+ notes. To change a 1–2-note loop to
  another 1–2-note loop use `HALF`/`ANY`, or PARTNER off/on (or a BARS change), which clears the loop.
- ANSWER is not affected (checked in code and test): the threshold is evaluated only inside
  `mode_ == Mode::Loop`; ANSWER/DUET get the question window exactly as before and plan with PARTNER
  `DELAY` (`loopRepeat()` is false outside LOOP).

## Code

- `src/input/call_response_listener.h` — `loopCycle_`, `loopRepeat()`, `replyDelaySteps()`,
  `LoopReplace` (`setLoopReplace` / `cycleLoopReplace` / `loopReplaceName` / `loopReplaceMinNotes`),
  optional `service(..., Phrase* heard)`; `answerPhrase` uses `replyDelaySteps()`.
- `GroovePuter.ino` — `serviceCallResponseLog`: `heard` (136 B on the loop stack), `[CR]` from the heard
  window when the loop was kept, `replyDelaySteps()` for the lookahead and the plan, `loop= skip=`.
- `src/ui/pages/perform_page.cpp` — row 15 `REPLACE`, Input rows 15 → 16, hint.
- `src/ui/pages/perform_page.h` — view-continuity max row for INPUT 12 → 15 (it was not updated for
  HOLD/EVOLVE: a remembered row 13/14 was restored as 0).
- `tests/test_cr_partner_settings.cpp` — the HOLD test plays one-note loops; it pins `REPLACE ANY`.
- `tests/test_cr_loop_grid.cpp` (`cr_loop_grid`, in `tests/run_smart_play_tests.sh`).

Untouched: `ResponsePlayer` (start/cancel/NoteOff), `UsbMidiOutput`, the answer lane, `src/audio`, the
dispatcher. No allocation, no locks, no file system.

**Touches the reply, not the cancel:** a replay is started with the same `startAnswer` → `responder.start`
(which cancels a still-running reply first, as before). What changed is only the plan of a replay (delay 0
instead of 1, so the last note is longer by one step and the first starts on the downbeat). The previous
reply's last NoteOff and the replay's first NoteOn now fall on the same step; the responder services the
due NoteOff before the window closes, so a same-pitch NoteOff comes before the NoteOn (tested).

## What is checked (host)

`cr_loop_grid`:
- grid: DELAY 1, EVOLVE 0..3, notes on 0/8/31: first reply `delay=1`, NoteOns at 33/41, onset 31 cut;
  two replays in a row `delay=0`, `loop=1,2`, NoteOns exactly at 64/72/95 and 96/104/127 (EVOLVE 3:
  first and last fixed), the step-31 note present with length 1; DELAY 0 unchanged;
- threshold 3+ (default): 1 and 2 notes keep the 4-note loop (`skip=1/2`, heard notes and onsets
  reported, replay at delay 0), 3 notes replace it (`loop=0`, delay 1) and the new loop repeats;
  first loop from one note; HALF against 6 → 3 → 2 → 1-note loops with `loopReplaceMinNotes` table;
  ANY as phase 4; cycle order and clamp;
- ANSWER and DUET × DELAY 0/1 × ANY/3+/HALF: identical events and times, `loop=0`, plan delay = DELAY;
- balance/cancel: OFF and BARS change in the middle of a sounding replay (one NoteOff at once, cycle and
  loop cleared), same pitch on last/first step over 6 windows, a new 3-note phrase over the loop — all
  NoteOn/NoteOff balanced. The existing `testLoopReplaysWithoutHumanInput` (`cr_cancel`) still passes.
- Mutation check: with `replyDelaySteps()` returning DELAY, or with the threshold forced to 1, the grid
  and 3+ tests fail.

Golden harness (phase 4, host only, not committed; now planning with `replyDelaySteps()`), 144 random
sessions vs. the phase-4 dump: ANSWER and DUET (BARS 1/2, DELAY 0/1) **byte-identical** with every
REPLACE value; LOOP DELAY 0 with ANY byte-identical; LOOP DELAY 1 differs only on replays (`at=+0`, events
one step earlier, last note one step longer); LOOP with 3+ differs where 1–2-note windows no longer
replace the loop (intended).

Full `bash tests/run_smart_play_tests.sh`: all targets pass (phases 0–4, PERFORM, `cr_loop_grid`).
`run_perform_page_arrows_tests.sh` PASS; perform/UI source regressions as before (two scripts fail
on unrelated asserts, identical failures before this change).

`bash scripts/build_cardputer_dynbuffers.sh`: PASS, `Cardputer DRAM globals` **185796 → 185796 B (+0 B)**
(`loopCycle_` and `loopReplace_` fit into existing padding of the listener; sizeof unchanged at 1072).

## Not verified (only by ear / on hardware)

- Not hardware-tested, not listened to: whether the loop now feels locked to the grid; whether 3+ is the
  right default while improvising over a loop.
- "On the grid" means planned on the window downbeat. The replay is started when the main loop sees the
  window closed, so its first NoteOn is up to one loop pass late (as `DELAY 0` in ANSWER already was);
  later notes are scheduled from the window origin. A loop stall longer than a note drops that note
  (`staleDrops`), as before.
- SDL: the row renders (compiles), nothing else wired, as in phases 0–4.
