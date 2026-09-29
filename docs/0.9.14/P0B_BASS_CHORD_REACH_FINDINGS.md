# 0.9.14 P0-B — Why development does not reach bass and chords (findings and proposed change)

Status: **findings + proposal, no product code.** Follows the owner's choice of option (b) in `P0_MUSICAL_PLAY_SPEC.md` §9.
Tags: **[OBSERVED]** read from code or measured on the M0 corpus; **[INFERENCE]**; **[OPEN]**.

## 1. Mechanism

1. **[OBSERVED]** A bar function (`Break`, `Reduction`, `Build`, `Turnaround`, `RepeatWithGhosts`, …) is applied by
   `applyRhythmBarFunctionMutation` to the **`RhythmPhrasePlan`**, over the lanes the archetype declares (all eight roles exist in the plan, including
   `BassRhythm`, `ChordRhythm`, `MelodicRhythm`). What it does is small and fixed: `Response` is **metadata only** (no change), `Repeat` and `Return` **copy** a
   bar, `Reduction` and `Break` **drop** events (budget: max drops per level), `Build`, `Turnaround` and `RepeatWithGhosts` **add ghost cues**.
2. **[OBSERVED]** In P1R migration the mutated plan is turned into patterns with `standardDrumPatternBinding(deferredSynthRoles())`: **only the drum roles are
   materialized from the plan; the synth roles are deferred.**
3. **[OBSERVED]** Bass and chord rhythm for the same bar are then realized by **separate owners** (`realizeBassRhythm`, the chord realizer) from the
   archetype, the generation seed, the bar ordinal, and the **kick onsets left after the drum mutation**. The bar function itself is **never passed** to them.

So bass and chord are blind to Break/Build/Reduction; they change only as a side effect of the kick pattern.

## 2. Evidence from the fixed corpus (ordinals 0–7, P3; `tools/m0/build_m0a_p0.sh`)

Every identity whose cycle changed the bass or chord had a bass rhythm that **reads the drums**: `KICK ANSWER` or `GAP FILL`
(Dub 0, Funk 0, UKG 4, UKG 5, DnB 4, DnB 6). None of the identities with `SYNCOPATED HOOK`, `ROLLING DRIVE`, `OFFBEAT PUSH`, `SPARSE ANCHOR` or
`HALF-TIME POCKET` changed (Techno 0/2/3/5/6, Darksynth 4, Dub 5, Funk 7, UKG 2, UKG 7, DnB 3, DnB 5). Reading the drums is necessary, not sufficient:
`KICK ANSWER` also failed when the mutation left the kick untouched (UKG 0, 1, 6; DnB 0, 1). This explains why the result varies within one archetype.

## 3. Admission is a whitelist

**[OBSERVED]** `stage12PhraseEnabledId` admits ten archetype ids (`broken_techno`, `machine_syncopation`, `electro_backskip`, `electro_gap_push`,
`two_step_roll`, `ghosted_roll`, `sparse_fast_break`, `halftime_switch`, `classic_2step`, `skippy_2step`) and `allowedPhraseBars` is {1, 2, 4}, which is why 8-bar
requests are rejected. `halftime_switch` has no eligible trajectory at the tested level. Everything else (for example `straight_drive`, `stacked_quarters`,
`straight_acid`, `sparse_acid`, `steppers`, `funk_house_bridge`, `shuffled_4x4`, `offbeat_open_hat`) is refused as "not admitted". Admission here is a
deliberate Stage 12 candidate overlay, not an accident.

## 4. Proposed change (two parts, both small and separable)

### B1. Bass and chord follow the bar function

Pass the bar's `BarFunction` into the bass and chord rhythm requests as an **input that defaults to no effect**.

* **Compatibility rule (hard):** for `Statement`, `Repeat`, `Return`, `Response` and absent phrase context the output must be **bit-identical** to today.
  Loop output, PMB-P1, GF2 and the D1-B/B1 origin evidence therefore do not change. This is testable by golden comparison over the M0 corpus.
* **Proposed intent per function [OPEN, musical decision]:**

| Function | Bass | Chord |
|---|---|---|
| Break | keep the downbeat attack only (anchor), drop the rest | omit or reduce to one hit |
| Reduction | drop one attack | drop one hit |
| Build | add one push attack in the late steps | add one hit |
| Turnaround | add a late fill attack | unchanged |
| RepeatWithGhosts | unchanged | unchanged |

* Each result must remain a valid `BassRhythmPlan` (protected space, kick relationship), so the existing bass pitch plan and tonal materialization work unchanged, and the
  origin evidence stays exact (it records the plan that was actually used).
* Determinism: any choice uses the existing per-bar seeds; no new randomness source.

### B2. Widen admission

Add archetypes to the whitelist **one at a time**, each with its own trajectory reference and mutation budget, and re-run the corpus after each. Order of value
from the corpus: `straight_drive` (Techno/Dub/Darksynth land on it often), `steppers` (Dub), `funk_house_bridge` (Funk), `offbeat_open_hat` (Techno). Acid and House stay excluded
from the first slice (spec §4). 8-bar phrases are a separate change (`allowedPhraseBars`) and not needed for the 4-bar cycle.

## 5. Order of work and exit

1. Owner decision on the table in B1 (intent per function) and on which archetypes to admit first.
2. B1 behind the compatibility rule; golden test proves Statement/Repeat/Return/Response are unchanged; new unit tests per function.
3. Re-run the fixed corpus. Exit for this step: the structural condition of spec §5 (bass **or** chord changes in the cycle, 8/8 per archetype on ordinals 0–7)
   holds for the archetypes that were failing, or the remaining failures are reported by name.
4. Only then B2, one archetype per commit, corpus re-run each time.
5. Audition renders with mix B; owner listens (DEVELOP, BREAK, Return recorded separately).
6. Gates as in the spec: D0-C…D1-C1, M0-A, PMB-P1, host, unified slots, SDL, ADV, SEQTRAK; DRAM delta reported (expected ≈ 0: code only).

## 6. Risks

* Changing the bass/chord realizers touches the P1R shared migration; the compatibility rule and golden comparison are the guard.
* More audible bass motion changes the character of BREAK: what counts as a good break is a listening question, not a structural one.
* Whitelist growth widens the set that phrase evolution can change; each archetype needs its own listening.
