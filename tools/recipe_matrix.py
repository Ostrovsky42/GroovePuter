#!/usr/bin/env python3
"""Build a BASE-versus-recipe semantic axis matrix from a census snapshot."""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path


def read_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def build_matrix(census: dict[str, object]) -> dict[str, object]:
    profiles = census.get("profiles", [])
    bases = {
        str(row["genre_id"]): row for row in profiles if row["kind"] == "BASE"
    }
    recipes = [row for row in profiles if row["kind"] == "RECIPE"]
    rows = []
    axis_change_counts: Counter[str] = Counter()
    for recipe in recipes:
        genre_id = str(recipe["genre_id"])
        if genre_id not in bases:
            raise RuntimeError(f"recipe has no BASE profile: {recipe['key']}")
        base = bases[genre_id]
        axis_names = sorted(set(base["axes"]) | set(recipe["axes"]))
        changed_axes = [
            axis for axis in axis_names if base["axes"].get(axis) != recipe["axes"].get(axis)
        ]
        unchanged_axes = [axis for axis in axis_names if axis not in changed_axes]
        axis_change_counts.update(changed_axes)
        rows.append(
            {
                "genre": recipe["genre"],
                "genre_key": recipe["genre_key"],
                "recipe_id": recipe["recipe_id"],
                "recipe": recipe["recipe"],
                "changed_axes": changed_axes,
                "unchanged_axes": unchanged_axes,
                "declared_changed_domains": recipe["changed_domains_vs_base"],
                "classification": recipe["classification_vs_base"],
                "single_option_axes": recipe["single_option_axes"],
            }
        )
    rows.sort(key=lambda row: (row["genre_key"], row["recipe_id"]))
    return {
        "schema_version": 1,
        "source_sha": census.get("source_sha", "UNKNOWN"),
        "base_profile_count": len(bases),
        "recipe_count": len(recipes),
        "axis_change_counts": dict(sorted(axis_change_counts.items())),
        "recipes": rows,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--census", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    matrix = build_matrix(read_json(args.census))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(matrix, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(
        f"recipe matrix: {matrix['recipe_count']} recipes across "
        f"{matrix['base_profile_count']} BASE profiles"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
