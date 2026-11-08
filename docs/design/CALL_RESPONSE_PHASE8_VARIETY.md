# CALL / RESPONSE partner — phase 8 (variety: tonic, genre chords, ending colour, LOOP form)

Status: implemented on `genre/cell-source` on top of a36a9727 (phases 0–7), host-tested, Cardputer builds
PASS, **not hardware-tested, not listened to**. 8-bar answers are out of scope (not done).

## A. The answer's tonic comes from the snapshot (bug fix)

**Before.** `GroovePuter.ino` built the answer key as
`responseKeyFromPitchClasses(context.rootMidi, context.scalePitchClasses)`. `context.rootMidi` is
`60 + snapshot.base.rootPc` (`performance_keyboard.cpp`, `setHarmonySnapshot`) — the root of the chord that
sounds when the CALL closes, not the key's tonic. A CALL closed over iv (Fm in C minor) got the key "F + the
C-minor pitch classes": the phase-6 full cadence and the fifth were counted from F.

**Now.**
- `PerformanceContext::tonicPc` (new, 0xFF = unknown) is filled from `snapshot.tonal.tonicPc`
  (`setHarmonySnapshot`) and from `origin.common.rootPitchClass` (`performanceContextAtGeneratedPhrase`). It
  sits in the struct's tail padding: `sizeof(PerformanceContext)` stays 24.
- `responseKeyFromHarmony(tonicPc, chordRootMidi, scalePcs)` (`call_response_planner.h`): the tonic when
  known, else the old chord-root source. The ino uses it.
- `context.rootMidi` keeps its meaning (Smart Play is chord-relative and is not changed).

Test: Fm7 published through `makePatternHarmonySnapshot` + `PerformanceKeyboard::setHarmonySnapshot` →
key root **C** (was F, asserted), same pitch classes; full cadence on **C** (was F, asserted).

## B. The genre template's chord (main lever)

**Before.** 13 genres write bass and lead from a genre template (`GenreIdiom::apply`, harmonic authority
`Idiom`). Every snapshot builder returns `Unsupported` for `Idiom`, so under the default `Legacy` strategy the
partner had no chord at all in those projects (no lookahead, no current harmony) — phase 2/6 were inert there.

**Now (full path, all 13 genres).**
- `GenreIdiom::idiomChordAt(variant, level, root, scale, barOrdinal, phraseBars, barCount, step)`
  (`genre_idiom.h`): the chord of a step = the **last stab at or before the step**; before the bar's first stab
  = the **previous bar's last stab** (bar 0 wraps to the phrase's last bar; a single looping Pattern bar to
  itself). Pitch classes go through the generator's own mapping (`transposeDegrees` by the bar's degree shift,
  then `mapSemi` to the project scale, then the project root), and return the stab's mapped root too. The bar
  degree shift is now one function (`barDegreeShift`) used by both `apply()` and the reader, so they cannot
  drift apart.
- Templates without stabs (Outrun, Darksynth, Electro, Chip, DnB dance) get an **implied triad**: a minor
  triad on the template tonic moved by the bar's degree shift = the diatonic triad of the bar's degree.
- The origin names the template: `GenreIdiom::packIdiomTemplate(variantIndex, level)` — one byte (5 bits
  variant index in `kIdiomVariants[]`, 2 bits level, 0xFF = none). Path: `StrongRhythmMigrationResult::
  idiomTemplate` (stack) → `MaterializedSynthABarEvidence::idiomTemplate` (stack) →
  `GeneratedSynthAOriginCommon::idiomTemplate` (Song phrase, 0xFF when bars disagree) and
  `GeneratedPatternHarmonyOrigin::idiomTemplate` (Pattern mode, tail padding). The variant is re-derived with
  `variantForRequest()` — the same selection `apply()` makes (salt → variant for Acid/Techno/UKG).
- `idiomChordAhead()` / `idiomPatternChordAhead()` (`performance_harmony_lookahead.h`): same position rules as
  the Legacy lookahead (`shiftHarmonyPosition`: participants, Song/loop end, ≤ 2/4 rows; published row must
  play the phrase's own material/version). Used only by `collectCallResponseHarmony` in the ino.
- No file system, no allocation, no generator call; template tables are `constexpr` (flash).
- **Not changed:** Smart Play / the PERFORM bridge still gets no harmony in Idiom projects (the partner's
  "current harmony" fallback is still empty there; the lookahead covers the answer beats inside the phrase).
  Changing Smart Play is a separate product decision.

Why one byte, not two: two separate bytes in `GeneratedPatternHarmonyOrigin` triggered a host GCC 13 ICE
(`gimple_add_tmp_var`, -O1) on `GeneratedPatternHarmonyOrigin{}` in `miniacid_engine.h`; one packed byte does
not, and costs less.

Chord roots: `ResponseHarmony::chordPcs[]` now carries the chord root in the unused top nibble (bits 12..15 =
root+1), so the struct (loop stack) did not grow; `chordAtStep()` masks it off, `rootAtStep()` reads it.
Roots come from the snapshot (`base.rootPc`), the template stab, or `context.rootMidi` for the current chord.

## C. Genre table for the final tone (`EndingColor`, `cadenceEndingNote`)

Constant table (`endingColorForGenre`, flash, 0 B DRAM). Applies only when the chord root under the ending is
known (snapshot / template / current chord) — otherwise phase 6 byte-identical (tested).

| Genre | Colour | Half cadence (odd) | Full cadence (even) when the chord has no tonic |
|---|---|---|---|
| Acid, Techno | Plain | key's fifth if in the chord and not its 7th/9th, else the chord's root/fifth nearest to it | chord's root/fifth nearest the tonic (over V in C: **D**, not the leading tone B) |
| House, LoFi, Dub (= Reggae, no "Dub" genre exists) | Extended | the chord's 7th (b7/7) or 9th when it has one (nearest the key's fifth), else phase 6 | root/fifth/7th/9th nearest the tonic |
| UK garage | Blue | the chord's b3 or b7 when it has one, else phase 6 | b3/b7 nearest the tonic, else root/fifth |
| all others / no genre | Default | phase 6 | phase 6 |

Kept from phase 6 for every colour: the full cadence lands on the tonic whenever the chord has it; the half
cadence never lands on the tonic; the ending is always a chord tone in the key. Decision: the colour changes
*which open tone*, not the form — the H/F alternation is what makes the loop read as question/answer.

Note: Techno's templates are dub-techno stabs (m9), but the spec puts Techno in Plain; followed the spec. Over
Techno m9 stabs the half cadence therefore avoids the 9th/7th.

## D. LOOP passes (`call_response_performance_plan.h`)

**Before.** The loop was an A A′ pair: EVOLVE 2/3 alternated DEVELOP/ECHO by reply memory (period 2) and the
cadence alternated H/F (period 2), in phase — DEVELOP always ended open, ECHO always closed.

**Now** (`planPerformanceReply(…, cadence, loopCycle)`; ino and listener pass `loopCycle_`):

| pass (`loopCycle % 4`) | content | cadence (phase-6 parity, unchanged) |
|---|---|---|
| 0 | the loop | Half |
| 1 | the loop | Full |
| 2 | the motif **one scale degree up** (transposed *before* `planHarmonicEcho`, so strong beats snap to the chords of their beats) | Half |
| 3 | back to the loop | Full |

This is a period a a′ b a′: the lift is on an antecedent (Half), so it still resolves on pass 3. The PHASE6
alternation holds unchanged (asserted for cycles 0..11) because the form only transposes; it never picks the
cadence.

- EVOLVE ≥ 1 gets the form (EVOLVE 1 is the default, so the change is audible by default). EVOLVE 0 stays an
  exact repeat.
- EVOLVE 2/3: strategy by `loopCycle % 3` = ECHO, DEVELOP, ECHO (pass 0 is always the loop as played);
  against the cadence `% 2` → DEVELOP meets both H and F; strategy × cadence period 6, with the form 12. The
  reply memory no longer drives LOOP (ANSWER still uses it; SHORT REPLY stays banned in LOOP).
- EVOLVE 3, passes 1 and 2 only: seeded variation (seed = `callSignature` + `loopCycle`): weak-beat notes (not
  the first note, not strong beats, not the ending) move one scale step with p = 1/4 (staying in 24..71) and
  their velocity moves by up to ±8. Passes 0 and 3 are untouched, so the loop is always recognisable.
- Rhythm, onsets, lengths, timing, grid chaining: unchanged (the phase-7 rig now passes `loopCycle` and stays
  green: 19 584 + 4 608 windows).

ANSWER / DUET: `loopCycle` is ignored for them (asserted: plans identical for cycles 0..11). Shared-code change
that reaches them: B (template chords) and C (ending colour), which apply to all modes by design.

## Memory

| build | before | after | Δ |
|---|---|---|---|
| `build_seqtrak_midi_only.sh` DRAM globals | 186236 | **186236** | **+0 B** |
| `build_cardputer_dynbuffers.sh` DRAM globals | 185796 | **185796** | **+0 B** |
| MIDI-only flash (sketch) | 1695182 | 1698730 | +3548 B |

The +1 B in `GeneratedSynthAOriginCommon` landed in the origin's existing alignment slack.

Persistent state: `GeneratedSynthAOriginCommon` +1 B (in the engine's origin); `GeneratedPatternHarmonyOrigin`
+1 B in tail padding; `PerformanceContext` +1 B in tail padding. Loop stack: `ResponseHarmony` unchanged (root
in the top nibble); `collectCallResponseHarmony` +0 arrays (roots packed into the existing `ahead[]`).

## NoteOff / cancel

Not touched. `ResponsePlayer` and the cancel path are unchanged; phase 8 changes only pitches/velocities and
the chosen strategy of a plan before `start()`. Rhythm and lengths in LOOP are as in phase 7, so the stuck-note
issue is not affected by this phase.

## Tests

`tests/test_cr_phase8_variety.cpp` (`bash tests/run_smart_play_tests.sh cr_phase8`, in the default set):
tonic from snapshot; template chord at step / across the bar line / key transposition / stab-less templates;
the read chord equals every Synth B stab onset `apply()` writes (1397/1397, the answer bar's melodic ending from
step 12 excluded); Idiom Song/Pattern lookahead and refusals; answers over template chords in key, 24..71,
deterministic, strong beats on chord tones (100 %); colour table examples; no-root / default genre ==
phase 6 (153 000 endings); form kept with colours (51 000 endings); LOOP periods, 4-pass form, period 4 at
EVOLVE 1 and 12 at EVOLVE 2, EVOLVE 3 variation bounds; ANSWER/DUET ignore the pass; listener passes the cycle.

Updated on purpose: `test_cr_development.cpp` — LOOP EVOLVE 3 strategy sequence is now ECHO, DEVELOP, ECHO,
ECHO, DEVELOP, ECHO by pass (was DEVELOP, ECHO, DEVELOP by memory). `test_cr_loop_gaps.cpp` — the rig passes
`listener.loopCycle()` like the ino (assertions unchanged).

## Listen for (not verified by ear / on hardware)

- House/LoFi half cadences on the 7th/9th (Bb over Cm9): open and "deep", or unresolved?
- UKG b3/b7 endings; Acid/Techno full cadence over V on the 5th of V (D) instead of the leading tone.
- LOOP pass 2 lifted a degree: does it feel like development or like a wrong note? Is a lift every 4 passes
  too regular?
- EVOLVE 3 weak-beat variations: musical or nervous? ±8 velocity may be inaudible on flat receiver curves.
- Template chords in Idiom projects: check `[CR-A] chord=A..` is non-zero now in House/UKG/LoFi Song phrases.
