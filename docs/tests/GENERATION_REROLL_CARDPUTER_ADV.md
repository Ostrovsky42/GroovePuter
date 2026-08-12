# Generation reroll / MORPH migration — Cardputer ADV acceptance

## Purpose

Validate the combined F-02/F-07 migration:

- the old `MORPH` percentage is no longer a hidden generation-seed selector;
- repeated accepted `G` requests are the only Genre reroll surface;
- rerolls stay inside the already selected rhythm/composition identity;
- request identity is assigned before live mutation/publication;
- a saved scene with historical non-zero MORPH data remains loadable and its already-saved patterns are not rewritten merely by loading it.

This test is intentionally separate from the quantized BAR_START test. Run the quantized commit acceptance first on the integration firmware target used for hardware validation.

## Hardware

- M5Stack Cardputer ADV (ESP32-S3).
- USB cable for flash + Serial Monitor.
- Headphones or powered speaker for audible comparison.
- Optional Yamaha SEQTRAK if the current integration target is also being checked for MIDI recovery.

No new external hardware is required.

## Wiring

No wiring changes are introduced by F-02/F-07.

If PORT.A I2C hardware is attached, the existing Cardputer ADV bus remains:

```text
SDA GPIO2
SCL GPIO1
3V3 3.3 V
GND GND
```

Do not move existing PORT.A devices to `Wire1`.

## Build / flash

Flash the exact SHA recorded in PR #254 after its automated matrix is green.

Use the same Cardputer ADV build profile as the release gate. Confirm both normal Cardputer ADV and fixed-DRAM CI jobs are green before hardware acceptance.

Open Serial Monitor with the repository's normal Cardputer baud setting.

## Expected behavior

### 1. UI migration

Open `GENRE`.

Expected:

```text
GENRE
VARIANT
RHYTHM
REROLL   REPEAT G
APPLY
```

There is no editable `MORPH xx%` row and no Alt+Left/Right MORPH shortcut.

### 2. First accepted G is compatibility attempt 0

Choose a Genre / Variant / P-level / pattern address and stop transport.

Press `G` once.

Expected:

- generation succeeds immediately while stopped;
- no new rhythm archetype is selected merely because reroll support exists;
- frozen attempt-0 corpus remains deterministic for default historical MORPH `0/0` input.

### 3. Repeated G is bounded reroll

Without changing Genre, Variant, P-level or pattern address, press `G` repeatedly.

Expected:

- successive accepted requests can change admissible realization details;
- the selected rhythm/composition identity does not jump to another archetype family;
- `P3` does not become CHAOS;
- the result remains recognizably inside the chosen Genre/Variant vocabulary.

### 4. PLAY + BAR_START

Start transport and press `G` several times at different positions in the bar.

Expected:

- current material keeps playing until the quantized publication boundary;
- no partial A/B/Drums replacement;
- repeated accepted G requests preserve their own assigned attempt identity even if an older pending candidate is superseded;
- target cancellation never publishes material into a different page/bank/slot.

### 5. Saved historical MORPH scene

Use a project saved by a pre-migration build with non-zero MORPH if available.

After loading, **do not press Enter or G yet**.

Expected:

- scene loads successfully;
- already stored Synth A / Synth B / Drum patterns are unchanged by load alone;
- no regeneration is triggered solely by decoding historical MORPH fields.

Then explicitly Apply or press `G` on the Genre page.

Expected:

- future Genre generation normalizes historical `morphTarget/morphAmount` to `0/0`;
- the old non-zero percentage no longer affects route or seed;
- subsequent variation is controlled by accepted repeated-G attempt ordinal only.

### 6. Session-only behavior

Generate the same tuple several times, reboot, reload the project and generate again.

Expected:

- attempt counter is not persisted in Scene/project data;
- the first accepted generation request for that tuple after reboot starts again at attempt 0.

### 7. Capacity fail-closed smoke

Normal use should not hit the fixed attempt table limit. If a stress build/session deliberately generates more than 64 distinct `(mode, recipe, P-level, patternAddress)` tuples, the next new tuple must fail generation cleanly instead of evicting another tuple or mutating Scene.

Expected Genre toast:

```text
GEN ATTEMPT FULL
```

Existing tuple identities must remain usable.

## Troubleshooting

### `MORPH` is still visible

Wrong branch/SHA was flashed. Confirm PR #254 exact head and rebuild without stale Arduino cache output.

### First G changes frozen default material unexpectedly

Treat as a blocker. Attempt 0 must bypass the new ordinal mixer and preserve the two former MORPH hash positions as zero bytes.

### Repeated G changes Genre/archetype family

Treat as a blocker. `generationAttemptOrdinal` must not participate in rhythm/composition selection; it may affect realization only.

### Reboot continues from the previous attempt number

Treat as a blocker. Attempt state is session-only and must not be written to Scene/NVS/project storage.

### `GEN ATTEMPT FULL` appears during ordinary short testing

Record the sequence of distinct Genre/Variant/P-level/pattern addresses used. The fixed 64-entry table is intended to fail closed, but ordinary workflow should not exhaust it unexpectedly.

### Click, gap or partial pattern on PLAY reroll

This is a quantized publication regression, not expected reroll behavior. Re-run `docs/tests/QUANTIZED_GENRE_GENERATION_CARDPUTER_ADV.md` on the same integration SHA.

## Acceptance checklist

- [ ] `REROLL / REPEAT G` visible; editable MORPH UI absent.
- [ ] Alt+Left/Right does not act as hidden MORPH control.
- [ ] First accepted request for a tuple is deterministic attempt 0.
- [ ] Repeated G produces bounded variation without changing selected archetype identity.
- [ ] PLAY publication remains complete A+B+Drums at the quantized boundary.
- [ ] Superseded/cancelled requests never publish to another target.
- [ ] Loading a historical non-zero-MORPH scene does not regenerate or rewrite saved material.
- [ ] Explicit Apply/G normalizes historical MORPH fields for future generation.
- [ ] Reboot resets attempt state; no Scene/project persistence.
- [ ] Capacity exhaustion, if deliberately induced, fails closed with no unrelated tuple eviction.
- [ ] Serial shows no crash/watchdog/reset during repeated generation.
- [ ] 10-minute repeated-G runtime smoke is clean.
