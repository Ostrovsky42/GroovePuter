#!/usr/bin/env python3
"""Run the reproducible GF2 census, comparison, and report pipeline."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASELINE = ROOT / "tools/baselines/gf2_semantic_census.json"
DEFAULT_OUTPUT = ROOT / "build/gf2-semantic-analysis"
REACHABILITY = ROOT / "docs/research/GF2_C1RF_FINAL_SEMANTIC_REACHABILITY.tsv"


def resolve_sha(ref: str) -> str:
    return subprocess.run(
        ["git", "rev-parse", "--verify", f"{ref}^{{commit}}"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def run(command: list[str]) -> None:
    subprocess.run(command, cwd=ROOT, check=True)


def read_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def markdown_list(values: list[str]) -> str:
    return ", ".join(f"`{value}`" for value in values) if values else "NONE"


def render_report(
    output: Path,
    census: dict[str, object],
    diff: dict[str, object],
    reachability: dict[str, object],
    recipes: dict[str, object],
    patterns: dict[str, object],
) -> None:
    changed_profiles = diff["profiles"]["changed"]
    lines = [
        "# GF2 Semantic Analysis Report",
        "",
        f"- Candidate SHA: `{diff['candidate_source_sha']}`",
        f"- Baseline SHA: `{diff['baseline_source_sha']}`",
        f"- Frozen semantic base: `{census['semantic_base_sha']}`",
        f"- Semantic changes: **{'YES' if diff['has_semantic_changes'] else 'NONE'}**",
        "",
        "## Census",
        "",
        "| Measure | Count |",
        "|---|---:|",
        f"| Profiles | {len(census['profiles'])} |",
        f"| BASE profiles | {patterns['base_profile_count']} |",
        f"| Recipes | {recipes['recipe_count']} |",
        f"| Rhythm archetypes | {patterns['archetype_count']} |",
        f"| BASE genre pairs | {len(census['base_pairs'])} |",
        f"| Static collision groups | {patterns['static_collision_group_count']} |",
        "",
        "## Snapshot diff",
        "",
        f"- Added profiles: {markdown_list(diff['profiles']['added'])}",
        f"- Removed profiles: {markdown_list(diff['profiles']['removed'])}",
        f"- Changed profiles: {len(changed_profiles)}",
        f"- Changed archetypes: {markdown_list(diff['archetypes']['changed'])}",
        f"- Changed BASE pairs: {len(diff['base_pairs']['changed'])}",
    ]
    if changed_profiles:
        lines.extend(["", "| Profile | Changed axes |", "|---|---|"])
        lines.extend(
            f"| `{row['key']}` | {markdown_list(row['changed_axes'])} |"
            for row in changed_profiles
        )

    lines.extend(
        [
            "",
            "## Reachability",
            "",
            f"Traces: {reachability['trace_count']}",
            "",
            "| Status | Count |",
            "|---|---:|",
        ]
    )
    lines.extend(
        f"| {status} | {count} |"
        for status, count in reachability["status_counts"].items()
    )

    lines.extend(
        [
            "",
            "## Recipe axis changes",
            "",
            "| Axis | Recipes changed |",
            "|---|---:|",
        ]
    )
    lines.extend(
        f"| {axis} | {count} |"
        for axis, count in recipes["axis_change_counts"].items()
    )

    lines.extend(
        [
            "",
            "## Declared vocabulary",
            "",
            "| Axis | Candidates |",
            "|---|---:|",
        ]
    )
    lines.extend(
        f"| {axis} | {payload['candidate_count']} |"
        for axis, payload in patterns["vocabulary"].items()
    )
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-ref", default="HEAD")
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--update-baseline", action="store_true")
    parser.add_argument("--fail-on-change", action="store_true")
    args = parser.parse_args()

    expected_sha = resolve_sha(args.source_ref)
    head_sha = resolve_sha("HEAD")
    if expected_sha != head_sha:
        raise RuntimeError(
            f"checkout mismatch: HEAD is {head_sha}, requested SHA is {expected_sha}"
        )
    if not args.baseline.exists() and not args.update_baseline:
        raise RuntimeError(
            f"baseline does not exist: {args.baseline}; run once with --update-baseline"
        )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    candidate_path = args.output_dir / "semantic_census.json"
    diff_path = args.output_dir / "genre_diff.json"
    reachability_path = args.output_dir / "reachability_report.json"
    recipe_path = args.output_dir / "recipe_matrix.json"
    pattern_path = args.output_dir / "pattern_statistics.json"
    report_path = args.output_dir / "report.md"

    run(
        [
            "python3",
            "tools/semantic_census.py",
            "--source-ref",
            expected_sha,
            "--output",
            str(candidate_path),
        ]
    )
    if not args.baseline.exists():
        args.baseline.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(candidate_path, args.baseline)
    run(
        [
            "python3",
            "tools/genre_diff.py",
            "--baseline",
            str(args.baseline),
            "--candidate",
            str(candidate_path),
            "--output",
            str(diff_path),
        ]
    )
    run(
        [
            "python3",
            "tools/reachability_report.py",
            "--input",
            str(REACHABILITY),
            "--output",
            str(reachability_path),
        ]
    )
    run(
        [
            "python3",
            "tools/recipe_matrix.py",
            "--census",
            str(candidate_path),
            "--output",
            str(recipe_path),
        ]
    )
    run(
        [
            "python3",
            "tools/pattern_statistics.py",
            "--census",
            str(candidate_path),
            "--output",
            str(pattern_path),
        ]
    )

    census = read_json(candidate_path)
    diff = read_json(diff_path)
    reachability = read_json(reachability_path)
    recipes = read_json(recipe_path)
    patterns = read_json(pattern_path)
    render_report(report_path, census, diff, reachability, recipes, patterns)

    if args.update_baseline:
        args.baseline.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(candidate_path, args.baseline)

    print(f"semantic analysis report: {report_path}")
    return int(args.fail_on_change and diff["has_semantic_changes"])


if __name__ == "__main__":
    raise SystemExit(main())
