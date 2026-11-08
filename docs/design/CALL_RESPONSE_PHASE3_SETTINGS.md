# CALL / RESPONSE partner — phase 3 (one-note fix, answer channel, settings)

Status: implemented on `genre/cell-source`, host-tested, Cardputer build PASS, **not hardware-tested,
not listened to**. Builds on phases 0–2 (`CALL_RESPONSE_PHASE0.md` … `PHASE2.md`). With default
settings the partner behaves exactly as in phase 2, except for the bug fix below.

## A. Bug: "one note — the partner answers only once"

### Cause

A one-note CALL whose note is quantized to the **last sixteenth of its window (onset 31)** got no
answer at all:

- the answer starts one sixteenth after the ANSWER window boundary (`kAnswerDelaySteps = 1`), so the
  last usable answer step is 30; phase 1 dropped CALL notes on onset 31 and counted them (`cut=`);
- with one note nothing was left, `planEchoVariant` / `planHarmonicEcho` returned `false`, and the
  CALL was ignored silently (no `[CR-A]` line; the `[CR]` line shows `mask=…_…1`).
  `test_cr_response_planner.cpp` even asserted this (`!planEchoVariant(onlyLast…)`).

A note lands on onset 31 whenever it is played **0.3 … 1.5 steps before a window downbeat** — the
pickup rule moves only the last 30 % of a step into the next window. That is a downbeat played a
little early (40–90 ms at 120 BPM), which is how a one-note call is usually played. On the free clock
the very first note anchors the grid (onset 0 exactly) and is answered; every later one-note call aimed
at a window downbeat and played a touch early is not: **one answer, then silence**. Under PLAY the
same happens on every even-bar downbeat played early.

Checked and ruled out (host simulations of the listener + planner + responder, and of
`PerformanceKeyboard` + listener + `UsbMidiOutput` on a fake wire, single notes at 0.2–5 s reaction
times, PLAY and free clock, 2 and 30 ms loop periods): the "window has notes" condition, the
"person already started the next window" rule, the CALL→ANSWER→LISTEN turn, the reset on ANSWER, the
idle stop of the free clock, ownership in `UsbMidiOutput`. All answered every one-note CALL except
onset 31 (≈ 4 % of random note positions, but ~100 % for early downbeats).

### Fix

`detail::echoLine`: when the delay would leave **no** answer note (exactly the one-note CALL on the last
step), that note is answered on the **last usable step** (30 with delay 1, 14 for 1 bar), length 1.
That is within one step of where a pickup would have put it (window boundary + 31 vs. next window
+ 1 + 0). Multi-note CALLs keep the phase-1 rule (the onset-31 note is cut, the rest answers).

Also hardened (not the cause): when `projectTransportTimeline().trySnapshot()` fails (writer mid-update),
the loop now reuses the previous transport view instead of treating it as "PLAY stopped", which reset
the CALL clock and cancelled a running answer for one loop pass.

### Test

`tests/test_cr_single_note.cpp` (`cr_single_note`): free clock, one note at step 0, then one note
0.5 step before every second window downbeat, silence in between; same under PLAY with genre Acid.
**Before the fix: `free clock: calls=6 answers=1 answer NoteOns=1` (assert fails). After: 6/6, and
4/4 under PLAY.** Plus: every onset 0..31 of a one-note CALL is answered for all 16 genres and none;
multi-note tail is still cut.

## B. Answer MIDI channel

| Question | Decision | Why |
|---|---|---|
| Default | `DX ROUTE` = phase-2 output unchanged (PERFORM DX route, MIDI ch 10 by default, PROJECT > MIDI) | owner decision; zero change for existing setups, CC26 mono/poly handling of the DX route kept |
| Explicit channel 1..16 | events carry the new flag `kMusicalEventWireChannel` and `channel` = wire channel; `UsbMidiOutput` plays them on its own **answer lane** (one `MidiVoiceLane`, outside `lanes_`) | the existing generated path is per *target*, not per channel; a separate lane gives the answer its own ownership, so cancel releases exactly its notes |
| Ownership | the lane uses the shared `owners_` table (`acquireActiveNote` / `releaseActiveNote`) | a channel/note shared with PERFORM or Pattern keeps the other owner sounding (tested) |
| Channel change during an answer | `ResponsePlayer` latches the channel per answer; every NoteOff (also cancel) goes to the NoteOn's channel; a NoteOn on another channel first releases the lane's note on its old channel | balance is per wire channel |
| Failures | a rejected answer NoteOff is retried before the next event of any lane; a Dx panic (also the control-queue overflow fallback for a dropped answer NoteOff) releases the answer lane; unmount clears it | no hanging note |
| USB and DIN | below `UsbMidiOutput` through the existing tee — nothing extra | tested with two fake wires |
| CC26 (SEQTRAK mono/poly) | not sent on an explicit channel | the channel may be any device; CC26 is a SEQTRAK DX-route concern |

Changes in `usb_midi_output.*`: one member `answerLane_`, `handleAnswerLaneEvent`, `releaseAnswerLane`,
calls in `configureLanes` / `releaseTargetAllNotes` (Dx) / `releaseAllActiveNotes` / `clearActiveState`
/ `handleMusicalEvent`. `releasePendingChannelPanics` (A1) is untouched.

Test `tests/test_cr_answer_channel.cpp` (`cr_channel`): channel 5 → NoteOn/NoteOff on wire channel 4 on
USB **and** DIN, none on DX; cancel → NoteOff on channel 5 even after the setting changed to 12, next
answer on 12; default → DX channel exactly as before; shared note with PERFORM keeps the person's note;
failed NoteOff retried; Dx panic releases; listener cycle 0..16 wraps.

## C. PARTNER settings

PERFORMANCE TOOLS > INPUT, rows right under `PARTNER` (row 8):

| Row | Values (default first) | Music problem it solves |
|---|---|---|
| 9 ` BARS` | **2 BARS** / 1 BAR | 2 bars = phases 0–2. 1 bar = quick trading: short riffs and one-note calls are answered after 1 bar + 1/16 (2 s at 120 BPM) instead of 2 bars + 1/16 (4 s), the wait that makes a player give up and play again (which cancels the answer) |
| 10 ` GENRE` | **AUTO <project genre>** / NONE / 16 genres | AUTO = phase 2. NONE = the plain phase-1 ending (no LATE/MINIMAL/RUNUP). A genre = reply in another idiom than the project (e.g. a UK Garage partner over a House project) |
| 11 ` CHANNEL` | **DX ROUTE** / CH1..16 | reply on another instrument than PERFORM DX (see B) |
| 12 ` DELAY` | **1/16** / 0 GRID | 1/16 = phase 1 (safe: a player who keeps playing on the downbeat cancels before a blip). 0 = the answer starts on the bar line, so the echoed rhythm sits on the grid exactly like the call; the cost is a possible one-note blip when the player continues on the downbeat |

`</>` and ENTER cycle each row; values are dim while PARTNER is OFF (still editable). Hints ≤ 40 chars.

### Decisions

- **Rows, not a popup.** The tools layer already scrolls (6 visible rows of 13); four rows in the
  existing row system need no new UI code, are visible next to PARTNER and follow the same `</>`/ENTER
  rules as every other row. A popup (Alt+E style) would hide the settings behind a second gesture and
  add a modal state to PERFORM, which is a live-playing page.
- **Bars: 1 or 2, not 4.** 4 bars would need 32+ capture events (16 now, 192 B of DRAM per 16), a
  64-bit onset mask, 16 strong-beat chords, and harmony lookahead past the current +48 steps / 2 Song
  rows (refused today → bars 3–4 would lose chord tones). That is roughly the whole 300 B budget for a
  length nobody has asked to trade in. 1 bar is the one that fixes a felt problem (latency).
- **Delay: 0 or 1.** 2+ sixteenths solve nothing the 1/16 does not.
- **Not added: answer level (%).** The phase-0 80 % rule keeps the reply under the call; on a different
  channel the receiving instrument's own level is the right control. Can be added later (1 byte).
- **Not persisted** (like PARTNER itself): the partner is a session tool, defaults are safe, and saving
  would touch the project format for four bytes. If owners want it, the place is the project, next to
  the PERFORM state.
- **Changing BARS** is a new epoch: capture and a running answer are dropped (answer note released at
  once). GENRE / DELAY / CHANNEL apply to the next answer.
- **1 bar and the genre MINIMAL ending**: MINIMAL thins the *second* answer bar; with 1 bar there is
  none, so MINIMAL equals RESOLVE there.

`[CR-A]` now ends with ` ch=<0|1..16> bars=<1|2>` (0 = DX route).

Test `tests/test_cr_partner_settings.cpp` (`cr_settings`): defaults = phase 2 (2 bars, delay 1, AUTO,
DX route, answer at step 33); genre AUTO/NONE/explicit resolution and wrap; 1 bar: window 16 steps,
answer at step 17 inside the answer bar, one note on the last step answered at step 14; delay 0: answer
at step 32 and onset 31 not cut; changing BARS mid-answer releases the note and resets the clock.

## Code

- `src/input/call_response_phrase_capture.h` — runtime window (`setWindowBars`, `Phrase::windowSteps`).
- `src/input/call_response_planner.h` — delay parameter, window from the phrase, one-note fix.
- `src/input/call_response_harmony.h` — `planHarmonicEcho(..., delaySteps)`.
- `src/input/call_response_responder.h` — wire channel latched per answer, `kMusicalEventWireChannel`.
- `src/input/call_response_listener.h` — BARS / GENRE / CHANNEL / DELAY settings, `lastTransport()`.
- `src/input/musical_event.h` — `kMusicalEventWireChannel`.
- `src/midi/usb_midi_output.*` — answer lane.
- `GroovePuter.ino` — window/delay/genre from the listener, snapshot hardening, `[CR-A] ch= bars=`.
- `src/ui/pages/perform_page.*` — rows 9–12.

Nothing in `src/audio`, no allocation, no locks, no file system. DRAM (`Cardputer DRAM globals`):
**185316** vs **185308** before phase 3 (**+8 B**).

## Not verified

- Not hardware-tested, not listened to: whether early downbeats really are what the owner played (the
  fix is proven in host tests only); the feel of 1 bar and of delay 0; the answer lane on a real
  receiver over USB and DIN; SEQTRAK behaviour when the answer channel equals a SEQTRAK track channel.
- A Dx panic from PERFORM itself (`x`, target change) also ends a sounding explicit-channel answer note
  (precise NoteOff, no CC123) — harmless, but audible as a cut note.
- SDL build: rows render (compiles), nothing is wired, as in phases 0–2.

## LOOP controls (2026-11-03)

INPUT now includes HOLD and EVOLVE after DELAY. Both are session settings;
controls are inactive outside LOOP. HOLD becomes available after the first
captured phrase. HOLD ON protects that original motif while the player keeps
playing live. HOLD OFF resumes capture at the next complete window; input
from the partial release window is ignored for replacement. BARS changes,
transport epochs, OFF and leaving LOOP clear HOLD and the saved loop.

EVOLVE defaults to 1: 0 repeats captured pitches/rhythm/velocity within the
existing 24..71 range and DELAY window rules; 1 keeps rhythm and changes only
harmonic interpretation/ending; 2 cycles existing variation strategies; 3
adds bounded shifts in the tail while keeping the first two attacks. Every
answer is planned from the saved original. EVOLVE changes affect the next
answer, not sounding notes. X turns PARTNER OFF.

DUET now reacts to the previous completed input window: at least eight notes
per bar selects a short reply (up to ceil(N/2) notes); sparse calls use the
normal response. This is window-based adaptation, not instantaneous ducking.
