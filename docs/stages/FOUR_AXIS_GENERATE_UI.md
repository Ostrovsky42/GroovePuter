# Four-axis GENERATE UI — Cardputer ADV acceptance

## Purpose

Validate the fixed four-axis GENERATE workflow on M5Stack Cardputer ADV:

```text
GENRE -> FEEL -> GENERATION -> TEXTURE
```

Each page has one musical responsibility and one visible address:

- **GENRE** — corridor and vocabulary;
- **FEEL** — event timing and velocity relative to the grid;
- **GENERATION** — generative operators and single-bar materialization into Song;
- **TEXTURE** — sound surface only.

The test verifies layout and causal isolation. This stage adds no generator architecture, Scene field, persistence codec or DSP parameter structure.

`PHRASE CORE` is the sole UI owner of the selected `1/2/4/8`-bar working length. `GENERATION` does not expose a second phrase-length selector.

## Hardware list

- M5Stack Cardputer ADV;
- USB-C data cable;
- computer with the existing GroovePuter Cardputer ADV toolchain;
- headphones or powered speaker for FEEL/TEXTURE listening checks.

## Wiring

1. Connect Cardputer ADV to the computer using USB-C.
2. Connect headphones or a powered speaker to the normal audio output.
3. Leave PORT.A disconnected. This test does not use I2C or external encoders.

Cardputer ADV voltage and pin configuration are unchanged from `dev`.

## Build / flash

```bash
git fetch origin
git checkout agent/genre-feel-generation-texture-analysis
git reset --hard origin/agent/genre-feel-generation-texture-analysis

python3 tests/test_four_axis_ui_source_regressions.py
./tests/run_host_tests.sh
./scripts/build_cardputer_adv.sh
```

Flash the normal Cardputer ADV firmware using the same tool used for current `dev` hardware tests.

## Expected behavior

### 1. Fixed page order

Open GENERATE and press `[` / `]` repeatedly.

```text
GENRE 1/4
FEEL 2/4
GENERATION 3/4
TEXTURE 4/4
GENRE 1/4
```

`Fn+[ / ]` still changes workflow rather than axis page.

### 2. GENRE owns the corridor

On `GENRE 1/4`:

1. Change `GENRE`, `VARIANT` and `MORPH`.
2. Cycle `APPLY`: `PROFILE ONLY`, `MATERIALIZE`, `MATERIALIZE+BPM`.
3. Press `Enter`.

Expected:

- `PROFILE ONLY` changes the active genre/recipe without replacing patterns;
- `MATERIALIZE` regenerates once;
- `MATERIALIZE+BPM` also applies the genre BPM hint;
- no texture or FEEL control appears;
- FEEL values remain unchanged;
- selected and active genre/variant states are distinguishable.

### 3. FEEL owns timing and velocity

On `FEEL 2/4`:

1. Adjust `SWING`, `TIME HUMAN` and `VEL HUMAN`.
2. Browse `TIGHT/HUMAN/LOOSE` with `Left/Right`.
3. Press `Enter` on a preset.

Expected:

- browsing a preset does not mark Scene dirty;
- applying changes only swing, timing humanize and velocity humanize;
- note count, scale, ghost generation, phrase role and sound remain unchanged;
- no `GRID/TB/LEN` control is presented as FEEL;
- footer shows `ENTER:PRESET`.

### 4. GENERATION owns materialization operators

On `GENERATION 3/4`:

1. Confirm `SCOPE` is `CURRENT SONG ROW`.
2. Confirm no editable `LENGTH` row is present.
3. Press `Enter`, or press `G`.

Expected:

- one constructive pass writes exactly one bar to the shown Song target;
- the toast identifies the written Song row;
- the page states `Phrase length owned by PHRASE CORE`;
- only PHRASE CORE exposes the selected `1/2/4/8`-bar working length;
- no flavor, texture, swing or humanize control appears;
- no candidate scoring, retry animation or indefinite generation occurs;
- previous transport state is restored.

### 5. TEXTURE owns sound surface

On `TEXTURE 4/4`:

1. Select texture mode and `AMOUNT`.
2. Press `Enter` on `APPLY`.
3. Toggle `FLAVOR LINK` separately.

Expected:

- sound processing changes without note/rhythm/form changes;
- `FLAVOR LINK` is explicitly marked cross-axis;
- `DI AG SP WD IN GR DK` are read-only 0–127 projections for dirt, age, space, width, instability, aggression and darkness;
- selection updates the projection before apply;
- `ACTIVE` identifies the applied texture.

### 6. Page-aware Alt+H

Expected headings:

```text
=== GENRE 1/4 ===
=== FEEL 2/4 ===
=== GENERATION 3/4 ===
=== TEXTURE 4/4 ===
```

Each section states what the page does not change.

### 7. Themes

Use the public theme shortcut to test CARBON and CYBER. User-facing change: AMBER is no longer in the public shortcut cycle. If an existing saved session loads AMBER, verify compatibility there as well.

Expected:

- axis colors and focus remain readable;
- meters and macro strips remain inside 240×135;
- text does not overlap footer or right edge;
- labels and controls remain identical across themes.

### 8. Session restore

1. Leave GENERATE on FEEL, GENERATION or TEXTURE.
2. Change workflow and return.
3. Reboot after UI-session persistence completes.

Expected:

- the last GENERATE page is restored;
- FEEL is not restored as SETTINGS;
- SETTINGS contains only `PROJECT / SETUP`.

## Troubleshooting

### GENERATE still has three pages

```bash
git branch --show-current
git rev-parse HEAD
python3 tests/test_four_axis_ui_source_regressions.py
```

The source gate must report `PASS`.

### FEEL preset browsing creates dirty state

Only `Enter`/`Space` on the selected preset may mutate Scene. `Left/Right` on PRESET changes only the UI selection.

### GENRE changes sound unexpectedly

GENRE does not invoke texture or genre-timbre application. Verify `APPLY` mode and confirm no separate TEXTURE apply occurred.

### TEXTURE changes pattern events

This is a failure. Record before/after 16-step masks, texture mode and amount. TEXTURE must use only the existing texture path.

### GENERATION differs after apparently identical actions

This stage preserves existing RNG behavior and adds no retry/scoring. Record the complete action sequence and whether another generator operation advanced RNG state first.

### Text clips

Record theme, page, selected value and a full 240×135 screenshot. Do not use different domain labels per theme as a workaround.

## Acceptance checklist

- [ ] GENERATE cycles GENRE -> FEEL -> GENERATION -> TEXTURE.
- [ ] GENRE has no TEXTURE or FEEL controls.
- [ ] PROFILE ONLY keeps existing pattern material.
- [ ] FEEL edits only swing/timing/velocity humanize.
- [ ] FEEL preset browsing is non-mutating until apply.
- [ ] GENERATION materializes exactly one bar and exposes no phrase-length selector.
- [ ] PHRASE CORE is the sole UI owner of the selected 1/2/4/8-bar length.
- [ ] GENERATION exposes no flavor, texture or humanize controls.
- [ ] TEXTURE changes sound without note/rhythm/form changes.
- [ ] Seven texture macro indicators are readable and non-editable.
- [ ] `FLAVOR LINK` is visibly marked cross-axis.
- [ ] `Alt+H` matches each active page.
- [ ] CARBON and CYBER are readable at 240×135.
- [ ] Existing AMBER sessions remain readable if loaded.
- [ ] Last GENERATE page restores in-session and after reboot.
- [ ] Host tests and Cardputer ADV build pass.
- [ ] Serial output has no new Scene/session persistence errors.
