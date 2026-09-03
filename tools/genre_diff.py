#!/usr/bin/env python3
"""Compare two canonical semantic census JSON snapshots."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def read_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def keyed(rows: list[dict[str, object]], field: str) -> dict[str, dict[str, object]]:
    result: dict[str, dict[str, object]] = {}
    for row in rows:
        key = str(row[field])
        if key in result:
            raise RuntimeError(f"duplicate snapshot key {field}={key}")
        result[key] = row
    return result


def compare_profiles(
    baseline: dict[str, object], candidate: dict[str, object]
) -> tuple[list[str], list[str], list[dict[str, object]]]:
    old = keyed(baseline.get("profiles", []), "key")
    new = keyed(candidate.get("profiles", []), "key")
    added = sorted(new.keys() - old.keys())
    removed = sorted(old.keys() - new.keys())
    changed = []
    for key in sorted(old.keys() & new.keys()):
        old_axes = old[key]["axes"]
        new_axes = new[key]["axes"]
        axis_names = sorted(set(old_axes) | set(new_axes))
        changed_axes = [axis for axis in axis_names if old_axes.get(axis) != new_axes.get(axis)]
        old_fingerprints = old[key].get("fingerprints", {})
        new_fingerprints = new[key].get("fingerprints", {})
        fingerprint_names = sorted(set(old_fingerprints) | set(new_fingerprints))
        changed_fingerprints = [
            name
            for name in fingerprint_names
            if old_fingerprints.get(name) != new_fingerprints.get(name)
        ]
        changed_metadata = [
            name
            for name in (
                "genre_id",
                "genre_key",
                "genre",
                "kind",
                "recipe_id",
                "recipe",
                "classification_vs_base",
                "changed_domains_vs_base",
                "single_option_axes",
            )
            if old[key].get(name) != new[key].get(name)
        ]
        if changed_axes or changed_fingerprints or changed_metadata:
            changed.append(
                {
                    "key": key,
                    "changed_axes": changed_axes,
                    "changed_fingerprints": changed_fingerprints,
                    "changed_metadata": changed_metadata,
                }
            )
    return added, removed, changed


def compare_archetypes(
    baseline: dict[str, object], candidate: dict[str, object]
) -> dict[str, object]:
    old = keyed(baseline.get("archetypes", []), "archetype_id")
    new = keyed(candidate.get("archetypes", []), "archetype_id")
    changed = []
    for key in sorted(old.keys() & new.keys(), key=int):
        fields = sorted(set(old[key]) | set(new[key]))
        changed_fields = [
            field for field in fields if old[key].get(field) != new[key].get(field)
        ]
        if changed_fields:
            changed.append(
                {"archetype_id": key, "changed_fields": changed_fields}
            )
    return {
        "added": sorted(new.keys() - old.keys(), key=int),
        "removed": sorted(old.keys() - new.keys(), key=int),
        "changed": changed,
    }


def pair_key(row: dict[str, object]) -> str:
    return f"{row['genre_a']}::{row['genre_b']}"


def compare_pairs(
    baseline: dict[str, object], candidate: dict[str, object]
) -> dict[str, object]:
    old = {pair_key(row): row for row in baseline.get("base_pairs", [])}
    new = {pair_key(row): row for row in candidate.get("base_pairs", [])}
    return {
        "added": sorted(new.keys() - old.keys()),
        "removed": sorted(old.keys() - new.keys()),
        "changed": sorted(
            key for key in old.keys() & new.keys() if old[key] != new[key]
        ),
    }


def build_diff(
    baseline: dict[str, object], candidate: dict[str, object]
) -> dict[str, object]:
    if baseline.get("schema_version") != candidate.get("schema_version"):
        raise RuntimeError(
            "cannot compare census snapshots with different schema versions"
        )
    added, removed, changed = compare_profiles(baseline, candidate)
    archetypes = compare_archetypes(baseline, candidate)
    pairs = compare_pairs(baseline, candidate)
    has_changes = bool(
        added
        or removed
        or changed
        or archetypes["added"]
        or archetypes["removed"]
        or archetypes["changed"]
        or pairs["added"]
        or pairs["removed"]
        or pairs["changed"]
    )
    return {
        "schema_version": 1,
        "baseline_source_sha": baseline.get("source_sha", "UNKNOWN"),
        "candidate_source_sha": candidate.get("source_sha", "UNKNOWN"),
        "has_semantic_changes": has_changes,
        "profiles": {"added": added, "removed": removed, "changed": changed},
        "archetypes": archetypes,
        "base_pairs": pairs,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fail-on-change", action="store_true")
    args = parser.parse_args()

    diff = build_diff(read_json(args.baseline), read_json(args.candidate))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(diff, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"genre diff: semantic_changes={str(diff['has_semantic_changes']).lower()}")
    return int(args.fail_on_change and diff["has_semantic_changes"])


if __name__ == "__main__":
    raise SystemExit(main())
