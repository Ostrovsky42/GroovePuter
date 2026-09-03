#!/usr/bin/env python3
"""Summarize declared pattern vocabulary and static profile collisions."""

from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path


BAG_AXES = {
    "rhythm": "rhythms",
    "feel": "feels",
    "bass": "bass",
    "chord": "chord",
    "progression": "progressions",
    "melodic": "melodic",
    "motif": "motifs",
    "phrase": "phrases",
}


def read_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def candidates(value: str) -> list[tuple[str, str, int]]:
    if not value:
        return []
    result = []
    for entry in value.split(";"):
        identity, raw_weight = entry.rsplit("@", 1)
        candidate_id, name = identity.split(":", 1)
        result.append((candidate_id, name, int(raw_weight)))
    return result


def build_statistics(census: dict[str, object]) -> dict[str, object]:
    profiles = census.get("profiles", [])
    vocabulary = {}
    for axis, field in BAG_AXES.items():
        profile_counts: Counter[str] = Counter()
        total_weights: Counter[str] = Counter()
        names: dict[str, str] = {}
        for profile in profiles:
            for candidate_id, name, weight in candidates(profile["axes"][axis][field]):
                profile_counts[candidate_id] += 1
                total_weights[candidate_id] += weight
                names[candidate_id] = name
        vocabulary[axis] = {
            "candidate_count": len(names),
            "candidates": [
                {
                    "id": candidate_id,
                    "name": names[candidate_id],
                    "profile_count": profile_counts[candidate_id],
                    "total_weight": total_weights[candidate_id],
                }
                for candidate_id in sorted(names, key=int)
            ],
        }

    collision_groups: dict[str, list[str]] = defaultdict(list)
    for profile in profiles:
        collision_groups[
            profile["fingerprints"]["primary_static_fingerprint"]
        ].append(profile["key"])
    collisions = sorted(
        (sorted(group) for group in collision_groups.values() if len(group) > 1),
        key=lambda group: (len(group), group),
    )

    pair_classifications = Counter(
        row["classification"] for row in census.get("base_pairs", [])
    )
    archetype_families = Counter(
        row["family"] for row in census.get("archetypes", [])
    )
    return {
        "schema_version": 1,
        "source_sha": census.get("source_sha", "UNKNOWN"),
        "profile_count": len(profiles),
        "base_profile_count": sum(row["kind"] == "BASE" for row in profiles),
        "recipe_profile_count": sum(row["kind"] == "RECIPE" for row in profiles),
        "archetype_count": len(census.get("archetypes", [])),
        "archetype_family_counts": dict(sorted(archetype_families.items())),
        "base_pair_classification_counts": dict(
            sorted(pair_classifications.items())
        ),
        "static_collision_group_count": len(collisions),
        "static_collision_groups": collisions,
        "vocabulary": vocabulary,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--census", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    statistics = build_statistics(read_json(args.census))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(statistics, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(
        f"pattern statistics: {statistics['profile_count']} profiles, "
        f"{statistics['archetype_count']} archetypes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
