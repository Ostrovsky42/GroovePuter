# 0.9.9-IUI1 Pattern Picker / PatternLease Integration

## Purpose

Prove that the existing Song Pattern Picker composes with the single P1a2
`PatternLeaseOwner` through EXISTING and GENERATE cancel/accept/Undo lifecycles.
IUI1 adds acceptance evidence only; it does not add a second allocator, history
owner, cleanup policy, or production behavior.

## Hardware

- M5Stack Cardputer ADV
- USB cable for flash and serial
- No external unit required

## Wiring

No wiring changes. Use the Cardputer ADV's normal USB connection.

## Build and flash

Run the focused host proof first:

```bash
bash tests/run_0_9_9_iui1_tests.sh
```

Build the Cardputer ADV firmware and verify its fixed DRAM budget:

```bash
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

Flash only the artifact produced from the final IUI1 SHA using the repository's
normal Cardputer ADV upload procedure. Hardware acceptance remains pending until
that exact SHA is exercised.

## Expected behavior

- Alt+Enter opens one Picker for the focused Song cell without changing page.
- EXISTING browse previews persistent Patterns; Esc changes no Song reference,
  Pattern bytes, lease count, or Undo receipt.
- EXISTING Enter commits one canonical Song mutation; Undo/Redo exchange only
  the Song reference and preserve existing Pattern backing.
- Tab enters GENERATE and acquires one selected-track lease. Repeated G rerolls
  at the same physical address without growing the active lease count.
- Esc clears temporary owned bytes, returns active lease count to zero, and
  leaves no Song reference or Undo receipt.
- Enter performs `preparePersistentTransfer`, canonical `commitSongMutation`,
  then `completePersistentTransfer`. Generated backing remains present and the
  lease becomes inactive.
- Save, load, and backup restore fail closed while a temporary lease is active.
- Q..I, B, Alt+B, Ctrl+B, plain G, double-G, Song navigation, Alt+H, canonical
  Undo, copy-on-write, and free-slot behavior remain unchanged outside modal.

## Troubleshooting

- `OWNER FULL` means the fixed two-record owner is already occupied; closing
  the active audition must discard its lease before retrying.
- `LEASE PAGE PIN` or a rejected page operation is expected while GENERATE owns
  temporary backing. Cancel or accept before changing pages.
- `GENERATION FAILED` must leave the active lease recoverable; Esc must still
  discard it.
- `ACCEPT FAILED` must not release the lease. Retry or cancel; do not serialize
  temporary backing.
- If Alt+Enter does nothing, confirm the exact final SHA and inspect `[KEY]` for
  an Alt-modified Enter event before treating it as Picker lifecycle failure.

## Acceptance checklist

- [ ] Exact final IUI1 SHA recorded before build/flash
- [ ] EXISTING cancel leaves Song, Pattern bytes, lease count, and Undo unchanged
- [ ] EXISTING accept creates one canonical Song mutation; Undo/Redo pass
- [ ] GENERATE acquire reports one lease with selected-track mask
- [ ] Reroll keeps the exact lease address and count
- [ ] GENERATE cancel clears temporary bytes and reuses the address repeatedly
- [ ] GENERATE accept preserves backing and releases temporary ownership
- [ ] Failed prepare/commit leaves lease safely recoverable
- [ ] Repeated cancel/accept is guarded
- [ ] Page save/load/restore are pinned while the lease is active
- [ ] P1a, P1a2, P4I, P4I-01, IUI1, full host, SDL, Cardputer, DRAM, SEQTRAK pass
- [ ] Hardware acceptance recorded separately on the exact final SHA
