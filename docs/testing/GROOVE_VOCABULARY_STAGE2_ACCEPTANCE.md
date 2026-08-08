# Groove Vocabulary Stage 2 acceptance

## Purpose

Validate the deterministic `RelationshipResolver` + `RhythmPhraseRealizer` implementation before it is connected to production Generate. Stage 2 produces only bounded role-level `RhythmPhrasePlan` data and does not materialize patterns, change Scene state, select Genre mappings, choose Synth engines, or generate pitch.

The Stage 2 branch is based on the final reviewed Stage 1 head. Until Stage 1 is merged into `dev_0.9_test`, Stage 2 remains a branch artifact and must not become another stacked merge PR.

## Hardware list

For automated gates:

- no hardware is required for host property tests;
- GitHub Actions builds M5Stack Cardputer ADV normal firmware;
- GitHub Actions builds the SEQTRAK MIDI-only Cardputer ADV profile.

Optional hardware smoke after Stage 2 is rebased onto merged Stage 1:

- M5Stack Cardputer ADV;
- USB-C data cable.

No MIDI device, PORT.A module, AUX cable, or external display is required because Stage 2 is not wired into runtime Generate.

## Wiring

None for automated tests.

For an optional Cardputer ADV boot smoke, connect only USB-C. Stage 2 does not touch PORT.A I2C, audio routing, MIDI routing, display initialization, or GPIO ownership.

## Build / flash steps

```bash
git fetch origin
git switch agent/20260808-04-groove-vocabulary-stage2
git reset --hard origin/agent/20260808-04-groove-vocabulary-stage2

bash tests/run_rhythm_stage1_tests.sh
bash tests/run_rhythm_stage2_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Optional flash after the branch is rebased onto merged Stage 1:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

Host tests must prove:

- exact `Exclude`, `Coincide`, `Offset`, `Respond`, and soft `FillGaps` predicates;
- `RelationshipScope::BarLocal` never wraps across a bar;
- `RelationshipScope::Phrase` may cross a bar boundary but never wraps the phrase end to its start;
- one hard `Respond` target satisfies at most one source window with deterministic nearest-source ownership;
- P1/P2/P3 reuse one deterministic `PhraseRhythmIdentity` when the caller requests VARIATE semantics;
- unpinned BarEvolution trajectory may differ by P-level without rerolling structural identity;
- `Repeat` has zero structural drift;
- `Return` restores the Statement structural core;
- immutable anchors are never removed;
- canonical anchors are suspended only by matching `BarFunction + TransformationIntent + AnchorTransformRule` permission;
- role-scoped protected space is never filled;
- hard relationship violations never escape as a valid plan;
- runtime impossible composition returns `InvalidConstraintSet`;
- `ValidButSparse` remains distinct from invalid musical minima;
- gate/importance authority remains inside the rhythm plan (`Normal` implicit, ghost events use `Short` in the current Stage 2 implementation);
- the realization path uses no heap allocation or dynamic standard containers;
- same relevant inputs produce byte-equivalent semantic plan fields across repeated runs;
- production `ModeManager` / `MiniAcid` generation remains unwired from Stage 2.

Firmware behavior must remain unchanged because no production caller invokes `realizeRhythmPhrase()`.

## Troubleshooting

### Stage 2 host test fails but Stage 1 passes

Treat this as a Stage 2 contract or implementation defect. Do not weaken the property corpus to make a seed disappear. Reproduce the failing seed, fix the resolver/realizer, reset the three-review counter to `0/3`, and rerun the entire Stage 2 matrix.

### Catalog validation fails in a Stage 2 fixture

Fix the fixture or Stage 1 contract first. Stage 2 must consume only catalogs accepted by `validateRhythmCatalog()`; it must not bypass Stage 1 validation.

### Cardputer ADV build fails while host tests pass

Treat it as a release-blocking embedded compatibility defect. The Stage 2 `.cpp` files live under `src/` and are part of Arduino compilation discovery even though they are not called at runtime.

### Fixed DRAM / firmware size increases

Record exact before/after values for normal and MIDI-only profiles. A code-size increase is expected once the realizer is linked by runtime callers in later stages, but Stage 2 is currently dead/unwired code and should normally be linker-eliminated. Any unexpected fixed-DRAM increase must be explained before acceptance.

### Existing core host regression is red

The current Stage 1 base contains a known unrelated source-regression drift around the removed Cardputer ADV PA_EN macro. Stage 1/2 focused runners must execute before that legacy step so their status remains independently observable. Do not modify that unrelated hardware contract inside Stage 2.

## Acceptance checklist

- [ ] `bash tests/run_rhythm_stage1_tests.sh` passes;
- [ ] Stage 2 source ownership regression passes;
- [ ] GCC Stage 2 property test passes;
- [ ] Clang Stage 2 property test passes;
- [ ] ASan + UBSan Stage 2 property test passes;
- [ ] 512-seed P1/P2/P3 corpus has zero identity-continuity violations;
- [ ] repeat structural drift violations = 0;
- [ ] immutable anchor violations = 0;
- [ ] illegal canonical suspension violations = 0;
- [ ] protected-space violations = 0;
- [ ] hard relationship violations = 0;
- [ ] runtime impossible compositions return `InvalidConstraintSet`;
- [ ] reference binding onset-invention violations = 0;
- [ ] reference binding structural-drop violations = 0;
- [ ] SDL build passes;
- [ ] Cardputer ADV normal build passes;
- [ ] fixed DRAM gate passes;
- [ ] SEQTRAK MIDI-only build passes;
- [ ] normal and MIDI-only flash/fixed-DRAM deltas are measured against the Stage 1 base;
- [ ] production Generate behavior is unchanged / Stage 2 remains unwired;
- [ ] temporary Stage 2 branch-only CI trigger is removed from the final diff;
- [ ] three consecutive control reviews pass on one unchanged final source SHA; any finding resets the counter to `0/3`.
