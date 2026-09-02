# Stage 15 Tonal RC Hardening

## Purpose

This checkpoint supplements `STAGE15_TONAL_INTEGRATION_ACCEPTANCE.md` for the
release-candidate line after the integrated Stage 15 code became the unit of
review.

It pins three release-facing facts that were not fully protected by the first
acceptance pass:

1. the TonalMaterializer stack gate must be measured with the firmware's `-Os`
   optimization policy;
2. both the legacy rollback path **and** the production tonal-materialized path
   need frozen A/B corpora;
3. the remaining scale ownership exception is deliberate: generation uses
   `src/generation/tonal/scale_catalog.h`, while `PerformanceKeyboard` remains a
   separate live-input compatibility context and is not a generated-pitch owner.

No production DSP/generation behavior is changed by this hardening checkpoint.

## Hardware list

- M5Stack Cardputer-Adv / Cardputer ADV (ESP32-S3).
- USB-C cable for build, flash and Serial monitoring.
- Built-in speaker for the final musical confirmation.
- Optional Yamaha SEQTRAK for MIDI-route confirmation.

## Wiring

No new wiring is introduced.

- Cardputer ADV remains powered/programmed over USB-C.
- PORT.A I2C remains GPIO2 SDA / GPIO1 SCL if external I2C hardware is attached.
- Stage 15 RC hardening adds no GPIO, I2C, SPI or UART ownership.

## Build / Flash

Run the focused host gates:

```bash
bash tests/run_tonal_materializer_tests.sh
bash tests/run_stage15_tonal_baseline_dump.sh > /tmp/stage15_legacy.tsv
diff -u tests/data/stage15_tonal_legacy_baseline.tsv /tmp/stage15_legacy.tsv
bash tests/run_stage15_tonal_baseline_dump.sh --tonal > /tmp/stage15_tonal.tsv
diff -u tests/data/stage15_tonal_enabled_baseline.tsv /tmp/stage15_tonal.tsv
python3 tests/test_stage15_tonal_corpus_boundary.py
```

Then run the repository gate:

```bash
bash tests/run_host_tests.sh
```

The exact frozen RC head must also pass SDL, Cardputer ADV normal + fixed DRAM,
SEQTRAK MIDI-only, Tonal Projector, Stage 15B, Stage 15C, global-scale,
register-sweep and final tonal acceptance workflows before hardware audition.

Flash only that unchanged reviewed SHA.

## Expected behavior

### Stack gate

`tests/run_tonal_materializer_tests.sh` compiles the stack-usage probe with
`-Os`, matching the firmware size-optimization policy rather than reporting an
unoptimized host-only frame.

The first CI measurement after this correction reported:

```text
Tonal Materializer stack usage (-Os): 192 B (gate 384 B)
```

The gate remains on the owner frame; the downstream TonalProjector has its own
bounded implementation. The device task high-water mark is still a separate
physical Stage 6.1 acceptance item and is not replaced by host `.su` files.

### Dual frozen corpora

The two independent 256-row corpora are:

```text
tests/data/stage15_tonal_legacy_baseline.tsv
tests/data/stage15_tonal_enabled_baseline.tsv
```

Each contains 16 base modes × 8 deterministic addresses × A/B, plus one header
line. The dump has two explicit modes:

```text
no argument  -> tonalMaterializationEnabled = false
--tonal      -> tonalMaterializationEnabled = true
```

The workflow performs `diff -u` against each golden independently. A change in
production tonal pitch can therefore no longer hide behind a green rollback-only
baseline.

`tests/test_stage15_tonal_corpus_boundary.py` additionally requires, for the
same `(mode, ordinal, voice)` keys:

- identical migration status;
- identical semantic secondary role;
- identical topology hash;
- identical articulation hash;
- at least one pitch hash difference between legacy and tonal paths.

Exact per-row pitch behavior is still pinned by the tonal golden itself.

### Fixed DRAM evidence

Independent object-section measurement on the integrated Stage 15 candidate
showed no mutable static allocation from the new tonal/generation modules:

```text
tonal_materializer    .bss=0  .data=0  .rodata=160
tonal_projector       .bss=0  .data=0  .rodata=40
chord_progression     .bss=0  .data=0  .rodata=315
melodic_pitch_intent  .bss=0  .data=0  .rodata=120
```

The `.rodata` entries are flash-resident constants. The absolute fixed-DRAM
budget remains owned by the normal Cardputer ADV CI gate; this evidence only
establishes the Stage 15 mutable-static delta as zero for the measured candidate.

### Scale ownership boundary

Generation-side scale interval data has one canonical owner:

```text
src/generation/tonal/scale_catalog.h
```

Both TonalProjector and legacy AdvancedPatternGenerator consume it.
`PerformanceKeyboard` is intentionally outside this ownership contract: it is a
live-input compatibility layer with its own limited keyboard-scale context, not
a generated-pitch materializer. Do not "deduplicate" it into the generation
catalog unless live-input semantics are separately redesigned and tested.

### Hardware confirmation targets

The frozen tonal corpus changes the interpretation of the listening matrix from
"did the hash change?" to concrete musical expectations:

- **Techno**: conservative/root-held behavior is expected; becoming more static
  than the old synthetic pitch ramp is not itself a regression.
- **House**: harmonic-root motion may move the bass while rhythm remains fixed.
- **Acid**: root/fifth/degree movement should be audible without onset or
  articulation movement.
- **LoFi Synth B**: sustained/held behavior must remain restrained where the
  semantic plan requests it.

For every case, topology and articulation must remain owned by their existing
layers; tonal integration changes absolute pitch only.

## Troubleshooting

### Legacy golden changes

Do not regenerate the legacy golden. A mismatch with tonal materialization
explicitly disabled is a rollback regression.

### Tonal golden changes

Treat a tonal golden mismatch as a production pitch regression until the exact
semantic cause is reviewed. Do not update the golden simply to make CI green.

If the change is intentional, review the affected progression/contour/scale
semantics and the audible result before freezing a replacement corpus.

### Cross-corpus topology or articulation mismatch

TonalMaterializer crossed its ownership boundary. Check the migration adapter
and role integration before changing the assertion.

### Stack gate differs from a local `-O2` probe

The release gate intentionally uses `-Os`, because that is the firmware
optimization policy. Compare like-for-like compiler flags before treating the
numbers as contradictory.

### Host gates green but runtime stack is unknown

Run the Stage 6.1 physical task high-water probe on the exact frozen firmware.
Host stack-usage files do not measure FreeRTOS task composition, interrupt
interaction or the device's real high-water mark.

## Acceptance checklist

- [ ] TonalMaterializer stack gate is compiled with `-Os`.
- [ ] TonalMaterializer owner frame remains <= 384 B.
- [ ] Legacy dump is exactly 257 lines and matches its frozen TSV.
- [ ] Tonal-enabled dump is exactly 257 lines and matches its frozen TSV.
- [ ] `test_stage15_tonal_corpus_boundary.py` passes.
- [ ] Legacy and tonal corpora have identical keys/status/role/topology/articulation.
- [ ] Tonal corpus differs from legacy in pitch for at least one row.
- [ ] Generation-side scale ownership remains centralized in `scale_catalog.h`.
- [ ] `PerformanceKeyboard` remains explicitly documented as a separate live-input context.
- [ ] Full host suite passes on the final unchanged SHA.
- [ ] SDL passes.
- [ ] Cardputer ADV normal + fixed DRAM passes.
- [ ] SEQTRAK MIDI-only passes.
- [ ] All Stage 15 focused workflows pass on that same SHA.
- [ ] Three clean reviews complete after the last head movement.
- [ ] Stage 6.1 physical task stack high-water is measured on the exact frozen firmware.
- [ ] Hardware listening matrix is confirmed on that same firmware.
