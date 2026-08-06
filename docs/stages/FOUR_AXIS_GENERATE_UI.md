# Four-axis GENERATE UI — Cardputer ADV acceptance

## Purpose

Validate the fixed four-axis GENERATE workflow on M5Stack Cardputer ADV:

```text
GENRE -> FEEL -> GENERATION -> TEXTURE
```

Each page must have one musical responsibility and one visible address:

- **GENRE** — corridor and vocabulary;
- **FEEL** — event timing and velocity relative to the grid;
- **GENERATION** — phrase form and materialization into Song;
- **TEXTURE** — sound surface only.

The test verifies both layout and causal isolation. No generator architecture, Scene field, persistence codec or DSP parameter structure is added by this stage.

## Hardware list

- M5Stack Cardputer ADV;
- USB-C data cable;
- computer with the existing GroovePuter Cardputer ADV build toolchain;
- headphones or powered speaker for hearing FEEL/TEXTURE differences.

## Wiring

1. Connect Cardputer ADV to the computer using USB-C.
2. Connect headphones or a powered speaker to the normal audio output.
3. Leave PORT.A disconnected. This test does not use I2C or external encoders.

Cardputer ADV internal voltage and pin configuration are unchanged from `dev`.

## Build / flash

```bash
git fetch origin
git checkout agent/genre-feel-generation-texture-analysis
git reset --hard origin/agent/genre-feel-generation-texture-analysis

python3 tests/test_four_axis_ui_source_regressions.py
./tests/run_host_tests.sh
./scripts/build_cardputer_adv.sh
```

Flash the produced normal Cardputer ADV firmware using the same command/tool used for current `dev` hardware tests.

## Expected behavior

### 1. Fixed page order

1. Open the GENERATE workflow.
2. Press `[` / `]` repeatedly.

Expected order:

```text
GENRE 1/4
FEEL 2/4
GENERATION 3/4
TEXTURE 4/4
GENRE 1/4
```

`Fn+[ / ]` must still change workflow rather than axis page.

### 2. GENRE owns the corridor

On `GENRE 1/4`:

1. Change `GENRE` and `VARIANT` with arrows.
2. Change `MORPH`.
3. Cycle `APPLY` between `PROFILE ONLY`, `MATERIALIZE` and `MATERIALIZE+BPM`.
4. Press `Enter`.

Expected:

- `PROFILE ONLY` changes the active genre/recipe contract without replacing current pattern material;
- `MATERIALIZE` regenerates pattern material once;
- `MATERIALIZE+BPM` also applies the genre BPM hint;
- no texture selection appears on this page;
- FEEL values do not change when applying GENRE;
- the active genre/variant readout clearly differs from an un-applied selection.

### 3. FEEL owns timing and velocity

On `FEEL 2/4`:

1. Adjust `SWING`.
2. Adjust `TIME HUMAN`.
3. Adjust `VEL HUMAN`.
4. Browse `TIGHT/HUMAN/LOOSE` with `Left/Right` without applying.
5. Press `Enter` on a preset.

Expected:

- browsing a preset does not mark the Scene dirty;
- applying a preset changes only swing, timing humanize and velocity humanize;
- note count, scale, ghost generation probability, phrase role and sound surface remain unchanged;
- the page contains no `GRID/TB/LEN` controls presented as FEEL;
- the footer states `ENTER:PRESET`.

### 4. GENERATION owns form and materialization

On `GENERATION 3/4`:

1. Select `LENGTH` and cycle `1/2/4/8 BARS`.
2. Observe the read-only `PLAN` row.
3. Move to `MATERIALIZE` and press `Enter`, or press `G`.

Expected:

- one constructive generation pass writes the requested bars into the displayed Song target;
- the result toast identifies the materialized Song range;
- the page exposes no flavor, sound macro, swing or humanize controls;
- no candidate scoring, retry animation or indefinite generation occurs;
- previous transport state is restored after generation.

### 5. TEXTURE owns sound surface

On `TEXTURE 4/4`:

1. Select a texture mode.
2. Adjust `AMOUNT`.
3. Press `Enter` on `APPLY`.
4. Toggle `FLAVOR LINK` separately.

Expected:

- applying texture changes sound processing only;
- notes, pattern occupancy, phrase length and Song structure remain unchanged;
- `FLAVOR LINK` is explicitly labelled as a cross-axis link and defaults to the persisted Scene value;
- the seven compact bars `DI AG SP WD IN AT DK` are read-only 0–127 projections;
- changing a texture mode updates the macro projection before apply, while `ACTIVE` identifies the currently applied mode.

### 6. Page-aware Alt+H

Press `Alt+H` on each page.

Expected headings:

```text
=== GENRE 1/4 ===
=== FEEL 2/4 ===
=== GENERATION 3/4 ===
=== TEXTURE 4/4 ===
```

Each help section must state what the page does **not** change.

### 7. Themes

Cycle CARBON, CYBER and AMBER with the existing global theme shortcut.

Expected:

- the four axis colors remain distinguishable;
- focused rows remain readable;
- meters and macro strips stay inside 240x135 bounds;
- no text overlaps the footer or right edge;
- all three themes preserve the same labels and controls.

### 8. Session restore

1. Leave GENERATE on FEEL, GENERATION or TEXTURE.
2. Change to another workflow and return.
3. Reboot after Scene/UI session persistence has completed.

Expected:

- the last used GENERATE page is restored;
- FEEL is no longer restored as a SETTINGS page;
- SETTINGS contains only `PROJECT / SETUP` in this stage.

## Troubleshooting

### GENERATE still has three pages

Confirm the branch and head:

```bash
git branch --show-current
git rev-parse HEAD
```

Then run:

```bash
python3 tests/test_four_axis_ui_source_regressions.py
```

The source gate must report `PASS`.

### FEEL preset browsing creates a dirty marker

Only pressing `Enter`/`Space` on the selected preset may mutate the Scene. Plain `Left/Right` on the PRESET row must change only the UI cursor.

### GENRE changes the sound unexpectedly

GENRE no longer invokes texture or genre-timbre application. Verify `APPLY` mode and confirm no separate TEXTURE apply was performed. Report the selected genre, active genre and the exact synth engine.

### TEXTURE changes pattern events

This is a failure. Capture before/after 16-step masks and report the selected texture and amount. TEXTURE must call only the existing texture application path.

### GENERATION appears random after identical actions

This stage preserves the existing generation seed behavior; it does not add retry or scoring. Record the exact sequence of actions and whether another generator operation advanced RNG state beforehand.

### Page text clips in one theme

Record:

- theme;
- page;
- selected value;
- screenshot of the full 240x135 display.

Do not solve clipping by shortening domain names differently per theme.

## Acceptance checklist

- [ ] GENERATE cycles through GENRE -> FEEL -> GENERATION -> TEXTURE.
- [ ] GENRE has no texture or FEEL controls.
- [ ] PROFILE ONLY keeps existing pattern material.
- [ ] FEEL edits only swing/timing/velocity humanize.
- [ ] FEEL preset browsing is non-mutating until apply.
- [ ] GENERATION materializes 1/2/4/8 bars into the shown Song target.
- [ ] GENERATION exposes no flavor, texture or humanize controls.
- [ ] TEXTURE changes sound without changing note/rhythm/form data.
- [ ] Seven texture macro indicators are readable and non-editable.
- [ ] `FLAVOR LINK` is visibly marked cross-axis.
- [ ] `Alt+H` matches each active page.
- [ ] CARBON, CYBER and AMBER remain readable at 240x135.
- [ ] Last GENERATE page restores within the session and after reboot.
- [ ] Host tests and Cardputer ADV build pass.
- [ ] Serial output contains no new Scene/session persistence errors.
