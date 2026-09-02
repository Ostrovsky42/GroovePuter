# 0.9.9-E1a — One Rhythm Evolution Mutation Owner

Production consolidation checkpoint. Base: `dev_0.9.9 @ 78bc8394ede5e6d81464cff5878c29bbf754c555`.
Contract/evidence only: PR #344 (`f34b2d67ea54387b602c3c84e0890ad197b3eb5b`) and PR #349 (`78a1b30b196a5859e5a972ada205f74ae705ba98`). Their test-only ancestry is not part of this branch.

## Purpose

Make the existing rhythm realizer the single owner that mutates `RhythmPhrasePlan` data while preserving the characterized production operation set, legacy trajectory behavior, PhraseEvolution orchestration, and the real MiniAcid 2/4/8-bar live bridge.

## Exact source audit — before E1a

| function | plans? | selects intent? | selects trajectory? | mutates `RhythmBarPlan` / `RhythmPhrasePlan`? | mutates physical Pattern? | unique behavior | E1a disposition |
|---|---:|---:|---:|---:|---:|---|---|
| `rhythm_realizer.cpp:addVariation()` | no | no | no | yes | no | deterministic production secondary + ghost additions under catalog budgets | preserve under rhythm realizer |
| `rhythm_realizer.cpp:realizeRhythmPhrase()` | base identity/realization | no | no | yes, via `addVariation()` | no | identity establishment/reuse, relationship repair/validation, gate policy | preserve |
| `bar_evolution.cpp:selectTrajectory()` | yes | no | yes | no | no | weighted eligible trajectory selection + deterministic salt domain | preserve as planning |
| `bar_evolution.cpp:applyBarFunction()` | no | no | no | yes | no | Repeat/Response/Reduction/Build/Turnaround/Break/Return executor | remove from bar owner; move implementation under realizer owner |
| `bar_evolution.cpp:dropOneStructuralEvent()` | no | no | no | yes | no | secondary-first bounded reduction with relationship validation | move unchanged under realizer owner |
| `bar_evolution.cpp:addGhostCue()` | no | no | no | yes | no | bounded deterministic trajectory ghost cue | move unchanged under realizer owner |
| `bar_evolution.cpp:evolveRhythmPhrase()` | plans one core segment | no | yes | yes, via private bar executor | no | compatibility request/status surface, trajectory selection, per-bar seed coordinates | retain as planning/compatibility shell; delegate mutation |
| `phrase_evolution.cpp:evolveMultiBarPhrase()` | yes | no | propagates requested trajectory and segment results | no mutation primitive of its own | no | validation, 1/2/4 or 4+4 segmentation, second ordinal, identity reuse, role continuity | preserve |
| `strong_rhythm_live_bridge.cpp:regeneratePhraseAuditionWithProbe(MiniAcid&)` | live request/materialization orchestration | no | supplies phrase request | no semantic mutation primitive | yes | actual shipped caller; materializes evolved bars into Bank B and Song B refs | preserve and execute in SDL integration test |

Before E1a there were two mutation implementation locations: production variation in `rhythm_realizer.cpp` and trajectory mutation primitives in `bar_evolution.cpp`.

## Ownership map — after E1a

```text
PhraseEvolution
  validation / 1,2,4,8 request handling
  8 bars = 4 + 4
  phrase-local segment coordinates
  requested trajectory continuity
  PhraseRhythmIdentity reuse
  phraseOrdinal N -> N+1 for segment 2
  PhraseRoleIdentity continuity
        |
        v
BarEvolution planning / compatibility shell
  eligible weighted trajectory selection
  deterministic trajectory + per-bar seed coordinates
  NO mutation primitive
        |
        v
rhythm_realizer (ONE mutation owner)
  existing addVariation(): characterized production secondary/ghost adds
  applyRhythmBarFunctionMutation(): moved legacy BarFunction executor
  reduction/drop only when the selected BarFunction already requests it
  protected space / lane bounds / hard relationship safety
        |
        v
materialization / live bridge
  no semantic re-mutation
```

`src/generation/rhythm/rhythm_realizer_evolution.cpp` is an implementation split of the existing `rhythm_realizer` owner, not a third abstraction. Its API is declared in `rhythm_realizer.h`; `bar_evolution.cpp` contains no `dropOneStructuralEvent`, `addGhostCue`, or `switch (function)` mutation executor after consolidation.

## Preserved operation corpus

The fixed-seed corpus is copied from the final characterization evidence and compared byte-for-field output against the frozen golden:

```text
DIFF id=404 secondary_added=3 secondary_removed=0 ghost=1 accent=0 structural_unchanged=false DROP=0 DISPLACE=0 RERUN_EQUAL=true
DIFF id=413 secondary_added=3 secondary_removed=0 ghost=1 accent=0 structural_unchanged=false DROP=0 DISPLACE=0 RERUN_EQUAL=true
DIFF id=714 secondary_added=2 secondary_removed=0 ghost=1 accent=0 structural_unchanged=false DROP=0 DISPLACE=0 RERUN_EQUAL=true
SECONDARY-SEARCH id=404 ordinal=0 added=2
AUDITION-DIRECT bars=2 segments=1 traj=2/0 identity_bars=2 phraseOrdinal_transition=none RERUN_EQUAL=true
AUDITION-DIRECT bars=4 segments=1 traj=5/0 identity_bars=4 phraseOrdinal_transition=none RERUN_EQUAL=true
AUDITION-DIRECT bars=8 segments=2 traj=5/5 identity_bars=4 phraseOrdinal_transition=N->N+1 RERUN_EQUAL=true
LEGACY trajectory=6 primitive_structural_after=8220 policy=trajectory RERUN_EQUAL=true
LEGACY trajectory=7 primitive_structural_after=8220 policy=trajectory RERUN_EQUAL=true
```

Evidence nuance: `accents` remains part of `RoleRhythmPlan`, but PR #344 characterized `accent=0` in the frozen production rows. E1a therefore does **not** invent a new accent mutation pass. Production `addVariation()` also remains free of `DROP` and `DISPLACE`. The existing legacy reduction/drop semantics remain reachable only through the already selected Reduction/Break BarFunctions.

## Removed / delegated duplicate behavior

Moved out of `bar_evolution.cpp` and into the canonical realizer owner without semantic changes:

- ghost cue candidate search and ornament budget enforcement;
- secondary-first drop candidate ordering;
- transactional relationship-safe drop validation;
- Repeat / RepeatWithGhosts / Response / Reduction / Build / Turnaround / Break / Return execution;
- evolved-plan validation after those mutations.

Retained in `bar_evolution.cpp`:

- request validation boundary;
- Stage 2 realization call;
- eligible trajectory lookup and weighted deterministic choice;
- trajectory/per-bar deterministic seed derivation;
- compatibility result/status surface;
- `evolvedPlanValid()` as an explicit delegate to `rhythmMutationPlanValid()`.

## Phrase planning retained

`PhraseEvolution` remains the bounded orchestration owner. It does not call the canonical mutation primitive directly and contains no drop/ghost mutation helper. For eight bars it still calls two four-bar core segments; segment 2 receives `phraseOrdinal + 1` and reuses `first.identity`. Role identity is copied unchanged into the result.

This checkpoint deliberately does not redesign trajectory selection ownership: the existing deterministic weighted selector remains the BarEvolution planning utility used under PhraseEvolution. E1a changes mutation ownership only; moving that deterministic planner would be a separate semantic-risk change.

## 8-bar proof

The fixed-seed characterization requires:

- `bars=8` -> `segments=2`;
- first segment is bars 0..3 and second segment bars 4..7;
- first segment trajectory remains `5` for the frozen fixture;
- second segment receives the same `PhraseRhythmIdentity` and `phraseOrdinal N+1`;
- rerun equality remains true.

The real SDL MiniAcid integration additionally checks the physical Bank B rows and Song B references rather than replacing runtime evidence with source assertions.

## Live-bridge proof

`tests/test_e1_live_bridge_runtime.cpp` is the final PR #349 runtime test. It links the shipped MiniAcid/SceneManager/strong-rhythm source graph and directly calls:

```cpp
regeneratePhraseAuditionWithProbe(MiniAcid&)
```

It covers 2/4/8-bar materialization, the 8-bar 4+4 reconstruction, Bank B drum/synth rows, Song B references/state, and the bounded SelectionFailed path. The SDL Makefile target is `e1-live-bridge-characterization`.

## Memory / stack

No production value structure receives a new field. `RhythmPhrasePlan`, `BarEvolutionRequest`, `BarEvolutionResult`, `PhraseEvolutionResult`, Scene, Song, and physical Pattern capacities are unchanged.

Pre-consolidation host GCC `.su` evidence from PR #344:

- `evolveRhythmPhrase`: 1216 B, static;
- `dropOneStructuralEvent`: 624 B, dynamic,bounded;
- `evolveMultiBarPhrase`: 3536 B, static.

E1a compiles separate `.su` files for the planner and canonical mutation owner and prints the actual nested production chain. These numbers are a host compiler comparison/regression guard only. They are **not** ESP32-S3 runtime stack high-water marks. Cardputer firmware continues to expose the real `uxTaskGetStackHighWaterMark()` probe in the shipped live bridge; no host `.su` value is relabeled as hardware HWM.

Fixed DRAM is validated by the existing `Core regressions` Cardputer ADV job. No heap-backed mutation owner, unbounded container, scheduler state, or lifecycle owner is introduced.

## Build / validation

Host ownership + GCC/Clang/sanitizers:

```bash
bash tests/run_rhythm_stage6_1_tests.sh
bash tests/run_phrase_stage12_tests.sh
```

Real shipped MiniAcid SDL path:

```bash
sudo apt-get install -y build-essential libsdl2-dev libsdl2-gfx-dev
make -C platform_sdl e1-live-bridge-characterization CXX=g++
```

Full PR regression gates are provided by the existing `Core regressions` workflow:

- Stage 1..6.1 host suites;
- Stage 12 and Stage 13/14/15 host regressions;
- `tests/run_host_tests.sh`;
- SDL desktop build;
- Cardputer ADV compile;
- fixed DRAM check;
- SEQTRAK MIDI-only compile/check.

The E1a-specific workflow reruns Stage 6.1, Stage 12 fixed-seed corpus, and the real MiniAcid live-bridge test as one focused ownership gate.

## Hardware assumptions

E1a changes no pins, voltages, buses, display initialization, MIDI wiring, or audio routing. Cardputer-Adv PORT.A remains SDA GPIO2 / SCL GPIO1 if external I2C units are present; this checkpoint does not touch I2C. No physical peripheral is required for host/SDL acceptance.

## Non-goals

Not implemented here:

- Phrase Lab UI;
- PatternLease;
- P2 EVOLVE NEXT UI;
- TransformationIntent UX;
- F08.1 HarmonicRhythm;
- E0a temporal plumbing;
- scheduler/lifecycle changes;
- new generation/mutation features;
- new production ACCENT, DROP, or DISPLACE vocabulary.

## Acceptance checklist

- [ ] Branch merge-base is exactly `78bc8394ede5e6d81464cff5878c29bbf754c555`; no #344/#349 test-only ancestry.
- [ ] `test_e1a_source_contract.py` proves `bar_evolution.cpp` has no mutation primitive and PhraseEvolution has none.
- [ ] Fixed-seed production corpus matches the frozen #344 golden with no update.
- [ ] Stage 6 and Stage 6.1 GCC/Clang/sanitizer matrices pass.
- [ ] Phrase Stage 12 GCC/Clang/sanitizer matrices pass using the #349 field-wise deterministic comparison.
- [ ] 2/4/8 phrase evolution and 8-bar 4+4 identity/ordinal contract pass.
- [ ] Real `regeneratePhraseAuditionWithProbe(MiniAcid&)` SDL integration passes.
- [ ] Full host regressions pass.
- [ ] SDL desktop build passes.
- [ ] Cardputer ADV build passes.
- [ ] Fixed DRAM gate passes.
- [ ] SEQTRAK MIDI-only build/check passes.
- [ ] Host `.su` remains bounded and is reported only as host comparison data.
- [ ] Draft PR remains unmerged at hard stop.
