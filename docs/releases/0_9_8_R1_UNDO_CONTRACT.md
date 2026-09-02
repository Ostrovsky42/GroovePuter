# GroovePuter 0.9.8-R1 — Bounded Undo Receipt Contract

## Purpose

Introduce the smallest reusable primitive needed by 0.9.8 Undo / Safe Editing without changing any user-visible behavior.

R1 defines a fixed-capacity, one-level retained receipt slot:

```text
BoundedUndoSlot<N>
```

It stores:

- one mutation kind;
- one trivially-copyable before-state payload;
- the exact `SceneRevisionState` that existed before the mutation.

R1 deliberately does **not** create a global instance and therefore does not reserve a production Undo payload budget yet. R2 must measure real Pattern/Song/Phrase receipt shapes before choosing that budget.

## Baseline

Production candidate parent:

```text
release/0.9.7-final
3341df09900098b649a4300696c0883fc0c14d61
```

0.9.7 is still completing its final release-integration gate. Before 0.9.8 production integration, this R1 stack must be rebased/delta-audited onto the exact accepted 0.9.7 merge SHA.

## Contract

### One-level semantics

A successful publish replaces the previous retained receipt:

```text
edit A -> receipt A
edit B -> receipt B
```

Undo history is intentionally one level at this stage.

### Admission before overwrite

A receipt is published only if:

- the mutation kind is not `None`;
- the payload fits the compile-time capacity;
- the payload type is trivially copyable.

Admission happens before retained bytes are touched.

Therefore:

```text
valid Undo A
    ↓
failed / oversized mutation B
    ↓
Undo A remains available
```

This is a required 0.9.8 safety invariant.

### Revision ownership

Each retained receipt stores the exact pre-mutation:

```text
SceneRevisionState
```

This allows later R2+ integration to distinguish:

```text
clean -> edit -> Undo -> clean
```

from:

```text
already dirty -> edit -> Undo -> still dirty
```

R1 stores the revision state but does not yet perform Scene restoration.

### No heap / no runtime dependencies

`BoundedUndoSlot<N>` uses inline `std::array` storage and `memcpy` of fixed values.

It does not depend on:

- `Scene`;
- `SceneManager`;
- UI pages;
- `AudioGuard`;
- MIDI;
- filesystem / SD;
- Arduino runtime;
- JSON;
- `std::vector` / `std::deque`;
- heap allocation.

### No global owner yet

R1 intentionally provides the bounded primitive but does not instantiate it globally.

This keeps production fixed DRAM unchanged until R2 measures the actual receipt candidates and selects the smallest sufficient capacity.

## Files

```text
src/state/bounded_undo_slot.h
tests/test_bounded_undo_slot_0_9_8.cpp
tests/test_bounded_undo_slot_0_9_8_source_regressions.py
tests/run_undo_0_9_8_r1_tests.sh
.github/workflows/undo-0-9-8-r1.yml
docs/releases/0_9_8_R1_UNDO_CONTRACT.md
```

## Hardware list

R1 focused contract:

- no hardware required.

Release regression build:

- M5Stack Cardputer ADV / ESP32-S3;
- normal no-PSRAM product profile;
- Yamaha SEQTRAK only for inherited MIDI/output regression acceptance.

## Wiring

No new wiring.

Cardputer ADV PORT.A remains unchanged and unrelated to this feature:

```text
SDA GPIO2
SCL GPIO1
```

## Build / test

Focused host contract:

```bash
bash tests/run_undo_0_9_8_r1_tests.sh
```

Expected final line:

```text
0.9.8 R1 bounded Undo contract: PASS
```

Normal repository regression gates remain required before accepting the checkpoint:

```bash
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

SDL remains part of the normal Core matrix.

## Expected behavior

### Screen

No new key binding, status label, menu entry, page or popup.

### Serial

No new boot/runtime logging.

### Runtime

No production Undo owner exists yet. Existing Song local Undo and provisional generation rollback behavior remain unchanged.

### Focused test

The executable contract proves:

- empty state;
- successful publish/read round-trip;
- exact revision snapshot round-trip;
- second successful mutation replaces the first receipt;
- wrong-kind read does not consume history;
- oversized admission fails without destroying previous history;
- `None` admission fails without destroying previous history;
- explicit clear removes the retained receipt;
- fixed-capacity slot metadata remains bounded.

The source regression proves:

- no heap containers or allocation APIs;
- no Scene/AudioGuard/UI/MIDI ownership leak;
- admission occurs before payload overwrite;
- no global `BoundedUndoSlot` instance is introduced in R1.

## Troubleshooting

### Focused compile fails on payload triviality

Do not weaken the `std::is_trivially_copyable` requirement. A production Undo receipt should be redesigned as a bounded value rather than placing dynamic ownership inside retained history.

### A future receipt does not fit

Do not silently increase a global buffer. R2 must first measure the actual candidate payload and Cardputer ADV fixed-DRAM delta.

### Someone proposes `sceneTransactionScratch()` for Undo

Reject the change. That full-Scene object is shared transaction scratch for loading/validation and can be overwritten by unrelated operations. Retained user history needs independent lifetime ownership.

### Existing Song Undo changes in R1

Reject the checkpoint. R1 is contract-only and must not alter current user-visible Song behavior.

## Acceptance checklist

- [ ] exact parent SHA recorded;
- [ ] focused R1 runner PASS;
- [ ] slot is compile-time fixed-capacity;
- [ ] no heap allocation or dynamic container in retained slot;
- [ ] only trivially-copyable payloads accepted;
- [ ] failed/oversized publish leaves previous Undo untouched;
- [ ] successful publish replaces previous one-level history;
- [ ] exact pre-mutation `SceneRevisionState` retained;
- [ ] no global Undo instance yet;
- [ ] no Scene/persistence schema change;
- [ ] no Song/Phrase/Pattern behavior change;
- [ ] no UI/keybinding change;
- [ ] no Output Ownership change;
- [ ] no Device Profile change;
- [ ] Core/host PASS;
- [ ] SDL PASS;
- [ ] Cardputer ADV compile PASS;
- [ ] fixed DRAM gate PASS;
- [ ] SEQTRAK MIDI-only compile PASS.

## Next checkpoint

0.9.8-R2 should measure and freeze the first real receipt shape, then instantiate one authoritative owner and migrate one simple Pattern destructive edit.

R2 must report at minimum:

```text
sizeof(SynthPattern)
sizeof(DrumPatternSet)
sizeof(Song)
sizeof(PhraseSlot)
sizeof(PhraseBank)
chosen Undo payload capacity
sizeof(authoritative Undo owner)
fixed DRAM delta
```

Do not add Song/Phrase migration or a global shortcut until that first vertical slice is green.
