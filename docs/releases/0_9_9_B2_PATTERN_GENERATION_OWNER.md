# GroovePuter 0.9.9-B2 — Pattern Generation Owner

## Purpose

Close the remaining Pattern Editor generation ownership gaps after 0.9.9-B1 without changing musical generation semantics.

Base: `dev_0.9.9` @ `d6db59c1fe3ddee0e9d611807508d6e44010e729`.

The current Pattern page has two generation routes and B2 treats them separately:

1. **Plain unmodified `G`** already uses B1 `regenerateSynthWithQuantizedCommit()` and can return `PendingNextBar`. B2 removes the stale page-level second `markSceneMutated()` and wires its existing large Generation receipt into Pattern-page Undo.
2. **Legacy/fallback `G`** (the retained handler reached by modified commands) keeps `MiniAcid::randomize303Pattern()` musical semantics, but PREPARE now runs on a scratch `SynthPattern` and persistent publication uses the canonical `UndoOwner` with the existing compact `SynthPatternUndoPayload`.

B2 does not add Song/Phrase safe-editing, a second scheduler, pending persistence, or new generation recipes.

## Hardware list

Focused host acceptance requires no hardware.

Build acceptance uses the normal repository targets for:
- M5Stack Cardputer ADV / ESP32-S3.
- Cardputer ADV SEQTRAK MIDI-only firmware.

## Wiring

None. B2 changes no USB, MIDI, audio, I2C, display, or external-device wiring.

## Ownership contract

### Plain `G`

`intent -> B1 PREPARE -> B1 UndoOwner COMMIT or PendingNextBar`

- Keeps `regenerateSynthWithQuantizedCommit()` exactly as the generation owner.
- Keeps existing `PendingNextBar` behavior while PLAY is active.
- `UndoOwner::commitPrepared()` is the sole persistent revision owner; PatternPage no longer adds a second `markSceneMutated()` on `CommittedNow`.
- Pattern-page Undo recognizes the B1 Generation payload by `quantizedGenerationUndoPayloadSize()` and calls `undoLastQuantizedGeneration()`.

### Legacy/fallback `G`

`intent -> PREPARE scratch SynthPattern -> validate target -> UndoOwner COMMIT -> one Scene revision`

- PREPARE copies the current Synth Pattern and runs the same compiled Genre generator used by `MiniAcid::randomize303Pattern()`.
- Reggae keeps the existing bass/lead split: bass `0x1111`, lead `0xAAAA`, with the same motif and cluster settings.
- COMMIT uses the existing 116-byte `SynthPatternUndoPayload` with `UndoKind::Generation`; no new receipt type or resident history is added.
- The commit callback performs no generation, filesystem work, waiting, or unbounded allocation.
- While PLAY is active, the existing same-pattern selector is used before replacement so the legacy Pattern note-off behavior is retained. Selector changes remain runtime selection and do not dirty the Scene.
- A generated no-op does not publish a new receipt or revision.

### Receipt dispatch

Pattern-page Undo size-discriminates the two `UndoKind::Generation` payloads before decoding:
- B1 quantized Generation receipt: dispatched to `undoLastQuantizedGeneration()`.
- B2 compact fallback receipt: dispatched as `SynthPatternUndoPayload`.

Manual Pattern edits continue using `UndoKind::Pattern` exactly as in 0.9.8-R3.

## Build / Flash

Focused host gate:

```bash
mkdir -p build/host-tests

g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
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

At STOP, plain `G` keeps the B1 genre-aware quantized synth generation path and creates exactly one canonical Scene revision transition. Undo on the Pattern page restores that B1 Generation receipt.

The retained legacy/fallback `G` produces the same compiled-Genre/Reggae/mode-generator family as before, but its write is now a 116-byte canonical Generation mutation and can also be undone from the Pattern page.

During PLAY, plain `G` keeps B1 `PendingNextBar` behavior. The fallback legacy route deliberately keeps its historical immediate replacement/note-off behavior; unifying audible activation belongs to 0.9.9-C.

## Troubleshooting

- Plain `G` increments revision twice: PatternPage still owns a `markSceneMutated()` after B1 `CommittedNow`; remove the page-level transition.
- Plain `G` Undo reports kind mismatch: verify `quantizedGenerationUndoPayloadSize()` dispatch occurs before compact receipt dispatch.
- Fallback `G` sounds structurally different: compare `preparePatternEditorGeneration()` against `MiniAcid::randomize303Pattern()`; compiled Genre parameters and the Reggae split must remain identical.
- Compact Generation Undo fails: verify the receipt is `UndoKind::Generation` with `sizeof(SynthPatternUndoPayload)`.
- B1 and compact Generation receipts cross-decode: payload-size dispatch was removed or reordered.
- Stuck note on fallback PLAY generation: verify the bounded apply still calls same-index `set303PatternIndex()` before restoring the prepared Pattern.
- Fixed-DRAM regression: B2 must reuse the existing 116-byte receipt and existing 1536-byte owner; do not add resident history.

## Acceptance checklist

- [ ] exact base is post-B1 `dev_0.9.9`
- [ ] plain `G` still uses `regenerateSynthWithQuantizedCommit()`
- [ ] plain `G` still supports `PendingNextBar`
- [ ] plain `G` has no page-level `markSceneMutated()`
- [ ] plain `G` B1 receipt is reachable from Pattern-page Undo
- [ ] fallback `G` no longer calls `handleEventLegacyUnowned()` for its persistent write
- [ ] fallback `G` no longer owns `markSceneMutated()`
- [ ] fallback PREPARE operates on a scratch `SynthPattern`
- [ ] legacy compiled Genre parameters preserved
- [ ] legacy Reggae bass/lead split preserved
- [ ] existing `modeManager().generatePattern()` preserved
- [ ] fallback COMMIT uses canonical `UndoOwner`
- [ ] compact receipt is `UndoKind::Generation`
- [ ] compact receipt remains 116 bytes
- [ ] one accepted fallback `G` produces one revision transition
- [ ] fallback no-op does not publish receipt/revision
- [ ] fallback PLAY note-off behavior preserved
- [ ] B1 and B2 Generation receipts cannot cross-decode
- [ ] manual Pattern edits remain `UndoKind::Pattern`
- [ ] R2 UndoOwner tests PASS
- [ ] R3 Pattern mutation tests PASS with the post-B1/B2 generation handoff invariant
- [ ] 0.9.9-A / Lo-Fi compatibility PASS
- [ ] B1 generation-owner tests PASS
- [ ] Core host regressions PASS
- [ ] SDL build PASS
- [ ] Cardputer ADV build PASS
- [ ] fixed DRAM gate PASS
- [ ] SEQTRAK MIDI-only build PASS
- [ ] no Song/Phrase R4 ownership introduced
- [ ] no physical hardware PASS claimed from CI
