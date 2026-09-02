# Integrated GENERATE + SONG + Phrase Core acceptance

## Purpose

Validate the firmware state that combines:

- current `dev` behavior;
- four-axis `GENRE -> FEEL -> GENERATION -> TEXTURE` navigation;
- Song generation and split layouts;
- Phrase Core `A/B/C/D` reference-backed slots;
- transport-synchronised PERFORMANCE tools;
- UI-session restoration across workflows.

This checklist validates the integrated firmware as one product. It is not a replacement for the focused Phrase Core and four-axis test documents.

## Hardware list

- M5Stack Cardputer ADV, ESP32-S3FN8;
- USB-C data cable;
- headphones or built-in speaker;
- optional Yamaha SEQTRAK or another class-compliant USB-MIDI target for MIDI checks.

## Wiring

No external wiring is required.

- Use the Cardputer ADV USB-C port for flash, Serial, and USB MIDI.
- Keep PSRAM disabled.
- Use the `huge_app` partition profile selected by the repository build scripts.

## Build and flash

```bash
git fetch origin
git switch dev
git reset --hard origin/dev

bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Optional SEQTRAK MIDI-only build:

```bash
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

## Expected behavior

### 1. Workflow navigation

Navigate with `[` and `]` inside each workflow and `Fn+[` / `Fn+]` between workflows.

Expected order:

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL -> GENERATION -> TEXTURE
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS -> SYNTH A SOUND -> SYNTH B SOUND
SONG:     SONG -> PHRASE CORE
SETTINGS: PROJECT / SETUP
```

Leave GENERATE on TEXTURE and SONG on PHRASE CORE. Move to other workflows and return. The last page in each workflow must be restored. Reboot after persistence has had time to save; the active page and remembered workflow pages must remain valid.

### 2. Four-axis causality

#### GENRE

- Change genre/variant and apply `PROFILE ONLY`.
- Existing pattern material must not be silently regenerated.
- Apply `MATERIALIZE` and confirm audible pattern material changes.
- Apply `MATERIALIZE+BPM` and confirm BPM follows the selected corridor.

#### FEEL

- Change swing, timing humanize, and velocity humanize.
- Timing/velocity should change without replacing notes, roles, or texture.
- Browsing a preset without applying it must not create a Scene mutation.

#### GENERATION

- Select an empty Song row with arrows.
- `Left/Right` moves one row; `Up/Down` moves eight rows.
- Browsing must not move the transport position or mutate Song.
- Press `G` or `Enter` and confirm the selected row is materialized.
- An occupied/unsafe destination must be rejected without partial changes.

#### TEXTURE

- Change texture mode/amount and apply it.
- Sound surface should change.
- Notes, rhythm, FEEL settings, and Phrase references must remain unchanged.

### 3. Song generation

- On SONG, use `G` on a selected empty cell.
- Confirm real audible material is written into a safe free pattern slot.
- Double-tap `G` and confirm Synth A, Synth B, and Drums are committed for one row.
- Confirm another Song cell referencing existing material is unchanged.
- Fill all safe destination slots and confirm `NO EMPTY PATTERN SLOTS` leaves Scene unchanged.

### 4. Phrase Core

#### Capture and derive

1. Prepare a recognizable four-bar Song region.
2. Open PHRASE CORE.
3. Select A, choose `4B`, and press `Enter`.
4. Preview all four bars with `Left/Right`.
5. Select B, choose parent A with `P`, and press `D`.

Expected:

- A and B show valid IDs and metadata;
- B identifies A as its parent;
- all saved bars remain visible;
- the UI shows `REF MUT` / `REF LINKED`, never copied/owned wording.

#### REF MUTABLE refresh

1. Note A's selected-bar masks/energy.
2. Edit the exact referenced pattern in its pattern editor.
3. Commit the edit so the Scene revision changes.
4. Return to PHRASE CORE without recapturing.

Expected: masks/energy update. A stale preview is a failure.

#### Safe write and overwrite

1. Select an empty Song destination and press `W`.
2. Confirm selected Phrase tracks are written.
3. Select an occupied destination and press `W`.
4. Confirm rejection is atomic.
5. Record an unselected track reference, then press `Alt+W`.

Expected: selected tracks are overwritten and unselected tracks remain unchanged. No partial write is allowed.

#### Clear and persistence

1. Clear B with `Backspace`.
2. Confirm A remains valid and B is empty.
3. Save, reboot, and reload.

Expected: A survives and B remains empty. Cleared slots must not resurrect.

### 5. PERFORMANCE transport sync

- Enable NOTE mode and start Pattern/Song transport.
- Play direct notes; they must remain responsive.
- Test ARPEGGIATOR, RATCHET, and EUCLIDEAN while transport runs.
- Test dense CHORD/MEMORY plus RATCHET near a bar boundary.

Expected:

- tools stay phase-aligned with project transport;
- no burst of caught-up notes after a late frame;
- no end-of-bar stall;
- stopping or changing clock domain releases generated notes cleanly.

### 6. Themes and help

- Toggle `CARBON <-> CYBER` with `Alt+\`.
- Open `Alt+H` on GENRE, FEEL, GENERATION, TEXTURE, SONG, and PHRASE CORE.

Expected:

- all rows and footer hints fit the 240x135 display;
- help names the active page and its real controls;
- Phrase help describes `REFERENCE VIEW / REF MUTABLE` behavior;
- the first Synth/Drum rows and HUB track 1 retain their accepted spacing.

## Serial checks

During the run, confirm:

- no panic/reset loop;
- no repeated allocation failure;
- no continuously increasing audio underrun counter during ordinary navigation;
- UI-session save messages occur only after state changes and debounce;
- Scene revisions increase after successful mutations, not after navigation-only actions.

## Troubleshooting

### Phrase preview does not change

Verify that the referenced pattern edit was committed and incremented the Scene revision. Navigation alone does not invalidate the cache.

### `W` reports occupied destination

This is the safe path. Use another empty destination or deliberately use `Alt+W` after recording which unselected references must survive.

### GENERATION target changes playback position

This is a failure. Target browsing must remain UI-only until materialization succeeds.

### Performance tools drift or burst

Capture the active BPM, tool combination, bar number, and Serial diagnostics. Late generated NoteOn events should be dropped rather than replayed as a burst.

### Build exceeds fixed DRAM budget

Do not bypass the gate. Record `.dram0.data`, `.dram0.bss`, the configured budget, and the exact ELF path.

## Acceptance checklist

- [ ] Host regression suite passes.
- [ ] Cardputer ADV normal firmware builds with warnings enabled.
- [ ] Fixed DRAM gate passes on the produced ELF.
- [ ] MIDI-only firmware builds.
- [ ] Workflow page order is correct.
- [ ] GENERATE and SONG last-page restoration works in-session and after reboot.
- [ ] GENRE apply policies have visible and bounded causality.
- [ ] FEEL changes timing/velocity only.
- [ ] GENERATION target browsing is non-mutating and write is atomic.
- [ ] TEXTURE changes sound surface only.
- [ ] Song single-cell and row generation work without alias overwrite.
- [ ] Phrase A capture and B derivation work.
- [ ] REF MUTABLE preview updates without recapture.
- [ ] `W` occupied rejection is atomic.
- [ ] `Alt+W` overwrites selected tracks only.
- [ ] Phrase clear persists across save/reboot/load.
- [ ] Direct NOTE input works during transport.
- [ ] ARP/RATCHET/EUCLIDEAN follow transport without bar-end stalls.
- [ ] CARBON/CYBER and page-aware help remain readable.
- [ ] Serial shows no reset loop, allocation failure, or continuously growing underruns.
