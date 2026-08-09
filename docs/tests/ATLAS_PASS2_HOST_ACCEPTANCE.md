# Atlas Pass 2 — Host Acceptance

## Purpose

Reproduce Atlas Pass #2 against the exact pinned SEQTRAK Pattern Atlas v2.6 corpus and verify that only aggregate/non-reversible evidence is committed.

## Hardware list

None. This is an offline host-only audit.

## Wiring

None.

## Build / run

Requirements:

```text
Python 3
C++17 compiler (g++ in CI)
exact Atlas ZIP with SHA-256:
5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd
```

From the repository root:

```bash
bash tests/run_atlas_pass2_runtime_dump.sh
python3 tests/test_atlas_pass2_extractor.py
python3 tests/test_atlas_pass2_outputs.py

rm -rf build/atlas-pass2/recomputed
python3 tools/atlas/run_atlas_pass2.py \
  /path/to/seqtrak_pattern_atlas_csv_v2_6.zip \
  docs/architecture/atlas_pass2/RUNTIME_RHYTHM_BASELINE.tsv \
  build/atlas-pass2/recomputed

diff -ru \
  --exclude=RUNTIME_RHYTHM_BASELINE.tsv \
  docs/architecture/atlas_pass2 \
  build/atlas-pass2/recomputed
```

The committed directory also contains `RUNTIME_RHYTHM_BASELINE.tsv`; it is produced separately by the C++ runtime dump and checked by `run_atlas_pass2_runtime_dump.sh`.

## Expected behavior

The unit/schema gates print:

```text
Atlas Pass 2 runtime catalog dump: OK
Atlas Pass 2 extractor unit contracts: OK
Atlas Pass 2 aggregate output schema/rights: OK
```

The recomputed result must reproduce the committed aggregate CSV/JSON outputs byte-for-byte.

Key summary invariants:

```text
patterns                       413
events                        9377
one-bar eligible               300
one-bar structural groups      269
recurring skeleton candidates    8
NEAR_EXISTING                    5
REVIEW                           2
HOLD                             1
ACCEPT                           0
Stage 7 admission           CLOSED
```

## Troubleshooting

- `unexpected Atlas archive SHA-256`: the supplied corpus is not the pinned v2.6 archive; do not bypass this guard.
- runtime baseline diff fails: `ReferenceVocabulary` changed; regenerate/review the baseline before trusting distances.
- recomputed aggregate diff fails: treat it as a Pass #2 regression. Do not update expected outputs until the cause is understood.
- rights/schema test fails: an output may expose a restricted identifier/hash/mask or an evidence contract changed.
- aggregate repository host CI may still fail later on the inherited Cardputer ADV `PA_EN` assertion; Pass #2 gates must pass before that unrelated failure.

## Acceptance checklist

- [ ] Atlas ZIP SHA-256 matches the pinned value.
- [ ] Runtime baseline dump is exactly 20 current archetypes.
- [ ] Runtime baseline diff passes.
- [ ] Extractor unit contracts pass.
- [ ] Aggregate output schema/rights test passes.
- [ ] Recomputed outputs match committed outputs byte-for-byte.
- [ ] No raw Atlas ZIP, pattern IDs, structural group IDs, source locators, event lists, literal masks or per-pattern fingerprints are committed.
- [ ] `ATLAS_PASS2_SUMMARY.json` keeps Stage 7 admission `CLOSED`.
