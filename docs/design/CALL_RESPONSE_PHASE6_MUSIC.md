# CALL / RESPONSE partner — phase 6 (music: answer dynamics, cadence ending)

Status: implemented on `genre/cell-source` (on top of 6dec977b + the uncommitted phases 4 and 5),
host-tested, Cardputer builds PASS, **not hardware-tested, not listened to**. Two items from the partner
review of CALL/RESPONSE as a practice tool.

## 1. Every answer note keeps its velocity

### Before

Every answer note got one velocity: `answerVelocityFor(CALL mean)`. A CALL `127 80 80 80` (accent on the
downbeat) came back as `96 96 96 96` — the accent, the most audible part of the phrasing, was gone.
LOOP `EVOLVE 0` already replayed the original per-note velocities.

### Now

`echoLine()` copies each CALL note's velocity into the plan; `placeInRange()` scales every note by the
answer level: `velocity_out = answerVelocityFor(velocity_in)` = `round(v × kAnswerVelocityPercent / 100)`,
clamped to 1..127. A ratio (not an offset) keeps every note's position relative to the mean — the accent
stays above it, the rest below — and the answer mean is the level × the CALL mean. With the level ≤ 100 %
the result is always 1..127 (a velocity-0 input becomes 1). ShortReply / Develop / Minimal keep the
velocity of the notes they keep; RunUp / Late change pitch / position, not velocity.

Memory: none. `ResponseNote::velocity` and `CapturedNote::velocity` already existed; no compression needed.

### Answer level: 80 % (PHASE0/PHASE1 docs) vs 100 % (code) — decision: **keep 100 %**

`kAnswerVelocityPercent` has been 100 since the first commit (6dec977b) with the comment "match the
average input velocity"; only the phase-0/1 documents say 80 %. Kept at 100 because:

- every listening session and hardware run so far happened at 100 %; switching the level now would be a
  blind perceptual change, not a fix;
- LOOP `EVOLVE 0` replays the raw velocities (spec: exact repeat). At 80 % for `EVOLVE 1..3`, switching
  EVOLVE would jump the loop level by 20 %;
- as a practice tool, hearing the own dynamics back unchanged (now including accents) is the point;
  "sit under the question" is better done on the receiving instrument — the phase-3 decision not to add
  an answer level said the same for an explicit answer channel.

Changing it is one constant (`call_response_motif.h`) and the `answerVelocityFor` asserts in
`test_cr_motif_descriptor.cpp`; the per-note rule stays the same. The PHASE1 table row now points here.

## 2. Ending: chord under it and place in the form

### Before

The answer's last note was the tonic after a CALL that ended away from the tonic, else the fifth — by the
key only. Over V this put the tonic on G B D (C major): unresolved, a clash, every time the answer ended
in a V bar. (Phase-2 counted it: only the ending could miss the chord.)

### Now — `AnswerCadence` (`call_response_harmony.h`, `cadenceEndingNote`)

| Answer | Chord under the ending known and sharing a tone with the key | No chord / chord with no key tone |
|---|---|---|
| 1st, 3rd, … (`Half`) | the key's fifth if the chord has it; else the chord tone in the key nearest the fifth, **excluding the tonic** (tonic only if it is the chord's sole key tone) | phase-1 rule (unchanged) |
| 2nd, 4th, … (`Full`) | the tonic if the chord has it; else the chord tone in the key nearest the tonic (tie goes down) | phase-1 rule (unchanged) |

- "Chord under the ending" = `harmony.chordAtStep(step of the last note)`, after the genre shaping, so a
  `LATE` ending uses the chord of the beat it actually sounds on.
- Octave: the target is the nearest tonic/fifth to the CALL's last note (as before); the chord-tone
  fallback is the nearest to that target; `placeInRange()` folds by octaves (24..71) — the pitch class
  stays, so the note stays in the key and in the chord.
- `RunUp` approaches the new ending by step from below, as before.

Examples (host test `testFinalExamples`):

| Key / chord under the ending | before | odd (1st) | even (2nd) |
|---|---|---|---|
| C major, CALL ends on D over **V** (G B D) | C (tonic over V) | G | B (no C in V; B is 1 below, D 2 above) |
| C major, ending over **I** | C | G | C |
| C major, ending over **IV** (F A C) | C | F (G not in IV; F/A tie → down; C excluded) | C |
| A minor, ending over **v** (E G B) | A (tonic over v) | E | G (A absent; G/B tie → down) |

### Answer number

- `CallResponseListener::answerCadence()`: LOOP uses the existing `loopCycle_` (cycle 0 = the reply to a
  new question = 1st answer → Half; replay 1 → Full; …). ANSWER / DUET use a new `answersStarted_`
  (uint8, +1 per answer actually started, reset by `stopClock` — BARS, PLAY start/stop, epoch, free-clock
  stop, OFF — and by any mode change). The uint8 wrap keeps the alternation (256 is even).
- `planPerformanceReply(…, AnswerCadence cadence = ByCall)` and `planHarmonicEcho(…, cadence = ByCall)`:
  defaults keep the old rule for every existing caller/test; `GroovePuter.ino` and
  `CallResponseListener::answerPhrase` pass `listener.answerCadence()`.
- `LOOP EVOLVE 0` ignores the cadence (exact repeat).
- Log: `[CR-A] … endct=1 fin=H` — `C` = phase-1 rule (no chord), `H` half, `F` full.

### Why the chord-less case keeps the phase-1 rule

Without a chord there is nothing for the tonic to clash with, and the phase-1 rule answers the person's
own question (open CALL → resolve, closed CALL → open). Applying the parity there would change every
chord-less ANSWER (a CALL ending on D would get G on the 1st answer instead of C) without a musical
reason we can check. So: no chord → byte-identical endings to phase 5.

### What changes in the output

- With harmony (a project with Smart Play harmony / pattern harmony — the normal PLAY case): the last note
  of ANSWER, DUET and LOOP `EVOLVE 1..3` follows the table above. Odd answers that used to end on the tonic
  now end on the fifth/chord tone; even answers over a chord without the tonic end on its nearest key tone.
  In LOOP the ending now alternates between replays (half / full), so a 2-bar loop is heard as a 4-bar
  antecedent–consequent pair.
- Velocity: every mode except `EVOLVE 0` (already per note) now carries per-note dynamics. A CALL with
  equal velocities gives exactly the old answer.
- Unchanged: rhythm, onsets, lengths, register rule, strong-beat chord tones, genre shaping, timing,
  NoteOff/cancel path, `EVOLVE 0`.

## Tests

`tests/test_cr_phase6_music.cpp` (`bash tests/run_smart_play_tests.sh cr_phase6`, in the default set):

- accent `127/80` survives (answer `127/80`, mean = level × CALL mean, accent above / rest below the mean);
- 600 random CALLs incl. velocity 0/1/127 × ANSWER, DUET, LOOP EVOLVE 0..3 × delay 0/1: 43 680 answer
  notes, all 1..127 and each equal to a CALL note's velocity at the level;
- ShortReply / Develop keep each kept note's velocity; EVOLVE 0 exact (pitch, step, velocity) for any
  cadence and harmony;
- I–IV–V–I (C major) and i–VI–VII–v (A minor), 1500 random CALLs × 17 styles × answers 1..4 (102 000
  endings each): ending always a chord tone of its step, in the key, 24..71; pitch class as specified;
  **0 tonic endings over V / v** (9 156 and 38 810 endings over the dominant per cadence); even answer
  over I/i = tonic; odd answer = fifth whenever the chord has it, never the tonic;
- chord with no key tone → phase-1 rule (byte-identical);
- no chord: 9 600 ANSWER/DUET/LOOP plans with Half/Full identical to the old rule; a flat-velocity CALL
  gives the old flat velocity;
- listener: ANSWER H F H, reset on mode change; LOOP cycles 0..4 alternate H F H F H.

Updated on purpose: `test_cr_response_planner.cpp` `checkInvariants` asserted that every answer note has
`answerVelocityFor(CALL mean)` (flat velocity); it now asserts per note `answerVelocityFor(note velocity)`.
No test pinned "tonic over V" — the old ending stays tested through the `ByCall` default.

Full `bash tests/run_smart_play_tests.sh`: PASS (all 31 targets).

## Build

`bash scripts/build_cardputer_dynbuffers.sh` PASS: DRAM globals **185796** bytes (before 185796, **+0**;
budget 191488). `bash scripts/build_seqtrak_midi_only.sh`: **186236** (before 186236, **+0**). The new
`answersStarted_` byte fits existing padding of `CallResponseListener`; the rest is stack/code only.

## Not verified (by ear / on hardware)

- Whether the half cadence on odd answers sounds like a question and the even one like an answer in
  real playing — especially in LOOP, where every replay now alternates its last note.
- Even answers over V ending on the leading tone (B in C) — chord-correct, but it pulls to the tonic; it
  may feel less "final" than intended. If so: prefer the chord's root/fifth over the nearest tone.
- Answer level 100 % vs 80 % on the DX/SEQTRAK route at the same channel as the person.
- Accents on SEQTRAK/receiver velocity curves (a flat curve would hide the change).
