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

Candidate files are written below `build/gf2-semantic-analysis/`. The
versioned generated files are:

```text
tools/baselines/gf2_semantic_census.json
tools/baselines/gf2_reachability_report.json
```

`source_sha` is provenance, not semantic data. A commit-only change therefore
does not fail the comparison. Profile axes, semantic fingerprints, rhythm
archetypes, BASE-pair classifications, reachability statuses, owners, blockers,
failure modes, and fallbacks are compared. Profile and archetype counts are not
fixed: intentional catalog growth must appear in the diff before acceptance.

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
- `reachability_diff.json` — exact reachability/owner delta;
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

Both baselines are updated by that command. Commit their changes together with
the production semantic change and its evidence. Do not update a baseline only
to make CI green.

## CI behavior

`.github/workflows/gf2-semantic-analysis.yml` runs on every pull request and on
pushes to `main` and `dev_0.9.9`. It executes the orchestration unit/integration
suite plus the frozen C1F/C1RF/C1DF predecessor contracts. CI fails on an
unaccepted census or reachability delta and uploads every JSON output and the
Markdown report, including on failure.
