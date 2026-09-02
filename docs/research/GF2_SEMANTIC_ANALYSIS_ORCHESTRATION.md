# GF2 Semantic Analysis Orchestration

This pipeline turns the frozen GF2 static research into a repeatable repository
check. It does not change production policy, add a semantic owner, or evaluate
rendered musical material.

## Contract

The caller checks out the exact commit to analyze. The orchestrator resolves the
requested ref and fails if it is not the current `HEAD`; it never switches the
user's branch or mutates the index.

```text
checkout exact SHA
  -> compile and run semantic census
  -> write canonical candidate JSON
  -> compare candidate with versioned baseline JSON
  -> derive reachability, recipe, and pattern reports
  -> render one Markdown report
```

Candidate files are written below `build/gf2-semantic-analysis/`. The only
versioned generated file is:

```text
tools/baselines/gf2_semantic_census.json
```

`source_sha` is provenance, not semantic data. A commit-only change therefore
does not fail the comparison. Profile axes, semantic fingerprints, rhythm
archetypes, and BASE-pair classifications are compared.

## Run

```bash
python3 tools/orchestrate_semantic_analysis.py \
  --source-ref HEAD \
  --output-dir build/gf2-semantic-analysis \
  --fail-on-change
```

Outputs:

- `semantic_census.json` — canonical candidate snapshot;
- `genre_diff.json` — exact baseline/candidate semantic delta;
- `reachability_report.json` — normalized C1RF status and blocker counts;
- `recipe_matrix.json` — each recipe compared with its own BASE profile;
- `pattern_statistics.json` — declared vocabulary and collision statistics;
- `report.md` — human-readable summary.

## Accept an intentional semantic change

First inspect `genre_diff.json` and `report.md`. If the delta is intended and
has its own musical evidence, update the baseline from the exact reviewed SHA:

```bash
python3 tools/orchestrate_semantic_analysis.py \
  --source-ref HEAD \
  --output-dir build/gf2-semantic-analysis \
  --update-baseline
```

Commit the baseline change together with the production semantic change and its
evidence. Do not update the baseline only to make CI green.

## CI behavior

`.github/workflows/gf2-semantic-analysis.yml` runs the fail-closed comparison
for changes to generation semantics, the GF2 tools, the frozen reachability
table, or the baseline. It uploads every JSON output and the Markdown report,
including on failure.
