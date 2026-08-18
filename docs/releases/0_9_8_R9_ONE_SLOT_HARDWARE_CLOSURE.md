# GroovePuter 0.9.8-R9 — One-Slot Undo/Redo Hardware Closure

## Status

R9 supersedes R8 as the final 0.9.8 hardware-acceptance candidate.

R8 established global `Ctrl+Z` and removed the TB303 shortcut collision. Physical testing then exposed three gaps that software-only R8 acceptance did not prove:

- TB303 `Ctrl+A/X/C/V` reset chords were not reliable on Cardputer ADV;
- DRUMS/rhythm generation did not participate in the public Undo path;
- GENERATION/GENRE materialization did not participate in the public Undo path.

The desired product behavior is deliberately bounded: **one reversible history slot**, not a conventional Undo/Redo stack.

Exact base: `dev_0.9.8 @ 09d095e10589708a47bd535833166adc4e45d72a`.

The exact R9 hardware SHA must be filled from the software-green PR head immediately before flashing. Never accept a moved branch name as hardware evidence.

## Product contract

```text
successful persistent mutation
        |
        v
CURRENT = B        RETAINED = A

Ctrl+Z
        |
        v
CURRENT = A        RETAINED = B

Ctrl+Z again
        |
        v
CURRENT = B        RETAINED = A
```

Repeated `Ctrl+Z` toggles the same logical mutation between its two states until another successful persistent mutation replaces the slot.

A new successful mutation always starts a new pair:

```text
A <-> B

new mutation B -> C

old A side is forgotten
new retained pair is B <-> C
```

There is intentionally:

- one fixed retained payload only;
- no multi-level history;
- no separate redo stack;
- no `Ctrl+Y`;
- no persisted Undo history across reboot;
- no full-Scene snapshot history.

`UndoOwner` remains inside the existing 1552-byte R2 resident budget.

## Global UX

- `Ctrl+Z` is the single public history gesture.
- The gesture is promoted to `GROOVEPUTER_APP_EVENT_UNDO` before active-page dispatch.
- The owning page/domain performs the bounded exchange.
- A wrong page does not consume the receipt and reports `UNDO: RETURN PAGE`.
- Empty history reports `UNDO: EMPTY`.
- `Ctrl+U` is not a public Undo alias.

The internal owner tracks whether the next exchange is logically Undo or Redo. Some domains expose that distinction in their toast text; the state-machine contract does not depend on the toast wording.

## Covered persistent domains

### Pattern

Ordinary persistent Pattern edits exchange the complete retained Pattern before/after image through the authoritative 0.9.8 owner.

### Song

Ordinary Song arrangement edits exchange the retained Song before/after image atomically.

Generated/materialized Song-cell ownership is still a separate later lifecycle concern and is not an R9 acceptance target.

### Phrase

Ordinary Phrase persistent edits exchange their retained Phrase state. Phrase-to-Song writes continue to use Song ownership where applicable.

### DRUMS manual edits

Persistent drum-pattern edits use a bounded `DrumPatternUndoPayload` and the same one-slot exchange contract.

### DRUMS plain G generation

Plain DRUMS `G` keeps the existing drums-only Strong Rhythm musical semantics, but publication now goes through the canonical bounded generation COMMIT. This creates one Generation receipt and one Scene revision transition instead of a page-local/direct mutation with no public history.

`Ctrl+G` voice-local randomize and `Alt+G` chaos remain separate destructive/editor commands. `Ctrl+Alt+G` remains the explicit multi-bar audition path.

### GENRE / GENERATION materialization

Successful quantized materialization publishes a canonical Generation receipt. The GENRE page exchanges that retained generation state through the same global `Ctrl+Z` gesture.

R9 does not introduce a second scheduler or a second generation history owner.

## TB303 Cardputer chord closure

TB303 resets are:

- `Ctrl+A` — Cutoff -> 800 Hz;
- `Ctrl+X` — Resonance -> 0;
- `Ctrl+C` — Env Amount -> 400;
- `Ctrl+V` — Env Decay -> 420.

The handler accepts both forms Cardputer/host input may produce:

- control-character key values `1..26`, normalized back to `a..z`;
- physical scancodes `GROOVEPUTER_A/X/C/V`.

This is specifically an input-compatibility fix. Source presence is not hardware acceptance; all four chords must be exercised on the physical Cardputer ADV.

## Software gates

Focused cumulative contract:

```bash
bash tests/run_undo_0_9_8_r9_tests.sh
```

The same exact head must also pass:

- Core host regressions;
- SDL build;
- Cardputer ADV normal compile;
- fixed-DRAM policy;
- Cardputer ADV SEQTRAK MIDI-only compile;
- inherited generation / tonal / sampler / device-profile gates triggered by the PR.

Do not call R9 software-green from the focused gate alone.

## Physical Cardputer ADV acceptance

Flash only the exact software-green SHA recorded in PR #331.

### 1. Pattern one-slot toggle

1. Make one obvious ordinary Pattern edit.
2. Press `Ctrl+Z`: previous Pattern state must return.
3. Press `Ctrl+Z` again: edited state must return.
4. Press `Ctrl+Z` once more: previous state must return again.
5. Make a different persistent edit.
6. The old pair must be gone; `Ctrl+Z` now toggles only the new mutation.

### 2. Song one-slot toggle

Repeat the same before -> after -> before sequence using an ordinary Song assignment/edit. The complete Song mutation must exchange atomically.

### 3. Phrase one-slot toggle

Repeat with an ordinary persistent Phrase edit.

### 4. DRUMS manual edit

Make an obvious drum-pattern edit, then verify `Ctrl+Z` toggles old <-> edited repeatedly.

### 5. DRUMS plain G generation

1. Start from an easily recognizable drum pattern.
2. Press plain `G` once.
3. Confirm a new drums-only generated rhythm is audible/visible.
4. Press `Ctrl+Z`: the exact previous drum pattern must return.
5. Press `Ctrl+Z` again: the generated rhythm must return.
6. Press plain `G` again: this new generation must replace the old history pair.

This is the physical regression that R8 did not cover.

### 6. GENRE / GENERATION materialization

Test first with transport STOP to remove BAR_START timing from the acceptance question:

1. Record/recognize the current material.
2. Trigger one GENRE/GENERATION materialization with `G`.
3. Confirm generated material changed.
4. `Ctrl+Z` must restore the exact previous material.
5. `Ctrl+Z` again must restore the generated material.
6. A new generation must replace the previous history pair.

PLAY/pending-boundary lifecycle belongs to the later activation acceptance and must not be used to hide a STOP-state failure here.

### 7. TB303 reset chords

On TB303 KNOBS/MORE, deliberately move each value away from its reset target, then verify physically:

- `Ctrl+A` -> Cutoff 800;
- `Ctrl+X` -> Resonance 0;
- `Ctrl+C` -> Env Amount 400;
- `Ctrl+V` -> Env Decay 420.

Also verify ordinary unmodified `A/Z`, `S/X`, `D/C`, `F/V` quick-adjust behavior still works.

### 8. Wrong page / empty history

- Retain a valid receipt, navigate to a page that does not own it, press `Ctrl+Z`: show `UNDO: RETURN PAGE` and keep the receipt.
- Return to the owning page and verify the same receipt still toggles.
- With no valid receipt, `Ctrl+Z` must show `UNDO: EMPTY`.

### 9. Stability

Any reboot, Guru Meditation, stack canary, watchdog, stuck-note side effect, or obvious timing collapse during the smoke is a FAIL.

After the functional matrix passes, run the normal short soak before promotion.

## Promotion rule

R9 may become the **FINAL 0.9.8 SHA** only when all of the following refer to one exact immutable commit:

- cumulative R1-R9 Safe Editing gates PASS;
- full Core/SDL/Cardputer/SEQTRAK software matrix PASS;
- physical Pattern/Song/Phrase/DRUMS/GENERATION toggle smoke PASS;
- physical TB303 `Ctrl+A/X/C/V` smoke PASS;
- stability smoke/soak PASS.

Only then merge/freeze 0.9.8 and use that exact accepted lineage as the base for the subsequent 0.9.9 delta/conflict/ownership audit.
