# Brief for Codex: livelier melodies from G (GroovePuter 0.9.18)

Please answer in Russian. The owner is not a musician; he judges by ear.

## Where to work

- Repo worktree: `Cardputer/miniacid-0918`, branch `feature/20261008-0918-sound-and-oled-theme`.
- Base: v0.9.17 (`dev_0.9.13` @ `d5503f57`). Prototype under review: commit `99103957` (diff vs `ff732410`).
- Device: M5Stack Cardputer ADV (ESP32-S3, no PSRAM, DRAM-only build: 183 828 / 191 488 bytes used, about 7.6 KB free). Two TB-303 style mono synths (Synth A = bass, Synth B = lead) and a drum machine. A pattern is 16 steps = 1 bar; a step has note, accent, slide, velocity, timing, probability.
- Do not push, do not flash. Propose changes as diffs or a branch off `99103957`.

## What the owner asked

"Make the melodies from G livelier." He listened to the prototype below and said it really is livelier. Now we want a couple of iterations and genre-specific ideas from you.

## Measured facts (v0.9.17, plain G on STEPS, 16 genres x 8 presses x 2 synths, recipe 0)

Probe: `tools/gen_probe/genprobe.cpp` (build with `tools/gen_probe/build.sh`).

| | Synth A (bass) | Synth B (lead) |
|---|---|---|
| notes per bar | ~5 of 16 | ~3 of 16 |
| distinct pitches | 2.8 | 1.2 |
| range | ~5 semitones | under 1 semitone |
| a second G changes the result | slightly (~2 steps of 16) | not at all in 9 of 16 genres |

- One bass rhythm skeleton across genres (`C . . X . . . C . . Y . . . C .`): Acid looks like Chip.
- Slides: 0 in every genre. Accents: under 1 per bar, even in Acid.
- STYLE (FAITHFUL / VARIANT / REWORK = `RealizationLevel` P1/P2/P3) changes nothing in synth notes; it only reaches the rhythm realizer.
- Darksynth lead is empty. HipHop and LoFi lead is one pitch held 16 steps with slides (a drone).
- MATERIAL path (G -> 4-bar TAKE, `tools/gen_probe/matprobe.cpp`): bars differ and the progression moves, but the lead is still 1-3 notes per bar, slides and accents 0. D (develop) is refused in 14 of 16 genres on the default scene: 8 genres "CAN'T GROW" by rule, 6 hit "NO ROOM".

## Root cause (traced in code)

Plain G -> `GroovePuterRhythm::regenerateSynthWithQuantizedCommit` (`src/generation/migration/quantized_generation_commit_impl.h`):

1. The legacy generator `GrooveboxModeManager::generatePattern` (`src/dsp/mode_manager.cpp`) writes bass and lead. Lead: 5-16 notes, 2-8 pitches, range up to 34 semitones (chaotic, but alive).
2. `migrateStrongRhythmMaterial` (`src/generation/migration/strong_rhythm_migration.cpp`) re-seats both on catalog rhythms and re-projects pitches. `tools/gen_probe/migprobe.cpp` prints the lead before and after: it drops to 1-3 notes and 1-2 pitches here.
   - Lead role per genre: `src/generation/composition/generation_profile.cpp` (`CompositionSecondaryRole`: Melodic / Chord / ChordWithMelodicFill) with weighted melodic rhythms (`kMelodicDrive`, `kMelodicBroken`, `kMelodicLoFi`, ...).
   - Melodic rhythm onsets: `src/generation/roles/melodic_motif.cpp` `realizeMelodicMotif`: TWO-NOTE HOOK {2,10}, DELAYED ANSWER {6,14}, PICKUP {12,14,15}, SYNCOPATED MOTIF {1,5,10,14}, REPEATED CELL {0,4,8,12}, SPARSE CALL 1-2 notes. Onsets that coincide with bass or chord onsets are removed (`blocked`).
   - Lead pitches: `src/generation/roles/melodic_pitch_intent.cpp` builds a contour of scale-degree offsets; the allowed contours come from the genre's tonal profile in `src/generation/composition/tonal_profile.cpp`. House, Rave and Techno use `kStaticProfile` (lead: `Static` only, bass: root only). Acid allowed only Static / Neighbor / RepeatThen*.
   - Bass articulation: `bassPolicy()` in `tonal_profile.cpp` gave every genre `Plain`, so no accents or slides. Styles exist (`AccentPulse`, `LegatoApproach`, `Dynamic`, see `src/generation/roles/bass_pitch_behavior.cpp` `applyArticulation`), but a slide needs the previous note held into the next (`isLegatoConnected`), and the bass rhythms leave gaps.
   - The lead gets no accent/slide masks at all in the tonal adapt step (`adaptTonalPlanToSynthPattern(synthB, ..., 0, 0, ...)`).
3. The Atlas corpus (`src/generated/rec_*`, recipes 6-11 only: Acid x2, UK Garage x2, Dub x2) is not used for recipe 0 and always takes variation 0.

## Prototype `99103957` (what the owner liked)

- Two new lead rhythms in `MelodicRhythmId`: RUNNING LINE {0,2,3,6,8,10,11,14}, EIGHTH ARP {0,2,...,14}. A dense line no longer drops bass onsets (it sits two octaves up). Weighted in for Acid, Synthwave (Outrun) and House recipe 0 (`kMelodicDriveDense`, `kMelodicHouseDense`).
- Acid lead may also StepUp / StepDown / LeapReturn / Arch. House leaves `kStaticProfile` for its own profile (moving bass, Drive contours). Acid / Synthwave / House bass get accent articulations.
- Result (lead): Acid 3 -> 8 notes, 1-2 -> 6-8 pitches; Synthwave 2 -> 8 notes, 5-6 pitches; House 1 -> 8 notes, 4-5 pitches.
- Listening page (before/after, 8-bar renders): https://claude.ai/artifact/AwPNiWaV37owWgjJAqvCK7. Render your own with `tools/gen_probe/leadrender.cpp <out-dir> <tag>`.
- Known weak spots: Acid take 2 is a plain scale run `C D D# F G A A# C` (lively but mechanical); Acid bass still has no slides; 13 genres untouched (Rave and Techno still one note).

## What we ask from you

1. **Genre ideas.** For each of the 16 genres (`GenerativeMode` in `src/dsp/genre_manager.h`), give the lead and bass character you would aim for in 1-2 lines: density, typical rhythm cells (as 16-step onset masks), contour habits, register, where accents and slides belong, call/response with the bass. Be concrete and idiomatic (e.g. Acid: 16th lines with slides on step-neighbours and accents on 1-2 off-beats per bar). Mark which genres should stay sparse on purpose (LoFi, Dub, Trip-Hop?).
2. **Two iterations on the prototype.** Pick the most valuable changes and implement them as small diffs on top of `99103957`, measuring with the probes. Candidates we see:
   - Acid bass slides: a bass rhythm with held notes (continuations) so `LegatoApproach` / `Dynamic` can slide, or a slide rule fit for TB-303 step semantics.
   - Lead contour that is a phrase, not an exercise: avoid long monotone scale runs; prefer motif + variation (repeat cell, then answer), stepwise with an occasional leap and a return; use the project key (`generatorParams.scaleRoot/scale`).
   - Lead accents and slides (masks are passed as 0 today).
   - STYLE that matters for synths: FAITHFUL = canon, VARIANT = new contour on the same rhythm, REWORK = new lead rhythm too.
   - A second G that audibly changes the lead in every genre.
3. **Risks.** What could break: determinism and seeds (`GenerationDomain`, attempt ordinals), Song/MATERIAL reproducibility, DRAM (no new static buffers; tables in flash are fine), CPU (generation runs on the UI thread), and the generation contract tests below.

## Constraints

- Deterministic: the same seed and settings must give the same pattern (tests depend on it).
- No new heap or static DRAM buffers; constexpr tables are fine.
- Keep the code style: data-driven profiles in `generation_profile.cpp` / `tonal_profile.cpp`, no `GenerativeMode` switches in roles code.
- Expect these to need deliberate updates when the vocabulary changes: semantic census and reachability tests (`tests/test_gf2_*`, `tools/semantic_census.py`), canonical rhythm and distinctness tests, `tests/test_generation_stage14_*`, `tests/test_melodic_stage11.cpp`, `tests/test_0_9_9_m1_*`. Say which ones fail and why, rather than editing them blindly.
- Gates before a proposal is final: `tests/run_host_tests.sh` and `scripts/ci/run_core.sh` (the CI runs the second; it caught six issues the first missed in 0.9.17).

## Output we want

1. A table: genre -> lead idea, bass idea, density, slides/accents, keep sparse yes/no.
2. Per iteration: the diff, probe numbers before/after (`genprobe` table rows for the touched genres), and 1-2 renders (WAV/MP3 paths) for the owner to listen to.
3. A short list of risks and failing tests with the reason for each.
