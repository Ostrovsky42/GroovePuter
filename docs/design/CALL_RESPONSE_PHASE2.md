# CALL / RESPONSE partner — phase 2 (next-bar harmony, genre ending)

Status: implemented on `genre/cell-source`, host-tested, Cardputer build PASS, **not hardware-tested,
not listened to**. Builds on phases 0 and 1 (`CALL_RESPONSE_PHASE0.md`, `CALL_RESPONSE_PHASE1.md`):
capture, clock, delay, voice, velocity, cancel and the `[CR]` log are unchanged.

## What phase 2 changes

The ECHO_VARIANT answer (same rhythm, CALL pitches in the key, changed ending) now:

1. puts its **strong-beat notes on chord tones** of the harmony that will sound at that beat;
2. shapes its **ending by genre** using the existing idiom dictionary;
3. logs both on the `[CR-A]` line:

```
[CR-A] bar=3 at=+1 n=5 cut=0 notes=0:60:80:3,3:62:80:1,... chord=A8C0 end=LATE ct=4/4 endct=1
```

`chord=A<n>C<m>` — strong beats whose chord came from a lookahead snapshot (A) / from the current
PERFORM harmony (C); `A0C0` = no harmony (phase-1 answer). `end` — `RESOLVE / RUNUP / LATE / MINIMAL`,
`P1` without a genre. `ct` — strong-beat answer notes on a chord tone / strong-beat notes with a known
chord. `endct` — the last note is a chord tone (1), is not (0), no chord known (2).

PERFORM output without the partner (and the partner in LISTEN/OFF) is unchanged.

## Decisions

| Question | Decision | Why |
|---|---|---|
| Which chord | **The chord of the step where the note sounds**, per strong beat: answer steps 0, 4, …, 28 (8 beats, two bars). Beats in the second answer bar are looked up at `absoluteStep + 16` and later (`performance_harmony_lookahead.h`) | The answer is two bars long; a single "current" or "next-bar" chord would be wrong for half of it, and half-bar harmonic rhythm (House) changes the chord inside a bar. When the CALL closes, playback is at the first answer bar; the second answer bar *is* the next bar (+16). One function covers both |
| Fallback | lookahead snapshot → current PERFORM harmony (`PerformanceContext.harmonyPitchClasses`, what Smart Play sounds in now) → none (beat keeps its phase-1 pitch) — `chooseAnswerChord()` | as specified. With PLAY stopped (free clock) there is no playback position and the bridge has cleared the context: the answer is exactly phase 1 |
| How the lookahead is built | The published position is shifted (`shiftHarmonyPosition`) to the target step: same row, or row+1/row+2 with that bar's material taken from `origin.bars[]`, then the **existing** `makeLegacy/Graph/PatternHarmonySnapshot` is called. So the lookahead equals the snapshot the bridge will build when playback gets there (tested) | no second harmony model |
| No file system | Only in-memory data: origin/recipe, `tryCaptureSongHarmonicParticipants` (in-memory Song table, already used every loop by the bridge), and for Graph harmony `PerformanceHarmonyRouteCache::stateFor()` (no `shouldLookup`, never `loadSongHarmonicState`). Cache miss → no Graph lookahead → fallback | owner rule |
| Song-row boundary | A later row is used only if (a) it is still inside the generated phrase (`recipe.songStart + bars`), (b) it is reachable by plain +1 steps — not in reverse, not past the loop end in loop mode, not past the Song end (`lastSequentialRow`), (c) the in-memory Song row still plays that bar's material and version (participants), (d) at most 2 rows ahead and never behind the published row. Otherwise the beat falls back | the engine's `nextSongRowFrom_` (wrap, reverse, loop) and pause/rehearsal rows are private; refusing anything non-sequential is the safe subset. A published position that still lags in the previous bar (block granularity) is handled by the signed offset |
| Pattern mode | `patternHarmonyAhead`: the same generated pattern at a later absolute step (the pattern loops; a queued pattern change already makes the capture fail) | |
| Chord tones on strong beats | Every non-final answer note on a strong beat (answer step % 4 == 0) whose chord is known is moved to the **nearest** pitch class in `chord ∩ key` (tie down). If `chord ∩ key` is empty (chromatic chord) the note keeps its key tone | the answer must stay in the key (tests); the nearest move keeps the CALL contour (≤ 2–3 semitones in practice) |
| Ending vs chord | The ending pitch class **always** obeys the phase-1 rule (open CALL → tonic, tonic CALL → fifth / b5). If that tone is in the chord, the ending is a chord tone; if not (e.g. tonic over a V chord), the rule wins and `endct=0` is logged | "chord-tone ending only if it does not break the phase-1 Ending rule" — breaking it would lose the question→answer sense that phase 1 is built on |
| ≥ 75 % chord tones | Holds by construction for non-final strong beats (100 % when `chord ∩ key` ≠ ∅); only the ending can miss. Test corpus (2000 random CALLs over I–IV–V–I, half-bar chords): **3876/3913 = 99.1 %**; per answer with ≥ 4 strong notes ≥ 75 % asserted | |
| Genre → strategy | `pickResponseStyle(genre, seed)`: one `GenreIdiom::Idea` drawn by `GenreIdiom::detail::ideaWeight(genre, NORMAL, idea)`; the Ending is mapped like `ideaPlanFor` (DelayedAnswer → Late, Turnaround → RunUp, ReducedMotif → Minimal, but Tonic for Acid, whose running line is never cut to one note; all others → Tonic = "resolve by the phase-1 rule"); the shape is `interactionShapeForIdea`. The generator is not called. `responseEndingWeights()` exposes the per-Ending weights (Acid 19/4/1/0, UKG 24/4/8/4, LoFi 22/4/8/6 for RESOLVE/RUNUP/LATE/MINIMAL) | reuse of the dictionary, no new table. The dictionary's Fifth is a secondary touch; in the partner the CALL decides tonic vs fifth, so Fifth has weight 0 |
| What each ending does | RESOLVE: phase-1 answer. LATE (DelayedResponse): the last note comes 2 sixteenths later (if it fits the window), a short silence before it. MINIMAL (ShortCall): the second answer bar keeps only its strong beats and the ending. RUNUP (Turnaround): the off-beat note before the ending becomes the scale tone right under it (left alone when that note is on a strong beat — it belongs to the chord) | rhythm/approach changes only; the ending pitch rule is untouched in all four ("always an admissible ending", tested) |
| No genre | `kNoGenre` (0xFF) or any value ≥ 16 → weights all 0, style inactive → phase-1 answer. With no harmony as well, `planHarmonicEcho` == `planEchoVariant` (tested on 500 random CALLs). On the device the project genre (`genreManager().generativeMode()`) is always set, so genre styles are always on | |
| Determinism | `responseSeed(phrase)` = hash of the CALL onset mask, its pitches and its window bar; same CALL + same window + same genre/harmony → same answer (tested). The same CALL in another window may get another strategy | variety without randomness |
| Liveliness | fixed NORMAL | the partner has no CALM/LIVELY control; a LIVELY link can come later |
| Register (“48..72 relative to the last CALL note”) | **Unchanged from phase 1**: the answer keeps the CALL register (whole line moved by octaves only if it leaves 24..71), the ending is the nearest tonic/fifth to the CALL's last note. Read as: the reply stays within about an octave around where the question ended — the ending is ≤ 6 semitones from the CALL's last note, every other note stays in the CALL's own span. Phase 2 does not move the register: chord-tone moves are nearest-tone (no octave jumps), genre endings change rhythm only. Measured (1000 CALLs × 16 genres): no answer note more than **7** semitones outside the CALL range (RunUp: a step under an ending 6 below), ending ≤ **6** from the CALL's last note — both asserted | no genre or harmony reason found to move the register; a fixed 48..72 window would move low CALLs (bass players) up to an octave and lose "my phrase came back" |
| Answer MIDI channel (owner, mid-phase) | `CallResponseListener::setAnswerChannel(0..16)` / `cycleAnswerChannel()`, 0 = follow PERFORM DX (default, channel 10 by default), not persisted, tested. **Not applied to the output and no UI row yet** | `UsbMidiOutput` routes by (source, target) lanes; `MusicalEvent::channel` is a logical lane index (drums), and generated-note ownership is per target, not per channel. Honouring a free channel needs a new generated lane (ownership bitmap, NoteOff/panic paths) in `usb_midi_output.cpp`, which is in the dispatcher and carries other tasks' uncommitted edits. A UI row that shows a channel that is not used would lie, so the row is left out until the output change is decided |

## Code

- `src/input/performance_harmony_lookahead.h` — `shiftHarmonyPosition`, `legacyHarmonyAhead`,
  `graphHarmonyAhead`, `patternHarmonyAhead` (pure, over the existing snapshot builders).
- `src/input/call_response_harmony.h` — `ResponseHarmony` (8 chord masks, 18 B, stack only),
  `chooseAnswerChord`, `ResponseStyle`, `responseEndingWeights`, `pickResponseStyle`, `responseSeed`,
  `planHarmonicEcho`, `ResponseHarmonyReport`, `formatHarmonyLog`.
- `src/input/call_response_planner.h` — `planEchoVariant` split into `detail::echoLine`,
  `detail::echoEndingRel`, `detail::placeInRange` (phase-1 output identical; phase-1 tests unchanged).
- `src/input/call_response_listener.h` — `canAnswer()` / `startAnswer()` (the old `answerPhrase()`
  is kept and built from them), answer channel setting.
- `GroovePuter.ino` — `collectCallResponseHarmony()` (main loop, after the CALL closes, only in
  ANSWER mode when the window is free), genre from `genreManager()`, plan + start + log.

Nothing in `src/audio`, nothing in the USB dispatcher, no allocation, no locks, no new globals.
Stack: `ResponseHarmony` 18 B + participants (as in `updatePerformanceHarmonyContext`) + one
`HarmonySnapshot` (≤ 64 B), once per two bars.

DRAM (`Cardputer DRAM globals`): **185308** vs **185292** for the same tree with phase 2 removed
(**+16 B**: listener answer-channel byte + phase-2 code in the ino; not split further). The `[CR-A]` line buffer grew 288 → 320 B (loop stack).

## Tests

`bash tests/run_smart_play_tests.sh cr_lookahead cr_genre cr_planner` (all in the default list):

- `cr_lookahead` — snapshot at +16 from every step of row 10 equals the bridge snapshot at row 11
  (the fixture's two bars have different chords); offset 0 equals the current snapshot; a lagging
  position (step 15, +2) reaches row 11; refused past the phrase, at Song/loop end or reverse
  (`lastSequentialRow`), when the Song row was edited (participants), without participants, behind
  the published row; pattern-mode +16 equals the snapshot one bar later; fallback order
  ahead → current → none.
- `cr_planner` (extended) — no harmony + no genre == phase 1; strong-beat chord-tone share ≥ 75 %
  (99.1 % measured), every non-final strong beat a chord tone; determinism with the same seed; all
  notes in the key and in 24..71, mono line, inside the window, for 3 scales × 3 roots × 16 genres +
  no genre, with a chromatic chord; register bounds above.
- `cr_genre` — genres give different weights (Acid ≠ UKG ≠ House, UKG ≠ LoFi); dictionary leans
  (Acid: Late 1, Minimal 0); Fifth weight 0, Tonic > 0 for all genres; 4000 seeds per genre follow
  the weights ±20 %; every picked ending admissible; unknown genre = phase 1; LATE / MINIMAL /
  RUNUP / RESOLVE shapes; a tonic CALL is answered on the fifth in every style; channel setter.

Regression: whole `tests/run_smart_play_tests.sh` (phases 0 and 1 included) and
`tests/run_hgs_tests.sh idiom-consumption` pass.

## Verified only by code and tests / not verified

- Not hardware-tested, not listened to: whether chord tones on strong beats sound "in the song",
  whether LATE/MINIMAL/RUNUP read as genre gestures or as glitches, the feel of a fifth/tonic ending
  over a chord that does not contain it (`endct=0`).
- Lookahead under PLAY on the device: the alignment of the CALL clock (`projectTransportTimeline`
  steps) with `HarmonyPlaybackPosition.absoluteStep` is read from code (both are
  `transport.absoluteSteps()`), not measured. The `[CR-A] chord=A..C..` field is the evidence to
  check: with a generated Song phrase playing, expect `A8C0` inside the phrase, `A4C4` at its last
  row.
- Graph-harmony lookahead is covered only through the shared `shiftHarmonyPosition` + existing
  `makeGraphHarmonySnapshot`; no Graph fixture test.
- Answer channel: stored and tested, not audible (see the decision above); no UI row.
