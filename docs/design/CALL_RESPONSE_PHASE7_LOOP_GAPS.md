# CALL / RESPONSE partner — phase 7 (LOOP without gaps)

Status: implemented on `genre/cell-source` (on top of 6dec977b + the uncommitted phases 4–6), host-tested,
Cardputer builds PASS, **not hardware-tested, not listened to**. Only PARTNER `LOOP` changes; ANSWER and DUET
are untouched (code paths gated by `mode_ == Mode::Loop` / `loop`).

Complaint: "зачем паузы между loop, теряется логика циклов"; then: micro-pauses, and the rhythm of the passes
sounds different from the question (shifted / stretched).

## Causes

No silent window comes from the clock or the state machine: with a saved loop an empty window is always turned
into a replay (`call_response_listener.h`, `service()`, the `loopValid_` branch), so the "two empty windows stop
the free clock" rule never fires in LOOP, `canAnswer()` is true for LOOP, and REPLACE 3+ windows with 1–2 human
notes replay on the downbeat (tested). The gaps came from four places:

| # | Where | Condition | What was heard |
|---|---|---|---|
| 1 | `call_response_performance_plan.h:38-40` (pre-fix; `chooseReplyStrategy` for `loop && evolve >= 2`) → `call_response_development.h` `transformCall` ShortReply | EVOLVE 2/3: every 3rd pass (Develop → ShortReply → Echo for the same loop), and the **first** pass of a dense loop (≥ 1 note per 2 steps) | ShortReply keeps the first ⌈n/2⌉ notes: **the second half of the window is silent** (a whole bar of a 2-bar loop, 2 s at 120 BPM). The loop's form is lost every third pass. |
| 2 | `call_response_listener.h:110` (pre-fix, phase 5: `loopRepeat() ? 0 : answerDelaySteps_`) | LOOP, DELAY 1/16 (the default), the first pass after a new loop phrase (`loop=0`) | The first pass starts a 1/16 late — a 125 ms rest between the question and the loop — every note shifted, a last-step note cut; the second pass jumps back onto the grid. "Rhythm shifted", "a note lost". |
| 3 | `call_response_performance_plan.h:6-11` (pre-fix; `performanceReplyStyle`: only `evolve == 1 \|\| evolve >= 3`) | EVOLVE 2 with a genre whose idea is LATE or MINIMAL | LATE moves the last note 2 steps later (a one-note loop lost its downbeat); MINIMAL drops the off-beat notes of bar 2+ (holes in bar 2). |
| 4 | `call_response_listener.h` `service()` → `positionAt()` / `startAnswer()` → `microsAtStep()` | LOOP under PLAY (transport clock) | The PLAY position is re-anchored on every audio block with `anchorMicros = now` of the pass that saw it, so the estimate lags by 0 … a few blocks, differently each time. The window close and the next pass's origin moved with it: each loop boundary got a **micro gap or overlap** (model: −10 … +12 ms per boundary), and a pass that closed early cut the previous pass's last note. The free clock has no such jitter. |

Not a cause (checked): NoteOff/NoteOn order at the boundary (the previous pass's tail NoteOff is sent by
`responder_.service()` before the window closes, so it always precedes the next NoteOn; within a pass the
responder handles slots in time order); µs rounding (all note times are `origin + step × sixteenth` with an
integer sixteenth, the free clock's windows are exactly `W × sixteenth` apart); phase-6 cadence alternation
(pitch only, no timing); one main-loop pass of scheduling delay (the first NoteOn waits ≤ one pass; a note is
dropped only when the pass is longer than the note — `staleDrops`, now logged).

## Fix

1. **No SHORT REPLY in LOOP** (`planPerformanceReply`): `ShortReply → Echo`. EVOLVE 2/3 alternate
   DEVELOP ↔ ECHO (dense loop: ECHO first). ANSWER/DUET keep SHORT REPLY.
2. **LOOP is always on the grid** (`replyDelaySteps()` = 0 for every LOOP reply, also `loop=0`). The safety
   1/16 exists so a person playing on the downbeat is heard first and cancels the answer (ANSWER); LOOP never
   cancels on human input. PERFORM INPUT row 12 shows `DELAY N/A` (inactive) in LOOP, hint
   `N/A: LOOP ALWAYS PLAYS ON THE GRID`, `</>` does nothing there; the setting is kept for ANSWER/DUET.
3. **LATE / MINIMAL → TONIC in LOOP for every EVOLVE** (`performanceReplyStyle`; EVOLVE 0 never reaches it).
   RUNUP (pitch only) stays.
4. **Chained grid in LOOP** (`startAnswer`, `closePosition`): a reply for the window right after the previous
   reply starts at `previous downbeat + window × sixteenth` while that agrees with the clock estimate within
   ½ sixteenth; under PLAY the window is also closed by that grid (same band; outside it — tempo change,
   anything unexpected — the PLAY estimate decides, as before). New state: `gridValid_`, `gridWindow_`,
   `gridMicros_`, cleared by `stopClock()` (BARS, PLAY start/stop, epoch, free-clock stop, OFF). The free
   clock gives the same numbers as before (its estimate is exact); the chained close is PLAY-only.

Log: `[CR-A] strategy= evolve= hold= loop= skip= late=<us> stale=<n>` — `late` = how long after the window
downbeat the reply was started (its first NoteOn waits for that), `stale` = answer notes dropped as too late
since boot.

NoteOff / cancel path: **not changed**. `ResponsePlayer` (start/cancel/service/offMicros) is untouched; the fix
changes only the origin passed to `start()` and the moment a window closes. Balance is asserted in every test
run below (every NoteOn has its NoteOff, no double NoteOn, OFF releases a sounding note, and at the same instant
no NoteOff is sent after a NoteOn).

## Tests

`tests/test_cr_loop_gaps.cpp` (`bash tests/run_smart_play_tests.sh cr_loop_gaps`, in the default set). The rig
follows `serviceCallResponseLog()`: describe, harmony per strong beat (C F G C, optional), genre style, reply
memory, cadence, `planPerformanceReply`, `startAnswer`, `rememberStartedReply`. One question window, then six
windows without human input.

- EVOLVE 0..3 × DELAY 0/1 × BARS 1/2 × harmony on/off × no genre + 16 genres × 6 loop shapes (quarters, dense
  eighths, off-beats, three notes, legato, one note) = 3264 runs, 19 584 windows: a reply in every window
  (`loop=0..5`, delay 0); as many notes as the question; every onset and every length (steps) equal to the
  question's (EVOLVE 2/3: a note at most one step later / shorter — DEVELOP and the tail shift are the
  designed variation — the first note always on the downbeat); every bar with a loop note sounds; the longest
  silence over the run ≤ the loop's own longest rest (+1 step at EVOLVE 2/3, + one pass); downbeat of each
  pass exactly one window after the previous one; `staleDrops == 0`; NoteOn/NoteOff balance and order.
- Clocks and passes: FREE and PLAY (audio-block anchors visible 0..2 blocks late) × main-loop pass 1/5/20/60 ms
  × EVOLVE × DELAY × BARS × shapes: 4608 windows, same checks (first NoteOn ≤ one pass late; PLAY absolute
  grid ≤ one pass + 3 blocks, window-to-window exact up to one pass).
- REPLACE 3+: windows with 1 and 2 human notes replay with a NoteOn exactly on the downbeat.
- PLAY tempo change 120 → 132 BPM mid-loop: a reply in every window, no drop, balanced.

Before → after (same test, fixes reverted in a copy): **21 708 failures → 0**. Mutation check (one fix reverted
at a time): delay 13 104, short reply 6 048, LATE/MINIMAL 2 526, grid chaining 788, chained close only 328
failures. Before, at default EVOLVE 1 / DELAY 1: first pass 1 step late on every note, last note cut; EVOLVE 2/3:
`4 notes, question has 8`, `bar 2 silent`, `longest silence 15–18 steps`; PLAY: downbeats 1 990 000 … 2 012 000 µs
apart instead of 2 000 000.

Updated on purpose:
- `tests/test_cr_loop_grid.cpp`: the first LOOP reply is now `delay=0`, NoteOns 32/40/63, onset-31 note kept
  (was `delay=1`, 33/41, cut); the replacing phrase's first reply `delay=0`.
- `tests/test_cr_development.cpp`: LOOP EVOLVE 3 after DEVELOP plays ECHO with all notes (was a 2-note SHORT
  REPLY); the LOOP strategy sequence is DEVELOP, ECHO, DEVELOP (was DEVELOP, SHORT, ECHO).

Full `bash tests/run_smart_play_tests.sh`: PASS (32 targets). `bash tests/run_perform_page_arrows_tests.sh` PASS.

## Build

`bash scripts/build_cardputer_dynbuffers.sh` PASS: DRAM globals **185796** (before 185796, **+0 B**).
`bash scripts/build_seqtrak_midi_only.sh` PASS: **186236** (before 186236, **+0 B**). `sizeof(CallResponseListener)`
1072 → 1080 (+8: `gridWindow_`, `gridMicros_`; `gridValid_` in padding) — the section totals did not move
(alignment slack). No allocation, no locks, no file system, nothing in `src/audio` or the dispatcher.

## Not verified (by ear / on hardware)

- Not listened to. Whether the gap the owner heard was #1 (EVOLVE 2/3), #2 (default DELAY 1/16) or #4 (PLAY):
  check with `[CR-A] … loop= late= stale=` — `strategy=1` no longer appears in LOOP; `late` should stay well under
  one 1/16 (125 ms at 120 BPM); `stale` must not grow.
- The PLAY jitter size is a host model (block 11.6 ms, published 0..2 blocks late). The real audio task may be
  better or worse; the chained grid hides it either way within ±½ sixteenth.
- Under PLAY the LOOP window boundary for human capture follows the chained grid (≤ the first pass's estimate
  error, a few ms): a note right at the boundary may fall into the other window.
- Rhythm at EVOLVE 2/3 still differs from the question by design (DEVELOP / tail shift move a note one 1/16
  later, which leaves a 1/16 rest before it). If that also reads as "micro-pauses", EVOLVE 2/3 should vary pitch
  only — a product decision, not changed here.
- Rhythm that the capture itself changes (quantization to 1/16: a note held 1.4 steps comes back 1 step long,
  swing/feel is not kept) is unchanged.
