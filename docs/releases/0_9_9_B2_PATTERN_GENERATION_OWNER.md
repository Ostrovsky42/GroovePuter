# GroovePuter 0.9.9-B2 — Pattern Generation Owner

## Purpose

Move Pattern Editor `G` from the remaining R3 legacy persistent-mutation path onto the canonical 0.9.8 `UndoOwner` without changing its musical generator.

Base: `dev_0.9.9` @ `d6db59c1fe3ddee0e9d611807508d6e44010e729`.

B2 completes the Pattern-side 0.9.9-B mutation handoff. It does not add Song/Phrase safe-editing, a second scheduler, pending persistence, or new generation recipes.

## Hardware list

Focused host acceptance requires no hardware.

Build acceptance uses the normal repository targets for:
- M5Stack Cardputer ADV / ESP32-S3.
- Cardputer ADV SEQTRAK MIDI-only firmware.

## Wiring

None. B2 changes no USB, MIDI, audio, I2C, display, or external-device wiring.

## Ownership contract

Pattern Editor `G` now follows:

`intent -> PREPARE scratch SynthPattern -> validate target -> UndoOwner COMMIT -> one Scene revision`

- PREPARE copies the current Synth Pattern and runs the same compiled Genre generator used by `MiniAcid::randomize303Pattern()`.
- Reggae keeps the existing bass/lead split: bass `0x1111`, lead `0xAAAA`, with the same motif and cluster settings.
- COMMIT uses the existing 116-byte `SynthPatternUndoPayload` with `UndoKind::Generation`; no new receipt type or resident history is added.
- The commit callback performs no generation, filesystem work, waiting, or unbounded allocation.
- While PLAY is active, the existing same-pattern selector is used before replacement so the legacy Pattern note-off behavior is retained. Selector changes remain runtime selection and do not dirty the Scene.
- A generated no-op does not publish a new receipt or revision.
- Pattern page Undo accepts only a `Generation` receipt whose payload size exactly matches `SynthPatternUndoPayload`, so it cannot reinterpret B1's larger full-generation receipt.
- Manual Pattern edits continue using `UndoKind::Pattern` exactly as in 0.9.8-R3.

## Build / Flash

Focused host gate:

```bash
mkdir -p build/host-tests

g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_pattern_generation_owner_0_9_9.cpp \
  -o build/host-tests/test_pattern_generation_owner_0_9_9
build/host-tests/test_pattern_generation_owner_0_9_9

python3 tests/test_pattern_generation_owner_0_9_9_source.py

bash tests/run_undo_0_9_8_r2_tests.sh
bash tests/run_undo_0_9_8_r3_tests.sh
```

Normal acceptance:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

For physical Cardputer ADV testing, flash the exact accepted B2 SHA using the repository's normal Cardputer ADV build/flash procedure. B2 itself adds no wiring requirements.

## Expected behavior

At STOP, pressing `G` on the Pattern Editor produces the same genre-aware synth Pattern family as before, but the persistent write is one canonical generation mutation and can be undone from the Pattern page.

During PLAY, B2 deliberately preserves the old immediate Pattern replacement/note-off behavior. Quantized audible activation belongs to 0.9.9-C and is not silently introduced here.

Undo after `G` restores the exact previous Synth Pattern and restores the Scene revision state according to the 0.9.8 owner contract.

## Troubleshooting

- Pattern `G` sounds structurally different from the previous build: compare the helper against `MiniAcid::randomize303Pattern()`; compiled Genre parameters and the Reggae split must remain identical.
- Revision increments twice: `PatternEditPage` must not call `markSceneMutated()` for `G`; only `UndoOwner::commitPrepared()` owns the transition.
- Undo reports kind mismatch after Pattern `G`: verify the B2 receipt is `UndoKind::Generation` with `sizeof(SynthPatternUndoPayload)`.
- Full B1 generation Undo is misread on Pattern page: the payload-size guard is missing or changed.
- Stuck note during PLAY generation: verify the bounded apply still calls the same-index `set303PatternIndex()` before restoring the prepared Pattern.
- Fixed-DRAM regression: B2 must reuse the existing 116-byte receipt and existing 1536-byte owner; do not add resident history.

## Acceptance checklist

- [ ] exact base is post-B1 `dev_0.9.9`
- [ ] Pattern `G` no longer calls `handleEventLegacyUnowned()`
- [ ] Pattern `G` no longer owns `markSceneMutated()`
- [ ] PREPARE operates on a scratch `SynthPattern`
- [ ] legacy compiled Genre parameters preserved
- [ ] legacy Reggae bass/lead split preserved
- [ ] existing `modeManager().generatePattern()` preserved
- [ ] COMMIT uses canonical `UndoOwner`
- [ ] receipt is `UndoKind::Generation`
- [ ] receipt remains 116 bytes
- [ ] one accepted `G` produces one revision transition
- [ ] no-op does not publish receipt/revision
- [ ] PLAY note-off behavior preserved
- [ ] Pattern-page Undo restores the previous Pattern
- [ ] B1 large Generation receipt cannot be misread as B2 Pattern receipt
- [ ] R2 UndoOwner tests PASS
- [ ] R3 Pattern mutation tests PASS
- [ ] 0.9.9-A / Lo-Fi compatibility PASS
- [ ] B1 generation-owner tests PASS
- [ ] Core host regressions PASS
- [ ] SDL build PASS
- [ ] Cardputer ADV build PASS
- [ ] fixed DRAM gate PASS
- [ ] SEQTRAK MIDI-only build PASS
- [ ] no Song/Phrase R4 ownership introduced
- [ ] no physical hardware PASS claimed from CI
