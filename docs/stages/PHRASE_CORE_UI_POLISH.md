# Phrase Core UI polish — Cardputer ADV acceptance

## Purpose

Validate the UI-only follow-up stacked on `agent/phrase-core-foundation`.

The test confirms that the Phrase page presents the existing backend honestly and readably:

- four fixed slots `A/B/C/D`;
- saved role, length, source and storage mode;
- `REFERENCE VIEW / REF MUTABLE` ownership semantics;
- full 1/2/4/8-bar energy shape;
- selected-bar Synth A, Synth B and Drums occupancy masks;
- next-capture settings shown separately from saved Phrase metadata;
- CARBON, CYBER and AMBER visual styles.

This stage does not change Phrase domain, Scene persistence, dirty revision or Song write behavior.

## Hardware list

- M5Stack Cardputer ADV;
- USB-C data cable;
- computer with the existing GroovePuter Cardputer ADV toolchain;
- headphones or powered speaker for optional playback confirmation.

No external I2C or MIDI hardware is required.

## Wiring

1. Connect Cardputer ADV to the computer by USB-C.
2. For sound validation, connect headphones or a powered speaker to the normal audio output.
3. Leave PORT.A disconnected; this test does not use I2C.

## Build / flash

```bash
git fetch origin
git checkout agent/phrase-core-ui-polish
git reset --hard origin/agent/phrase-core-ui-polish

./scripts/build_cardputer_adv.sh
```

Flash the produced normal Cardputer ADV firmware using the same command/tool used for the current `dev` hardware tests.

Host checks used by CI:

```bash
python3 tests/test_phrase_ui_source_regressions.py
python3 tests/test_phrase_scene_source_regressions.py

mkdir -p build/host-tests
g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
  -Iplatform_sdl \
  -include platform_sdl/arduino_compat.h \
  -c src/ui/pages/phrase_page.cpp \
  -o build/host-tests/phrase_page.o
```

## Expected behavior

### 1. Open Phrase Core

1. Open the SONG workflow.
2. Press `[` or `]` until `PHRASE CORE` is shown.

Expected:

- the top row contains four slot tabs;
- an empty slot is shown as `A ---`, `B ---`, and so on;
- the selected slot has the strongest border/color;
- the page does not claim `COPIED`, `RECORDED` or `OWNED` for a reference slot.

### 2. Empty-slot state

Select an empty slot with `1..4`.

Expected:

- summary shows `EMPTY SLOT`;
- the next capture role and length are visible;
- source context shows current Song slot and row;
- all eight bar cells are visible, but only the selected capture length is active;
- `ENTER CAP` is shown at the bottom right.

### 3. Capture and saved metadata

1. Choose length using `Up/Down`.
2. Choose role using `R`.
3. Press `Enter` to capture the current bounded Song region.

Expected:

- slot tab changes from `---` to its saved role;
- summary shows saved role, saved length, source and `REF MUT`;
- the bottom `NEXT ...` line remains separate and describes the next capture settings;
- phrase ID, parent ID and active track mask are readable;
- a successful `CAPTURED <slot> #<id>` toast appears.

### 4. Energy-by-bar

Capture a four- or eight-bar region with visibly different density between bars.

Expected:

- the `BARS` strip contains one cell per possible bar, up to eight;
- bars inside the saved length are active;
- fill height represents each bar's bounded energy;
- the selected bar has an outer highlight;
- `Left/Right` moves only the selected preview bar and does not change saved length.

### 5. Per-bar masks

Move between preview bars with `Left/Right`.

Expected:

- `SA`, `SB` and `DR` rows each show a 16-step occupancy mask;
- stronger separators mark quarter-note boundaries;
- the pattern reference is shown to the right of each row;
- unresolved/missing tracks remain outlined and show `---` rather than fabricated data.

### 6. Reference semantics

After capturing a Phrase, edit one referenced pattern in the normal pattern editor and return to Phrase Core.

Expected:

- Phrase remains labelled `REF MUT` / `REF LINKED`;
- changed pattern occupancy is reflected after the Scene revision changes;
- the UI never describes the Phrase as an independent copied event container.

### 7. Themes

Cycle visual style with the existing global theme shortcut.

Expected in all three styles:

- CARBON: semantic green/cyan/amber accents on dark panels;
- CYBER: cyan/magenta/yellow/green slot accents;
- AMBER: readable amber-family palette with visible selection contrast;
- no text overlaps the right screen edge;
- all four slot tabs remain distinguishable.

### 8. Existing commands remain unchanged

Validate:

```text
1..4        Select slot
Up/Down     Next capture length
Left/Right  Preview bar
R           Role
P           Parent
Enter       Capture
D           Derive
W           Write to empty Song row
Alt+W       Explicit overwrite
Backspace   Clear slot
Alt+H       Page-aware help
```

Expected: behavior matches the Phrase Core foundation runbook.

## Troubleshooting

### Phrase page is not present

Confirm the checked-out branch:

```bash
git branch --show-current
git rev-parse HEAD
```

It must be `agent/phrase-core-ui-polish`, stacked on the accepted Phrase Core head.

### All energy bars are empty

- confirm the selected Phrase slot is valid;
- confirm its referenced patterns contain events;
- move through bars with `Left/Right`;
- check that the pattern page used by the Phrase still exists.

### `REF LINKED` does not update after pattern editing

The preview cache keys on Phrase ID, Scene revision and source page. Confirm the pattern edit increments Scene revision. Do not add a UI-local polling or raw Scene mutation workaround.

### Theme colors look identical

Confirm the global theme actually changed and the page header reflects the selected visual style. The Phrase page reads the runtime `UI::currentStyle`; it stores no separate theme state.

### Text is clipped

Record the exact theme, slot role, Phrase ID and source shown. The acceptance target is 240×135 with the standard header/footer and no external display scaling.

## Acceptance checklist

- [ ] Phrase page opens inside SONG workflow.
- [ ] Four slots fit on one row without clipping.
- [ ] Empty and valid slots are visually distinct.
- [ ] Saved metadata and `NEXT` capture settings are not mixed.
- [ ] `REF MUT` and `REF LINKED` are visible for reference-backed slots.
- [ ] Energy strip represents all saved bars up to eight.
- [ ] Left/Right moves the selected bar highlight.
- [ ] `SA/SB/DR` masks and references update with the selected bar.
- [ ] CARBON, CYBER and AMBER are readable.
- [ ] `Alt+H` remains correct.
- [ ] Capture, derive, write, overwrite and clear behavior is unchanged.
- [ ] No new heap allocation or independent event storage appears.
- [ ] Serial log shows no new boot, Scene codec or persistence errors.
