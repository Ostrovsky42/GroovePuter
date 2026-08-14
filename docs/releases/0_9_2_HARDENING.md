# GroovePuter 0.9.2 — Release Hardening Record

Status: **ACCEPTED / COMPLETE**

Runtime branch: `dev_0.9.2`

Hardware-tested P0/P1 head: `c3a06347bf6b07d19b038d678661c1e28c4c8e82`

Merged runtime freeze: `3b1187d74a1456ffc52ac9e940ba6dcfcfa0271b`

Production tree: `a99d9303924dcd9ab79be8f95146ed9de8869265`

The merged runtime freeze and the hardware-tested PR head have the same production tree. The merge therefore adds no untested runtime content.

## Purpose

0.9.2 is a hardening release for the 0.9.x product line. It closes the Song generated-pattern lifecycle defect before sampler recovery begins in the next development line.

No sampler recovery, arranger redesign, synth-engine expansion, transport rewrite, MIDI scheduler rewrite, Scene format expansion, or audio-thread storage I/O is part of 0.9.2.

## P0 — Song generated-pattern lifecycle

A resident pattern page contains `2 banks x 8 slots = 16 slots` per editable track. The project address space remains `16 pages x 16 slots = 256 global pattern IDs`, with one page resident in RAM at a time.

0.9.2 contract:

- Song-generated patterns are explicitly marked as Song-owned using an already-reserved step bit;
- Scene size and raw `.gpp` page layout do not grow;
- uniquely referenced generated cells reroll in place instead of burning another slot;
- shared generated sources remain copy-on-write;
- clearing a Song cell removes only the arrangement reference, preserving immediate Undo;
- unreferenced Song-generated orphans may be reclaimed by later Song generation;
- non-empty manual/imported patterns are never reclaimed automatically;
- referenced empty slots remain protected;
- failed multi-track materialization remains transactional;
- a genuinely full resident page is handled with existing `Alt+[` / `Alt+]` page navigation rather than unsafe synchronous SD scanning.

## P1 — UX / diagnostics

The canonical 0.9.2 key map documents:

- Song cell clearing with `Backspace`;
- generated orphan reuse;
- manual/imported pattern protection;
- resident-page capacity;
- `Alt+[` / `Alt+]` recovery when the current page is genuinely full.

`Clear Project` is not normal Song pattern-pool maintenance.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3.
- Existing SD card used by GroovePuter project/pattern storage.
- Yamaha SEQTRAK is optional for MIDI-only smoke testing.

## Wiring

No new wiring is required. Use the normal Cardputer ADV release configuration and existing USB/SD setup.

## Build / flash

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

On the Song page:

1. Generate a cell with `G`.
2. Reroll the same uniquely owned generated cell repeatedly; its global pattern reference remains stable.
3. Clear generated cells with `Backspace`; later Song generation may reuse their orphaned generated slots.
4. Sharing a generated pattern and rerolling one reference leaves the other unchanged.
5. Manual/imported non-empty patterns are not overwritten.
6. If the resident page is legitimately full, move to another page with `Alt+]` and continue generation.
7. Save, reboot and Load preserve usable Song/pattern lifecycle behavior.

## Troubleshooting

If Song generation reports no available slot:

- verify whether the current resident page is genuinely full of referenced/manual material;
- move to another pattern page with `Alt+[` / `Alt+]`;
- remember that manual/imported patterns are intentionally protected;
- do not use `Clear Project` merely to recover generated orphan capacity.

A failure after `Save -> reboot -> Load` is a release correctness defect, not expected behavior.

## Acceptance checklist

### Automated

- [x] `test_song_pattern_materializer` passes.
- [x] unique generated reroll keeps the same global pattern reference.
- [x] shared generated source uses copy-on-write.
- [x] deleted generated orphan can be reclaimed.
- [x] non-empty manual/imported material cannot be reclaimed.
- [x] referenced empty slot cannot be overwritten.
- [x] generator failure leaves Scene and dirty revision unchanged.
- [x] full Core regressions pass.
- [x] SDL build passes.
- [x] Cardputer ADV normal + fixed-DRAM build passes.
- [x] Cardputer ADV SEQTRAK MIDI-only build passes.
- [x] all 12 release workflows on `c3a06347bf6b07d19b038d678661c1e28c4c8e82` completed SUCCESS.

### Cardputer ADV

- [x] 20+ rerolls of one generated Song cell do not burn 20 pattern slots.
- [x] cleared generated cells yield reclaimable capacity without `Clear Project`.
- [x] shared generated pattern remains unchanged on the untouched reference after COW reroll.
- [x] manual pattern protection verified.
- [x] resident-page switch and generation on the next page verified.
- [x] playback across the page boundary verified.
- [x] `Save -> reboot -> Load` lifecycle smoke verified.

## Release boundary

0.9.2 runtime development is closed at `3b1187d74a1456ffc52ac9e940ba6dcfcfa0271b`, production tree `a99d9303924dcd9ab79be8f95146ed9de8869265`.

Only documentation/release metadata may follow before tagging. Any production-code change reopens acceptance and requires a new hardware-tested candidate.
