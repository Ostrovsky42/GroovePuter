# GroovePuter 0.9.9-B1 — Generation Mutation Owner

## Purpose

Move the already-existing quantized GENRE/full-generation and synth-reroll persistent commits onto the canonical 0.9.8 `UndoOwner` without changing activation ownership, persistence schema, Output Ownership, Device Profiles, or the 0.9.9-A Lo-Fi compatibility contract.

Base: `dev_0.9.9` @ `c93ddd135b5931fbc330b4f2db245890f2f16601`.

B1 deliberately does not migrate Pattern Editor `G`; that legacy generator is a separate B2 handoff so its musical algorithm is not silently replaced.

## Hardware list

Host acceptance requires no hardware.

Build acceptance uses the repository targets for:
- M5Stack Cardputer ADV / ESP32-S3.
- Cardputer ADV SEQTRAK MIDI-only variant.

## Wiring

None. B1 changes no MIDI, I2C, USB, display, audio, or external-device wiring.

## Ownership contract

- PREPARE produces a complete fixed candidate before persistent mutation.
- COMMIT uses the single global `GroovePuterUndo::undoOwner()` with `UndoKind::Generation`.
- One accepted generation commit creates one receipt and one Scene revision transition.
- Failed/busy/target-changed generation does not publish a new receipt or revision.
- `GenerationUndoPayload` is trivially copyable and must fit the existing 1536-byte R2 budget; the budget is not enlarged.
- Full generation receipt covers the exact targeted Synth A, Synth B, Drums, Genre settings, groovebox mode, BPM and swing value changed by that atomic commit.
- Synth-only generation receipt restores only the targeted synth material; unrelated tracks and configuration are outside the receipt.
- PLAY preparation remains scratch-only. The existing BAR_START hook remains the publication boundary in B1; C owns the later persistent-COMMIT vs audible-ACTIVATE split.
- Pending activation remains runtime-only and is not persisted.
- Output Ownership and Device Profile fields are neither captured nor mutated by the generation receipt.
- Lo-Fi persisted identity remains `GenerativeMode::LoFi == 15`; recipe IDs `12..17` remain unchanged.

## Build / test

Focused gate:

```bash
mkdir -p build/host-tests

g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
  tests/test_generation_undo_owner_0_9_9.cpp \
  -o build/host-tests/test_generation_undo_owner_0_9_9
build/host-tests/test_generation_undo_owner_0_9_9

python3 tests/test_generation_undo_owner_0_9_9_source.py

bash tests/run_undo_0_9_8_r2_tests.sh
bash tests/run_undo_0_9_8_r3_tests.sh
```

Compatibility gate:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_generation_0_9_9_compatibility.cpp \
  -o build/host-tests/test_generation_0_9_9_compatibility
build/host-tests/test_generation_0_9_9_compatibility
python3 tests/test_generation_0_9_9_source_regressions.py
```

Normal repository acceptance:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

## Expected behavior

With transport stopped, an accepted GENRE materialization or quantized synth reroll prepares material first and then performs one bounded persistent commit through `UndoOwner`. The Scene revision advances exactly once. Existing A-stage Lo-Fi identities and realized-material compatibility remain unchanged.

While transport is playing, B1 preserves the existing next-BAR publication behavior; it does not introduce another clock, queue, or persisted pending state.

B1 intentionally makes no claim about physical Cardputer/SEQTRAK behavior beyond successful firmware builds.

## Troubleshooting

- `GenerationUndoPayload exceeds 1536`: do not increase the owner budget in B1; reduce the receipt or re-evaluate the atomic mutation unit.
- direct `markSceneMutated()` found in the selected generation owner: duplicate revision ownership; do not promote.
- generation call found inside `commitPreparedGeneration`: PREPARE/COMMIT boundary was violated; do not promote.
- Lo-Fi ID/recipe regression: persisted compatibility was broken; do not renumber silently.
- R2/R3 Undo regression: generation integration damaged the canonical owner; do not promote.
- Cardputer fixed-DRAM failure: treat the 1536-byte owner budget and generated candidate buffers as fixed constraints; do not solve it by adding resident history.

## Acceptance checklist

- [ ] `GenerationUndoPayload` <= 1536 bytes
- [ ] receipt remains trivially copyable
- [ ] selected full-generation COMMIT uses `UndoKind::Generation`
- [ ] selected synth-reroll COMMIT uses the same owner
- [ ] COMMIT performs no generation/filesystem/wait/unbounded allocation
- [ ] generation commit advances Scene revision once
- [ ] failed/busy/cancelled preparation leaves mutation ownership unchanged
- [ ] 0.9.8 R2 owner tests PASS
- [ ] 0.9.8 R3 Pattern mutation tests PASS
- [ ] 0.9.9-A / Lo-Fi compatibility PASS
- [ ] Core host regressions PASS
- [ ] SDL build PASS
- [ ] Cardputer ADV build PASS
- [ ] fixed DRAM gate PASS
- [ ] SEQTRAK MIDI-only build PASS
- [ ] Pattern Editor `G` remains explicitly deferred to B2 with its existing generator semantics
- [ ] no Song/Phrase R4 safe-editing ownership introduced
- [ ] no physical-hardware PASS claimed from CI
