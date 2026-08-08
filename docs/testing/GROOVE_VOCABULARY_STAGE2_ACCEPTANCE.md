# Groove Vocabulary Stage 2 acceptance

## Purpose

Validate the deterministic `RelationshipResolver` + `RhythmPhraseRealizer` implementation before it is connected to production Generate. Stage 2 produces only bounded role-level `RhythmPhrasePlan` data and does not materialize patterns, change Scene state, select Genre mappings, choose Synth engines, generate pitch, or own BarEvolution.

Stage 6 remains the owner of 2–4 bar trajectory selection and `BarFunction` evolution. Stage 2 may realize 1–4 caller-sized bars, but every bar is a plain `Statement`; `trajectoryId` remains `kNoTrajectoryId` and `TransformationIntent` remains `Auto`.

Until Stage 1 is merged into `dev_0.9_test`, Stage 2 remains a direct draft PR to `dev_0.9_test`; it must not be merged before its Stage 1 dependency is present in the base.

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
- feasible hard-relationship repair is transactional, so a failed candidate cannot poison another legal candidate;
- P1/P2/P3 reuse one deterministic `PhraseRhythmIdentity` when the caller requests VARIATE semantics;
- P-level changes use isolated deterministic variation domains and do not reroll structural identity;
- Stage 2 never selects or pins a BarEvolution trajectory;
- Stage 2 never applies `Repeat`, `Return`, `Reduction`, `Break`, `Build`, `Turnaround`, or `Response` BarFunction semantics;
- every Stage 2 output bar remains `Statement`, with `trajectoryId == kNoTrajectoryId` and `intent == Auto`;
- immutable and canonical anchors are never removed in Stage 2;
- role-scoped protected space is never filled;
- hard relationship violations never escape as a valid plan;
- invalid reused identity returns `InvalidConstraintSet` instead of partially mutating caller data;
- a legal variation never becomes invalid merely because an optional structural/ghost addition would exceed the global structural or ornament budget; that candidate is skipped instead;
- optional P2/P3 additions are transactional and cannot turn an already valid shared identity into a hard-relationship failure;
- hard `Respond` repair spends capacity only on candidates that reduce the deterministic response deficit;
- `ValidButSparse` remains distinct from invalid musical minima;
- gate/importance authority remains inside the rhythm plan: `Normal` is implicit, explicit `Short/Held/Tie` lane overlays survive realization, and unclassified ghost events use `Short`;
- the realization path uses no heap allocation or dynamic standard containers;
- same relevant inputs produce byte-equivalent semantic plan fields across repeated runs;
- production `ModeManager` / `MiniAcid` generation remains unwired from Stage 2.

Firmware behavior must remain unchanged because no production caller invokes `realizeRhythmPhrase()`.

## Explicitly deferred to Stage 6

The following are intentionally **not** Stage 2 implementation acceptance items:

```text
trajectory selection
BarEvolution RNG domain consumption
Repeat / Return semantics
Break / Reduction / Build / Turnaround / Response BarFunctions
canonical-anchor suspension by AnchorTransformRule
explicit TransformationIntent routing
```

Those contracts stay represented in the Stage 1 data model, but Stage 2 must not execute them.

## Normative stage-ownership clarification

`GROOVE_VOCABULARY_MUSICAL_CONTRACTS.md` defines cumulative Groove Vocabulary Core v1 invariants. Its Stage 2 gate language about Repeat stability, legal canonical-anchor suspension, and physical-binding preservation must **not** be read as transferring ownership of those mechanisms into this implementation stage.

The stage-specific ownership from `GROOVE_VOCABULARY_ARCHITECTURE_BRIEF.md` remains authoritative:

```text
Stage 2 -> relationship resolution + deterministic Statement realization
Stage 4 -> materialization / physical-binding boundary checks
Stage 6 -> BarEvolution, Repeat/Return and transformable canonical-anchor behavior
```

Therefore:

- G-22 `Repeat stability` remains normative Core v1 behavior and becomes executable acceptance when Stage 6 owns `Repeat`;
- G-23 `Transformable anchors` remains normative, while Stage 2 proves the stricter pre-BarEvolution rule that immutable **and canonical** anchors are never suspended;
- G-25 `Binding preservation` remains normative for the later binder/materializer. Stage 2 may use a reference/mock binding preservation test as an interface proof, but it must not implement or own the production physical binder.

This clarification narrows only **when** each cumulative invariant becomes executable; it does not weaken or remove G-22, G-23, or G-25.

## Troubleshooting

### Stage 2 host test fails but Stage 1 passes

Treat this as a Stage 2 contract or implementation defect. Do not weaken the property corpus to make a seed disappear. Reproduce the failing seed, fix the resolver/realizer, reset the three-review counter to `0/3`, and rerun the entire Stage 2 matrix.

### Catalog validation fails in a Stage 2 fixture

Fix the fixture or Stage 1 contract first. Stage 2 must consume only catalogs accepted by `validateRhythmCatalog()`; it must not bypass Stage 1 validation.

### Cardputer ADV build fails while host tests pass

Treat it as a release-blocking embedded compatibility defect. The Stage 2 `.cpp` files live under `src/` and are part of Arduino compilation discovery even though they are not called at runtime.

### Fixed DRAM / firmware size increases

Record exact before/after values for normal and MIDI-only profiles. Stage 2 is currently dead/unwired code and should normally be linker-eliminated. Any unexpected fixed-DRAM or firmware increase must be explained before acceptance.

### Existing core host regression is red

The current Stage 1 base contains a known unrelated source-regression drift around the removed Cardputer ADV PA_EN macro. Stage 1/2 focused runners must execute before that legacy step so their status remains independently observable. Do not modify that unrelated hardware contract inside Stage 2.

## Acceptance checklist

- [ ] `bash tests/run_rhythm_stage1_tests.sh` passes;
- [ ] Stage 2 source ownership/scope regression passes;
- [ ] GCC Stage 2 property + adversarial tests pass;
- [ ] Clang Stage 2 property + adversarial tests pass;
- [ ] ASan + UBSan Stage 2 property + adversarial tests pass;
- [ ] the complete runner includes base, relationship, Respond and Atlas-realization falsification suites;
- [ ] 512-seed P1/P2/P3 corpus has zero identity-continuity violations;
- [ ] Stage 2 BarEvolution/trajectory-selection violations = 0;
- [ ] non-Statement Stage 2 bars = 0;
- [ ] immutable anchor violations = 0;
- [ ] canonical anchor violations = 0;
- [ ] protected-space violations = 0;
- [ ] hard relationship violations = 0;
- [ ] transactional relationship false-negative fixture passes across its seed corpus;
- [ ] optional Respond-source variation cannot invalidate an otherwise legal shared identity;
- [ ] explicit `Short/Held/Tie` gate-policy violations = 0;
- [ ] optional structural additions never exceed global structural max;
- [ ] ghost additions never exceed global ornament max;
- [ ] invalid reused identity returns `InvalidConstraintSet`;
- [ ] reference binding onset-invention violations = 0;
- [ ] reference binding structural-drop violations = 0;
- [ ] SDL build passes;
- [ ] Cardputer ADV normal build passes;
- [ ] fixed DRAM gate passes;
- [ ] SEQTRAK MIDI-only build passes;
- [ ] normal and MIDI-only flash/fixed-DRAM deltas are measured against the final Stage 1 base;
- [ ] production Generate behavior is unchanged / Stage 2 remains unwired;
- [ ] all temporary self-patch / measurement workflows are absent from the final diff;
- [ ] three consecutive control reviews pass on one unchanged final source SHA; any finding resets the counter to `0/3`.
