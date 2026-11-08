# CI orchestration

GroovePuter intentionally separates verification depth from GitHub Actions surface.

## Canonical workflows

Only these seven files are allowed in `.github/workflows`:

- `core.yml` — host contracts, current semantic/material regressions, SDL build.
- `cardputer-adv.yml` — normal Cardputer ADV and authoritative FS1B firmware builds.
- `midi-targets.yml` — MIDI ownership/device-profile contracts and SEQTRAK target.
- `memory.yml` — memory instrumentation plus baseline build matrix.
- `release.yml` — release-only exact-head verification, flashable artifacts, hashes and provenance.
- `nightly.yml` — broad preservation corpus that is valuable but too expensive/noisy for every PR.
- `research-manual.yml` — explicitly selected historical/research suites.

`scripts/ci/check_workflow_surface.py` enforces this boundary.

## What was removed

The previous candidate contained 111 workflow YAML files. Most were checkpoint wrappers around test scripts, exact-SHA evidence procedures, or old release/research orchestration. They were removed from the active workflow directory, not from Git history.

The verification assets under `tests/`, `scripts/`, source code, documentation and commits remain intact. The migration ledger is in `docs/ci/WORKFLOW_MIGRATION.tsv`.

## Historical provenance

The pre-consolidation workflow set is permanently recoverable from commit:

`73e31358a4bb38d05be8ab3087461c23abee5ba2`

For example:

```bash
git show 73e31358a4bb38d05be8ab3087461c23abee5ba2:.github/workflows/0_9_10_memory_r1_product_closure.yml
```

Historical exact-SHA A/B procedures are provenance evidence, not automatic current-PR gates. Script-based contracts that remain useful are routed to `nightly.yml` or selectable through `research-manual.yml`.

## Design rule

A musical/runtime/test contract belongs in `tests/` or `scripts/`.

A GitHub workflow is only an orchestration entry point.

Do not create a new workflow for a checkpoint. Add the verification command to the appropriate runner or research suite instead.
