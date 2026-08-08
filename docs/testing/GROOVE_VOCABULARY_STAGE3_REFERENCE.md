# Groove Vocabulary Stage 3 Reference Vocabulary

## Purpose

Validate the first production-candidate Rhythm Vocabulary package against the Stage 1 contracts and Stage 2 realizer before any materializer, shadow backend, Genre, Scene, Song, UI, or production Generate migration.

Stage 3 contains exactly 20 curated one-bar rhythm archetypes. Stage 3 is the catalog-contract stage: passing it means the vocabulary package is technically coherent and ready for Stage 4 materialization/shadow integration.

Stage 3A was a temporary listening gate inserted between Stage 2 and Stage 3. Its purpose was to falsify the architecture on real Cardputer hardware before investing in the full reference catalog. It is not a downstream sub-stage of Stage 3 and must not become a second production vocabulary owner.

## Stage ordering

The relevant sequence is:

1. Stage 1 — data model + catalog validation.
2. Stage 2 — RelationshipResolver + RhythmPhraseRealizer.
3. Stage 3A — temporary hardware listening gate; already used to validate that the grammar approach produces musically useful results.
4. Stage 3 — curated reference vocabulary.
5. Stage 4 — materializer + shadow backend.

The Stage 3A branch remains audition-only historical test infrastructure and is not merged into the production chain.

## Hardware list

Host validation requires no hardware.

Embedded compatibility gates use:

- M5Stack Cardputer ADV target, normal firmware profile;
- M5Stack Cardputer ADV target, SEQTRAK MIDI-only profile;
- no external wiring or peripherals are required for Stage 3 itself.

## Wiring

None. Stage 3 is not wired into production Generate or playback.

Stage 4 owns the first production-candidate materialization and shadow-backend integration. Stage 3 must not grow runtime bindings merely to make individual archetypes audible.

## Reference package

- Four-floor / techno: `straight_drive`, `offbeat_open_hat`, `hypnotic_sparse`, `broken_techno`
- Acid-compatible frames: `straight_acid`, `rolling_acid`, `syncopated_acid`, `sparse_acid`
- Dub: `one_drop_space`, `steppers`, `sparse_skank`, `chord_response`
- Break / D&B: `two_step_roll`, `ghosted_roll`, `sparse_fast_break`, `halftime_switch`
- UK / machine: `classic_2step`, `skippy_2step`, `shuffled_4x4`, `machine_syncopation`

ACID is deliberately not a `RhythmFamily`. Acid-compatible entries use generic rhythm families and expose bass-rhythm/duration relationships for later Genre/Bass/Articulation composition.

## Build / test

Run the focused matrix:

```bash
bash tests/run_rhythm_stage1_tests.sh
bash tests/run_rhythm_stage2_tests.sh
bash tests/run_rhythm_stage3_tests.sh
```

Firmware compatibility:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

SDL compatibility:

```bash
make -C platform_sdl clean all CXX=g++
```

## Expected behavior

The Stage 3 host matrix must prove:

- the catalog contains exactly 20 unique grammars;
- every grammar validates;
- every grammar realizes legal P1/P2/P3 results across seeds 1..64;
- P2/P3 reuse the exact semantic `PhraseRhythmIdentity` established by P1;
- repeated realization with the same inputs is deterministic;
- no protected-space, lane-bound, or hard-relationship violation occurs;
- every archetype has more than one P1 identity across the 64-seed corpus;
- broken/2-step reference frames do not collapse into generic four-floor kick topology;
- dub protected space remains role-scoped and empty for affected roles;
- acid-compatible frames expose independent, duration-aware `BassRhythm` motion;
- every reference archetype exposes at least one legal P2 and P3 realization distinct from P1.

The firmware itself must sound and behave exactly as Stage 2 because Stage 3 is not connected to production Generate.

## Troubleshooting

If an archetype is valid but has only one identity, do not add arbitrary randomization. Inspect the interaction of lane `structuralMin`, global `DensityContract::structuralPreferred`, anchors, and available preferred/optional sites.

If GCC passes but Clang fails equality/determinism checks, do not compare fixed-capacity C++ structs using raw `memcmp`; padding bytes are not semantic state. Compare fields and masks explicitly.

If the legacy `tests/run_host_tests.sh` fails on the existing `GROOVEPUTER_CARDPUTER_ADV_PA_EN_PIN 21` assertion after the Stage 1/2/3 focused steps pass, treat that as the existing base-branch host-test drift, not a Stage 3 catalog failure. Do not hide it.

If Arduino fails while host tests pass, treat that as a release blocker: files under `src/` are visible to the firmware build even when the vocabulary is not runtime-wired.

## Acceptance checklist

- [ ] `tests/run_rhythm_stage1_tests.sh` passes.
- [ ] `tests/run_rhythm_stage2_tests.sh` passes.
- [ ] `tests/run_rhythm_stage3_tests.sh` passes under GCC, Clang, ASan and UBSan.
- [ ] 20/20 archetypes validate and realize P1/P2/P3 for seeds 1..64.
- [ ] No archetype collapses to one P1 identity.
- [ ] No duplicate grammar fingerprint is present.
- [ ] Musical anti-collapse invariants pass.
- [ ] Cardputer ADV normal build passes.
- [ ] Fixed DRAM gate passes.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] SDL build passes.
- [ ] Exact Stage 2 -> Stage 3 firmware/DRAM delta is recorded.
- [ ] Temporary measurement workflows are absent from final diff.
- [ ] Stage 3A remains outside the production merge chain and is not expanded into a duplicate vocabulary catalog.
- [ ] Stage 3 exposes no materializer, shadow backend, Genre mapping, Scene/Song ownership, UI wiring, or production Generate path.
- [ ] Three consecutive control reviews pass on one unchanged final SHA; any finding resets the counter to 0/3.

## Exit contract

When this checklist is satisfied, Stage 3 is complete. The next branch must be Stage 4 and may consume `ReferenceVocabulary` through a materializer/shadow backend, but it must not move Genre/Song/Scene ownership yet. Strong rhythmic production-path migration belongs to Stage 5.
