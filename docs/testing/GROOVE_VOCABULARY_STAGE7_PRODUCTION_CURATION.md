# Groove Vocabulary Stage 7 — Batch 2 Production Curation

## Purpose

Promote the four Stage 7B hardware-approved rhythm identities into the production `ReferenceVocabulary` without carrying the temporary audition harness into production.

Admitted identities:

```text
HARD_01  stacked_quarters   PASS
HARD_06  electro_backskip   PASS / DISTINCT
HARD_07  funk_house_bridge  PASS
HARD_08  electro_gap_push   PASS / DISTINCT
```

`HARD_06` and `HARD_08` are separate musical identities. They must not be merged.

This PR is vocabulary curation only. It does **not** add Genre/Variant routing, BarEvolution production wiring, Bass/Motif generation, Mood, Feel changes, Scene fields, UI or persistence behavior.

## Hardware list

For the optional production smoke:

- M5Stack Cardputer ADV;
- USB-C data cable;
- built-in speaker or normal audio output path.

SEQTRAK, PORT.A devices, external display and SD content are not required.

## Wiring

USB-C only.

This stage changes no GPIO, PORT.A I2C, I2S/audio, USB-MIDI, external-display or SD wiring contract.

## Build / flash

```bash
git fetch origin
git switch agent/20260810-02-stage7-production-curation-batch2
git reset --hard origin/agent/20260810-02-stage7-production-curation-batch2

bash tests/run_rhythm_stage1_tests.sh
bash tests/run_rhythm_stage2_tests.sh
bash tests/run_rhythm_stage3_tests.sh
bash tests/run_rhythm_stage4_tests.sh
bash tests/run_rhythm_stage5_tests.sh
bash tests/run_rhythm_stage6_tests.sh
bash tests/run_rhythm_stage6_1_tests.sh

bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Use the actual serial port if it differs from `/dev/ttyACM0`.

## Production transfer contract

The four production entries must preserve the Stage 7B audition contracts exactly:

- one bar only;
- active roles are exactly Kick / Backbeat / ClosedHat / OpenHat / Percussion;
- no BassRhythm, ChordRhythm or MelodicRhythm is inferred;
- canonical/preferred/optional masks are unchanged;
- lane structural and ornament bounds are unchanged;
- density contracts are unchanged;
- HARD_01 and HARD_07 keep no forced Kick/Backbeat relationship;
- HARD_06 and HARD_08 retain hard `Backbeat -> Kick` exclusion;
- HARD_07 retains its SwingCompatible timing contract;
- no protected space is invented.

The production IDs remain:

```text
711 stacked_quarters
712 electro_backskip
713 funk_house_bridge
714 electro_gap_push
```

They are intentionally not renumbered to a contiguous `421..424` range. `RhythmPhraseRealizer` derives the RhythmIdentity seed from `archetype.id`; changing the IDs would change the deterministic corpus that was hardware-audited.

## Deterministic audition-corpus regression

The production `ReferenceVocabulary` is tested with the same Stage 7B generation context:

```text
project seed: 1..64
phraseOrdinal: 0
phraseBars: 1
P1 -> P2 -> P3 using one shared PhraseRhythmIdentity
```

Expected exact metrics:

| Identity | P1 distinct | max bucket | P2 changed | P3 changed |
|---|---:|---:|---:|---:|
| `stacked_quarters` | 6 | 20 | 64/64 | 64/64 |
| `electro_backskip` | 39 | 4 | 64/64 | 64/64 |
| `funk_house_bridge` | 26 | 6 | 64/64 | 64/64 |
| `electro_gap_push` | 32 | 5 | 64/64 | 64/64 |

Any mismatch invalidates the transfer and requires a new hardware listening decision before production admission.

## Expected behavior

### Host / CI

`tests/run_rhythm_stage3_tests.sh` must report a valid 24-entry production vocabulary and the exact Stage 7 Batch 2 corpus metrics above under GCC, Clang and ASan/UBSan.

All 24 grammar fingerprints must remain unique. `electro_backskip` and `electro_gap_push` must remain structurally distinct.

### Screen

No new Stage 7 screen, toast, shortcut or selectable Genre/Variant entry appears in this PR.

That is expected: production routing is deliberately deferred. Existing UI must look and behave exactly as before.

### Serial

No new Stage 7 runtime log is expected. Normal boot/runtime diagnostics remain unchanged.

### Audio

Existing production generation remains unchanged because this PR only expands the reference catalog. The four new identities are not user-reachable until a later explicit routing PR.

## Troubleshooting

### Stage 3 test reports a different Stage 7 distinct/max-bucket count

Do not update the expected numbers. First verify that IDs remain `711..714`, lane contracts match the Stage 7B audited definitions and `GenerationContext` uses `phraseOrdinal=0` for the transfer regression.

A changed corpus means the production transfer is no longer identical to what was hardware-audited.

### `electro_backskip` and `electro_gap_push` collapse together

Treat this as a Stage 7 blocker. HARD_06 and HARD_08 were explicitly judged distinct in hardware listening. Verify their exact Kick contracts and retain the separate hard Backbeat/Kick exclusion grammars.

### New Stage 7 items are not visible in GENRE

Expected. This PR does not modify production selection/routing.

### Firmware fails after catalog-only changes

Check the exact failing focused step. Do not attribute the known inherited Cardputer ADV `PA_EN` source assertion to Stage 7 unless this branch changes that hardware profile/test.

## Acceptance checklist

- [ ] exactly four new `ReferenceVocabulary` definitions exist;
- [ ] production IDs are exactly `711..714`;
- [ ] all four are one-bar and drums-only;
- [ ] exact Stage 7B lane/density/timing/relationship contracts are pinned by tests;
- [ ] HARD_06 and HARD_08 remain separate grammar fingerprints;
- [ ] exact 64-seed metrics are `6/20`, `39/4`, `26/6`, `32/5`;
- [ ] P2 changes all 64 seeds for each identity without changing PhraseRhythmIdentity;
- [ ] P3 changes all 64 seeds for each identity without changing PhraseRhythmIdentity;
- [ ] all 24 production grammars remain valid and non-collapsed;
- [ ] Stage 4 shadow coverage consumes all production definitions;
- [ ] no audition UI/session code is present in the PR;
- [ ] no Genre/Scene/Song/PhraseCore/BarEvolution production routing is added;
- [ ] no Bass/Motif/Mood/Feel/Texture scope is added;
- [ ] SDL build passes;
- [ ] Cardputer ADV normal build passes;
- [ ] fixed DRAM gate passes;
- [ ] SEQTRAK MIDI-only build passes;
- [ ] Synth persistence passes;
- [ ] optional Cardputer boot smoke shows unchanged UI/audio/runtime behavior;
- [ ] three consecutive clean control reviews pass on one unchanged final SHA.
