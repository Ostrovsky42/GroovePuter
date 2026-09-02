#!/usr/bin/env python3
"""Produce the canonical machine-readable GF2 semantic census snapshot."""

from __future__ import annotations

import argparse
import importlib.util
import json
import subprocess
from pathlib import Path
from types import ModuleType


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "tools/gf2/generate_gf2_c1f_final_static_census.py"

AXIS_FIELDS = {
    "rhythm": ("rhythms", "rhythm_payload_fingerprint", "canonical_drum_fingerprint"),
    "feel": ("feels",),
    "bass": ("bass",),
    "chord": ("chord",),
    "progression": ("progressions",),
    "melodic": ("melodic",),
    "motif": ("motifs",),
    "phrase": ("phrases",),
    "corridor": (
        "bpm_min",
        "bpm_suggested",
        "bpm_max",
        "grid_steps",
        "density_min",
        "density_max",
    ),
    "secondary_role": ("secondary_role",),
    "tonal": ("tonal_payload",),
}

FINGERPRINT_FIELDS = (
    "composition_support_fingerprint",
    "composition_weighted_fingerprint",
    "corridor_fingerprint",
    "role_fingerprint",
    "tonal_payload_fingerprint",
    "rhythm_payload_fingerprint",
    "canonical_drum_fingerprint",
    "primary_static_fingerprint",
    "full_trace_fingerprint",
)


def load_generator() -> ModuleType:
    spec = importlib.util.spec_from_file_location("gf2_c1f_generator", GENERATOR)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load census generator: {GENERATOR}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def resolve_sha(ref: str) -> str:
    return subprocess.run(
        ["git", "rev-parse", "--verify", f"{ref}^{{commit}}"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def profile_snapshot(row: dict[str, str]) -> dict[str, object]:
    recipe_id = row["recipe_id"] if row["is_base"] == "0" else "BASE"
    return {
        "key": f"{row['genre_key']}:{recipe_id}",
        "genre_id": row["genre_id"],
        "genre_key": row["genre_key"],
        "genre": row["genre_display"],
        "kind": "BASE" if row["is_base"] == "1" else "RECIPE",
        "recipe_id": recipe_id,
        "recipe": row["recipe_name"] if row["is_base"] == "0" else "BASE",
        "axes": {
            axis: {field: row[field] for field in fields}
            for axis, fields in AXIS_FIELDS.items()
        },
        "fingerprints": {field: row[field] for field in FINGERPRINT_FIELDS},
        "classification_vs_base": row["classification_vs_base"],
        "changed_domains_vs_base": row["changed_domains_vs_base"],
        "single_option_axes": row["single_option_axes"],
    }


def validate_structure(
    profiles: list[dict[str, str]], archetypes: list[dict[str, str]]
) -> None:
    if not profiles:
        raise RuntimeError("semantic census contains no profiles")
    if not archetypes:
        raise RuntimeError("semantic census contains no rhythm archetypes")
    profile_keys = [
        (row["genre_id"], row["recipe_id"], row["is_base"]) for row in profiles
    ]
    if len(set(profile_keys)) != len(profile_keys):
        raise RuntimeError("semantic census contains duplicate profile identities")
    archetype_ids = [row["archetype_id"] for row in archetypes]
    if len(set(archetype_ids)) != len(archetype_ids):
        raise RuntimeError("semantic census contains duplicate rhythm archetype ids")
    base_genres = [row["genre_id"] for row in profiles if row["is_base"] == "1"]
    if len(set(base_genres)) != len(base_genres):
        raise RuntimeError("semantic census contains multiple BASE profiles for a genre")
    base_genre_set = set(base_genres)
    orphan_recipes = sorted(
        row["recipe_name"]
        for row in profiles
        if row["is_base"] == "0" and row["genre_id"] not in base_genre_set
    )
    if orphan_recipes:
        raise RuntimeError(f"recipes without BASE profiles: {orphan_recipes}")


def build_snapshot(source_ref: str) -> dict[str, object]:
    generator = load_generator()
    generator.compile_dump()
    profiles = generator.dump_rows("profiles")
    archetypes = generator.dump_rows("archetypes")
    validate_structure(profiles, archetypes)
    generator.add_fingerprints(profiles, archetypes)
    generator.classify_recipes(profiles)
    pairs = generator.base_pairs(profiles, expected_count=None)

    return {
        "schema_version": 1,
        "source_sha": resolve_sha(source_ref),
        "semantic_base_sha": generator.BASE_SHA,
        "profiles": sorted(
            (profile_snapshot(row) for row in profiles),
            key=lambda row: str(row["key"]),
        ),
        "archetypes": sorted(
            (
                {
                    "archetype_id": row["archetype_id"],
                    "archetype_key": row["archetype_key"],
                    "name": row["name"],
                    "family": row["family"],
                    "bpm_min": row["bpm_min"],
                    "bpm_max": row["bpm_max"],
                    "semantic_payload_fingerprint": row[
                        "semantic_payload_fingerprint"
                    ],
                    "drum_payload_fingerprint": row["drum_payload_fingerprint"],
                }
                for row in archetypes
            ),
            key=lambda row: int(str(row["archetype_id"])),
        ),
        "base_pairs": sorted(
            pairs,
            key=lambda row: (row["genre_a"], row["genre_b"]),
        ),
    }


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-ref", default="HEAD")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    snapshot = build_snapshot(args.source_ref)
    write_json(args.output, snapshot)
    print(
        f"semantic census: {len(snapshot['profiles'])} profiles, "
        f"{len(snapshot['archetypes'])} archetypes, "
        f"{len(snapshot['base_pairs'])} BASE pairs"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
