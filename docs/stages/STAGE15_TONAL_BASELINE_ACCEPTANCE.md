# Stage 15 — Pre-materialization Tonal Baseline

## Purpose

Freeze deterministic legacy Synth A/B fingerprints before Stage 15 replaces legacy pitch redistribution with tonal-intent materialization. This checkpoint changes no production code.

The frozen corpus covers all 16 base `GenerativeMode` values, ordinals `0..7`, and both synth voices. Each row records separate topology, pitch, articulation, and full-step hashes so the later production diff can identify what changed.

Golden file:

```text
tests/data/stage15_tonal_legacy_baseline.tsv
```

It contains 256 data rows plus one header.

## Hardware list

- No hardware required for baseline capture or comparison.
- M5Stack Cardputer-Adv is required only for the later audible Stage 15 acceptance.

## Wiring

No wiring changes. This checkpoint is host-only and does not access GPIO/I2C/SPI/UART/MIDI hardware.

## Build / Flash

Generate the current legacy corpus:

```bash
bash tests/run_stage15_tonal_baseline_dump.sh
```

The CI workflow writes that output to a temporary file and requires:

```bash
test "$(wc -l < actual.tsv)" -eq 257
diff -u tests/data/stage15_tonal_legacy_baseline.tsv actual.tsv
```

No firmware flash is required. The repository Core/SDL/Cardputer/SEQTRAK gates remain regression guards.

## Expected behavior

The corpus contains exactly:

- 16 base genres;
- 8 deterministic ordinals per genre;
- 2 synth voices per ordinal;
- 256 data rows plus one TSV header.

Columns:

```text
mode ordinal voice status secondary_role topology pitch articulation full
```

The source patterns are fixed and deterministic. `StrongRhythmMigrationContext` uses P2, Straight FEEL, amount 0, and the ordinal as pattern address.

The four hashes have deliberately different scopes:

- `topology`: active-step placement only;
- `pitch`: absolute legacy note values by step;
- `articulation`: slide/accent/ghost state by step;
- `full`: note plus articulation, velocity, timing, FX, probability.

The golden was captured from the converged Stage 15 tree before any production tonal materialization was introduced. A second run on the frozen golden head matched it byte-for-byte.

This corpus is a measurement oracle, not an assertion that every row must remain unchanged. The Stage 15 integration diff must classify changes by hash domain. Exact-preservation claims may be added only for profile/ordinal subsets empirically proven equivalent.

## Troubleshooting

If compilation fails, keep the source set aligned with the existing Stage 13/14 migration host harness. Do not add production dependencies merely to make the baseline tool compile.

If row count differs from 257 including the header, verify the 16-mode list, ordinal range `0..7`, and both A/B output rows.

If two runs on the same unchanged head differ, treat that as a determinism bug and do not start tonal materialization.

If a later Stage 15 integration changes `topology` unexpectedly, investigate timing/role ownership before interpreting a pitch change as musical improvement.

## Acceptance checklist

- [x] No production file changes in this checkpoint.
- [x] All 16 base GenerativeMode values are represented.
- [x] Ordinals 0..7 are represented for every mode.
- [x] Both Synth A and Synth B are represented.
- [x] Topology, pitch, articulation, and full fingerprints are separate.
- [x] Exact corpus output is committed as `tests/data/stage15_tonal_legacy_baseline.tsv`.
- [x] Repeated run matches the committed corpus byte-for-byte.
- [ ] Core host suite remains green on the final frozen SHA.
- [ ] SDL remains green on the final frozen SHA.
- [ ] Cardputer ADV + fixed DRAM remains green on the final frozen SHA.
- [ ] SEQTRAK MIDI-only remains green on the final frozen SHA.
- [ ] Three review passes are CLEAN on one unchanged final SHA.
