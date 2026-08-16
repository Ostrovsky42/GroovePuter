# 0.9.8 R4 — Song / Phrase safe editing

## Purpose

Migrate manual persistent Song and Phrase edits onto the one authoritative bounded `UndoOwner` established in 0.9.8 R2/R3. R4 does not add history depth and does not change generation or activation ownership.

## Contract

- **PREPARE is pure.** Song mutations are prepared on a copied `Song`; Phrase CAPTURE/DERIVE/CLEAR are prepared on a copied `PhraseBank`; Phrase WRITE/INSERT is prepared on a copied `Song`.
- **COMMIT is bounded.** One logical edit publishes one canonical receipt, applies one fixed before-prepared value under the existing audio guard, and advances Scene revision exactly once through `UndoOwner`.
- **Song receipt:** full Song before-image plus stable Song slot address. Active Song selection is not restored by Undo.
- **Phrase receipt:** full `PhraseBank` before-image. This intentionally restores `nextPhraseId` as well as A/B/C/D slot data.
- **One level only.** A newer successful persistent edit replaces the previous receipt. A later uncaptured revision expires it.
- **No second history owner.** The old vector-backed Song page history is removed; the retained legacy handler has only a zero-storage compatibility shim and cannot own an Undo payload.

## R4 scope

Included: manual Song cell assignment/clear/bank flip, row insert/delete, Cut/Paste/Reset, Song application Undo, Phrase CAPTURE/DERIVE/CLEAR, manual Phrase WRITE/INSERT to Song, Phrase application Undo.

Excluded: Song/Pattern generation, generated Phrase-to-Song materialization, Song reverse activation, playback/loop/slot selectors, persistent multi-level history, redo, persistence of Undo state. Those boundaries remain for later slices.

## Hardware assumptions

No new peripherals or wiring. R4 only changes edit-state ownership on the existing Cardputer ADV firmware. It reuses the resident 1536-byte Undo payload; the measured R2 ceiling is unchanged.

## Build / flash

```bash
bash tests/run_undo_0_9_8_r4_tests.sh
bash tests/run_host_tests.sh
bash scripts/build.sh
```

Flash the exact candidate SHA with the normal Cardputer ADV procedure only after software gates are green.

## Expected behavior

Manual Song or Phrase edit -> one Scene revision transition -> Undo restores exactly the previous persistent value. Changing Song/Phrase selector after the edit does not redirect the receipt. Phrase CAPTURE/DERIVE Undo also restores the phrase ID allocator. Generation and Song reverse continue using their pre-R4 paths.

## Troubleshooting

If Undo reports expired, check whether another persistent mutation advanced Scene revision after the captured edit. If Song Undo restores the wrong slot, verify the receipt `songSlot` rather than the current active selector. If Phrase IDs skip after Undo, verify the receipt is a full `PhraseBank`, not a single `PhraseSlot`.

## Acceptance checklist

- [ ] Focused R4 runner passes on the exact candidate SHA.
- [ ] Host, SDL, Cardputer ADV + fixed DRAM, and SEQTRAK MIDI-only gates pass on the same SHA.
- [ ] No `std::vector<UndoCell>` Song history remains.
- [ ] `PhraseWorkspace` does not call `markSceneMutated()` directly.
- [ ] Song edit -> Undo restores cells, length and reverse without changing active Song selector.
- [ ] Phrase CAPTURE/DERIVE -> Undo restores slot data and `nextPhraseId`.
- [ ] Phrase WRITE/INSERT -> Undo restores the target Song.
- [ ] Generation and reverse activation remain outside R4.
