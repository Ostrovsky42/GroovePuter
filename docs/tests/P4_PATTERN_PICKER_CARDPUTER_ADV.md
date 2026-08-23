# 0.9.9-P4I — Song Pattern Picker / Audition — Cardputer ADV

## Purpose

Validate the production Song Pattern Picker introduced by 0.9.9-P4I.

The checkpoint adds two modes from one selected Song cell:

- **EXISTING** — audition already-persistent Pattern material read-only, then optionally assign it to the Song cell.
- **GENERATE** — generate into one temporary P1a2 PatternLease owned by the selected Song track, audition it, then accept or discard it.

`RELATED` has no producer in this checkpoint. It is documented as not implemented and is not mapped to EVOLVE or DERIVE.

Implementation stack base:

- `agent/20260823-06-0.9.9-p1a2-pattern-lease-generalization`
- `4c570acc0620ee4f1e03c01807ff3749957ecb0a`

P4R PR #350 is the research contract only and is not ancestry of P4I.

## Hardware

Required for hardware smoke:

- M5Stack Cardputer ADV / Stamp-S3A / ESP32-S3FN8
- USB-C data cable
- host with `arduino-cli` and repository Cardputer dependencies installed
- built-in Cardputer speaker or the project's normal audio output path

Optional:

- Yamaha SEQTRAK for the separate MIDI-only compile target; it is not required to exercise the Picker UI.

## Wiring

None.

P4I adds no GPIO, I2C, SPI, MIDI electrical, audio-routing, or voltage dependency. Use the Cardputer ADV exactly as for the existing GroovePuter firmware.

## Build / Flash

From repository root:

```bash
bash tests/run_0_9_9_p4i_tests.sh
bash tests/run_host_tests.sh
cd platform_sdl && make clean all CXX=g++
cd ..
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash the current source build:

```bash
bash scripts/upload.sh /dev/ttyACM0
```

If the Cardputer enumerates on another device, replace `/dev/ttyACM0` with that device path.

Do not use `scripts/upload.sh --prebuilt` for P4I validation; that flashes the retained release binary rather than this branch.

## Controls

Outside the Picker, existing Song controls are unchanged.

| Control | Song Page | Picker |
|---|---|---|
| `Enter` | Quick Jump to Pattern editor | Accept candidate |
| `Alt+Enter` | Open Picker for current single Song cell | already open / consumed |
| `Q..I` | Assign existing slot 1..8 at current PAT bank | Direct EXISTING slot 1..8 |
| `B` | Change Song PAT assignment bank A/B | Change EXISTING browse bank; GENERATE address stays pinned |
| `Alt+B` | Flip already-stored Song reference bank | consumed by modal |
| `Ctrl+B` | Switch Song playback slot A/B | consumed by modal |
| arrows | Song row/track navigation | Browse EXISTING candidates |
| `Tab` | Legacy clear-cell behavior | Toggle `EXISTING` / `GENERATE` |
| `G` | Legacy cell generation; double-G row generation | Reroll GENERATE candidate in the same lease |
| `Enter` | — | Accept |
| `Esc` or backtick | existing Song selection/back behavior | Discard temporary candidate and close |
| `Alt+H` | page-aware help | global help includes Picker controls |

The Picker uses the existing compact top overlay for bounded status updates. Candidate changes do not explicitly call a full Song-grid redraw.

Typical status:

```text
PICK EXIST A 1A1 Q-I/B/<> TAB ENTER ESC
PICK GEN A 1B4 T02 G:REROLL TAB ENTER ESC
```

The address format is the canonical `PAGE/BANK/SLOT` form such as `1A1`, `3B5`.

## Existing workflow

1. Open Song page.
2. Put the cursor on exactly one editable cell: Synth A, Synth B, or Drums.
3. Press `Alt+Enter`.
4. Picker opens in `EXISTING` mode.
5. Browse with arrows, `Q..I`, and `B`.
6. Listen to the candidate through the existing pattern playback/runtime selectors.
7. Press `Enter` to assign it, or `Esc` to close without assignment.

Expected ownership:

- no `PatternLease` is acquired;
- Pattern backing is never rewritten by browsing;
- Song is unchanged until `Enter`;
- `Enter` performs one canonical `commitSongMutation()` assignment;
- cancel creates no Song Undo receipt and no Scene Pattern mutation.

Existing browsing follows the currently resident Pattern page. `Alt+[ / ]` may be used to move to another Pattern page while no Generate lease is active; once the new page is resident, the next Picker browse action resolves candidates on that page.

## Generate workflow

1. Open Picker with `Alt+Enter` on one Song cell.
2. Press `Tab` to enter `GENERATE`.
3. P1a2 acquires exactly one address:

```text
count = 1
trackMask = selected Song track only
```

Examples:

```text
Synth A cell -> SynthA-only lease
Synth B cell -> SynthB-only lease
Drums cell   -> Drums-only lease
```

4. The existing musical producer semantics generate the candidate without assigning the Song cell.
5. Press `G` repeatedly to rerun candidate production.
6. The address shown by the Picker must remain identical across rerolls. `TAKE` increments; no second lease/address is allocated.
7. Press `Enter` to accept or `Esc` to discard.

The candidate writer touches only the leased track. If Synth A owns an address whose aligned Synth B or Drums backing already contains material, those unrequested tracks remain byte-for-byte under their existing ownership.

## Expected behavior

### EXISTING

- opening the Picker does not change the Song cell;
- browsing does not change Pattern bytes;
- candidate playback uses existing runtime pattern selectors, not a second sequencer;
- accept changes exactly the selected Song cell;
- discard restores previous runtime Song/pattern selection state;
- there is no temporary lease.

### GENERATE

- one active P1a2 lease owns one address and one track mask;
- candidate generation writes only the selected track;
- repeated `G` reuses the same lease/address;
- before accept, the persistent Song reference remains unchanged;
- discard clears only leased candidate backing and releases the lease;
- accept promotes backing into the existing Song-generated/reclaimable class and assigns the Song cell through canonical Song ownership.

### Legacy controls

After closing Picker, verify again:

- `Q..I` still assigns directly;
- plain `B` still changes PAT assignment bank;
- `Alt+B` still flips a stored reference bank;
- `Ctrl+B` still changes the playback Song slot;
- plain `G` still performs legacy current-cell generation;
- double `G` still performs the existing row transaction;
- normal Song A/B horizontal navigation is unchanged.

## Undo behavior

Accepted Picker assignment uses canonical Song Undo; P4I does not add another Undo owner.

For an accepted EXISTING candidate:

1. accept a different address;
2. press `Ctrl+Z` after Picker closes;
3. the old Song reference returns;
4. redo restores the accepted reference according to the existing global Undo direction behavior.

For an accepted GENERATE candidate:

1. accept generated candidate;
2. press `Ctrl+Z` after Picker closes;
3. only the Song reference is restored;
4. accepted generated Pattern backing remains intact;
5. it remains marked as existing Song-generated/reclaimable backing and may be reclaimed only by the existing reference-aware allocator rules;
6. redo can therefore restore the generated Song reference without its backing having been destroyed by Undo.

Do not expect Undo to clear an accepted generated Pattern merely because one Song reference disappeared.

## Page-pin behavior

A temporary GENERATE lease must never cross raw Pattern page persistence.

While a generated candidate is active:

1. note the current Pattern page and candidate address;
2. try `Alt+[` or `Alt+]`;
3. raw `PatternPagingService::savePage()` is fail-closed while any P1a2 lease is active;
4. `loadPage()` and backup restore are also rejected while leased;
5. the leased page/material remains resident and is not serialized as project state;
6. use `Enter` to accept or `Esc` to discard;
7. Pattern page switching works normally again after the lease is released/transferred.

A normally discarded preview therefore needs no serializer exclusion: discard returns its temporary bytes to canonical empty state before persistence is allowed again.

## Hardware smoke procedure

Use a project containing at least two visibly/audibly different existing patterns on each editable track and at least one safe empty Pattern slot for generation.

### A. Existing / cancel

1. Stop playback and select a populated Song Synth A cell.
2. Record its displayed address.
3. `Alt+Enter`.
4. Browse to another EXISTING address.
5. Press `Esc`.
6. Confirm the Song cell still displays the original address.
7. Start playback and confirm the pre-Picker Song arrangement still plays.

### B. Existing / accept / Undo

1. Reopen Picker on the same cell.
2. Select a different existing Pattern.
3. Press `Enter`.
4. Confirm only that Song cell changes.
5. Press `Ctrl+Z`; confirm the original address returns.
6. Trigger redo with the repository's current direction-aware Undo behavior; confirm the accepted address returns.

### C. Generate / one-track scope

1. Choose a Synth A Song cell on a page where the same aligned slot has recognizable Synth B and/or Drum material.
2. Open Picker, `Tab` to GENERATE.
3. Note generated address.
4. Press `G` three times.
5. Confirm the address never changes.
6. Exit Picker with `Esc`.
7. Open Synth B and Drums at that aligned slot and confirm their previous material remains unchanged.

Repeat once from a Drums Song cell to verify the inverse direction.

### D. Generate / accept / Undo

1. Generate a candidate and accept it.
2. Confirm the Song cell points at the leased address.
3. `Ctrl+Z`.
4. Confirm the old Song reference returns.
5. Do not expect the accepted generated Pattern bytes to disappear.
6. Redo and confirm the generated reference is usable again.

### E. Page pin

1. Open GENERATE and keep a generated candidate active.
2. Press `Alt+]`.
3. Confirm the Pattern page does not switch/save the leased preview.
4. Press `Esc`.
5. Press `Alt+]` again.
6. Confirm normal page switching resumes.

### F. Legacy regression

After Picker is closed, exercise `Q..I`, `B`, `Alt+B`, `Ctrl+B`, plain `G`, double `G`, left/right Song-slot crossing, and `Ctrl+Z`. Their pre-P4I behavior must remain intact.

## Troubleshooting

**`PICKER: SINGLE CELL`**

A Song range is selected. Clear the selection and place the cursor on one Synth A, Synth B, or Drums cell.

**`PICKER: PAGE BUSY`**

A Pattern page load/switch is already in progress. Finish that transition before opening/generating.

**`No safe pattern addresses`**

The selected track has no empty, unreferenced slot on the current page that P1a2 can lease. Existing/manual/referenced material is never overwritten to make room.

**`Pattern lease owner busy`**

Both bounded P1a2 owner records are already active. Do not add another allocator; resolve/release the existing audition owner.

**Page switch reports a save/load failure while GENERATE is open**

Expected page-pin safety behavior. Accept or discard the candidate first.

**`LEASE TRANSFER FAULT`**

This is not a recoverable UX condition. `completePersistentTransfer()` is required to be bookkeeping-only after a valid prepare and successful canonical Song commit. Treat this as a contract regression and reject the build.

**Generated reroll keeps the same musical result**

P4I intentionally preserves the existing producer/seed semantics and does not invent a new seed UI. The storage contract being tested is that reroll reuses the same leased address and does not grow the bank.

**RELATED is missing as a selectable mode**

Expected. There is no separate RELATED producer contract in P4I. Alt+H explicitly labels RELATED as not implemented rather than mapping it to EVOLVE/DERIVE.

## Acceptance checklist

- [ ] branch merge-base is P1a2 `4c570acc0620ee4f1e03c01807ff3749957ecb0a`
- [ ] P4R `396137993...` is not implementation ancestry
- [ ] `Alt+Enter` opens Picker from one Song cell
- [ ] modal consumes Picker keys without Song-page fallthrough
- [ ] Alt+H help documents Picker accurately
- [ ] RELATED has no producer and is not EVOLVE/DERIVE
- [ ] EXISTING browsing acquires zero PatternLeases
- [ ] EXISTING browsing writes zero Pattern bytes
- [ ] EXISTING cancel leaves Song unchanged
- [ ] EXISTING accept is one canonical Song mutation
- [ ] GENERATE acquires `count=1`
- [ ] GENERATE lease mask is exactly selected track
- [ ] SynthA generation leaves aligned SynthB/Drums untouched
- [ ] SynthB generation leaves aligned SynthA/Drums untouched
- [ ] Drums generation leaves aligned SynthA/SynthB untouched
- [ ] repeated GENERATE `G` reuses the same lease address
- [ ] failed Song commit leaves lease active
- [ ] discard clears only leased track backing
- [ ] accept order is prepare -> Song commit -> complete
- [ ] successful complete is bookkeeping-only
- [ ] accepted generated backing is Song-generated/reclaimable
- [ ] Song Undo restores reference without clearing accepted backing
- [ ] existing reference-aware reclaim rules remain authoritative
- [ ] active Generate lease blocks raw Pattern page save/load/restore
- [ ] no lease state is serialized
- [ ] no new allocator
- [ ] no new generator
- [ ] no new Undo owner
- [ ] no Phrase Lab / Phrase KEEP
- [ ] no EVOLVE / DERIVE / F08.1 implementation
- [ ] candidate changes do not explicitly redraw full Song grid
- [ ] legacy `Q..I` unchanged outside Picker
- [ ] legacy `B` / `Alt+B` / `Ctrl+B` unchanged outside Picker
- [ ] legacy `G` / double-G unchanged outside Picker
- [ ] Song A/B navigation unchanged outside Picker
- [ ] focused P4I suite PASS
- [ ] cumulative P1a/P1a2 suite PASS
- [ ] full host suite PASS
- [ ] SDL build PASS
- [ ] Cardputer ADV build PASS
- [ ] fixed DRAM check PASS
- [ ] SEQTRAK MIDI-only build PASS
- [ ] hardware smoke A-F PASS
- [ ] PR remains Draft and unmerged
