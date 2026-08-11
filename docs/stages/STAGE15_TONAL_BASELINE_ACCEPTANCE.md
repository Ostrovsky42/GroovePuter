# Stage 15 — Pre-materialization Tonal Baseline

## Purpose

Freeze deterministic legacy Synth A/B fingerprints before Stage 15 replaces legacy pitch redistribution with tonal-intent materialization. This checkpoint changes no production code.

The corpus covers all 16 base `GenerativeMode` values, ordinals `0..7`, and both synth voices. Each row records separate topology, pitch, articulation, and full-step hashes so later diffs can identify what changed.

## Hardware list

- No hardware required for baseline capture.
- M5Stack Cardputer-Adv is required only for the later audible Stage 15 acceptance.

## Wiring

No wiring changes. This checkpoint is host-only and does not access GPIO/I2C/SPI/UART/MIDI hardware.

## Build / Flash

Dump the current legacy corpus:

```bash
bash tests/run_stage15_tonal_baseline_dump.sh
```

No firmware flash is required. The repository Core/SDL/Cardputer/SEQTRAK gates remain regression guards.

## Expected behavior

The dump contains exactly:

- 16 base genres;
- 8 deterministic ordinals per genre;
- 2 synth voices per ordinal;
- 256 data rows plus one TSV header.

Columns:

```text
mode ordinal voice status secondary_role topology pitch articulation full
```

The source patterns are fixed and deterministic. `StrongRhythmMigrationContext` uses P2, Straight FEEL, amount 0, and the ordinal as pattern address.

This first PR run intentionally prints the corpus. The exact output is then committed as the immutable pre-materialization reference and the gate is changed from dump-only to compare-against-reference before the checkpoint is merged.

## Troubleshooting

If compilation fails, keep the source set aligned with the existing Stage 13/14 migration host harness. Do not add production dependencies merely to make the baseline tool compile.

If row count differs from 256, verify the 16-mode list, ordinal range `0..7`, and both A/B output rows.

If two runs on the same unchanged head differ, treat that as a determinism bug and do not start tonal materialization.

## Acceptance checklist

- [ ] No production file changes in this checkpoint.
- [ ] All 16 base GenerativeMode values are represented.
- [ ] Ordinals 0..7 are represented for every mode.
- [ ] Both Synth A and Synth B are represented.
- [ ] Topology, pitch, articulation, and full fingerprints are separate.
- [ ] Exact corpus output is committed after the initial dump run.
- [ ] Repeated run matches the committed corpus byte-for-byte.
- [ ] Core host suite remains green.
- [ ] SDL remains green.
- [ ] Cardputer ADV + fixed DRAM remains green.
- [ ] SEQTRAK MIDI-only remains green.
