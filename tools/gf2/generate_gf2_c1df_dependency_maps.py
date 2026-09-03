#!/usr/bin/env python3
"""Join the final GF2-C1F census with the GF2-C1RF reachability audit.

This is a static, deterministic research transform. It does not compile or run
the renderer and does not inspect generated musical material.
"""

from __future__ import annotations

import argparse
import csv
import io
import sys
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CENSUS_PATH = ROOT / "docs/research/GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv"
C1_PAIR_PATH = ROOT / "docs/research/GF2_C1F_BASE_GENRE_PAIRS.tsv"
REACHABILITY_PATH = ROOT / "docs/research/GF2_C1RF_FINAL_SEMANTIC_REACHABILITY.tsv"
BASE_OUTPUT = ROOT / "docs/research/GF2_C1DF_BASE_PAIR_DEPENDENCY.tsv"
RECIPE_OUTPUT = ROOT / "docs/research/GF2_C1DF_RECIPE_BASE_DEPENDENCY.tsv"

GF2_BASE = "0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d"
C1RF_HEAD = "574b830526c784ffe761286096dd62e22d6361d4"

CONNECTED = {"CONNECTED"}
UNREACHABLE = {"BLOCKED", "DROPPED", "DECLARED_ONLY"}
UNRELIABLE = {
    "PARTIALLY_CONNECTED",
    "FAILURE_MASKED",
    "LEGACY_FALLBACK",
    "DUPLICATE_OWNER",
    "AMBIGUOUS_OWNER",
    "UNKNOWN",
}

BAG_FIELDS = [
    ("rhythms", "RHYTHM_COMPATIBILITY", "rhythm_archetype_and_weight", "DRUMS"),
    ("feels", "PROFILE_FEEL", "profile_suggested_feel", "ALL"),
    ("bass", "BASS_RHYTHM", "bass_rhythm_bag", "BASS"),
    ("chord", "CHORD_RHYTHM", "chord_rhythm_bag", "CHORD"),
    ("progressions", "CHORD_PROGRESSION", "progression_bag", "CHORD"),
    ("melodic", "MELODIC_RHYTHM", "melodic_rhythm_bag", "MELODIC_SECONDARY"),
    ("motifs", "MOTIF_SHAPE", "motif_shape_bag", "MELODIC_SECONDARY"),
]

TONAL_FIELDS = [
    (
        "TONAL_BASS_CONTOUR",
        ("bass_allowed_contours_mask", "bass_preferred_contours_mask"),
        "bass_contour_allowed_and_preferred",
        "BASS",
    ),
    (
        "TONAL_BASS_ARTICULATION",
        ("bass_allowed_articulations_mask", "bass_preferred_articulations_mask"),
        "bass_articulation_allowed_and_preferred",
        "BASS",
    ),
    (
        "TONAL_MELODIC_RHYTHM_OPERATION",
        ("melodic_allowed_rhythm_ops_mask", "melodic_preferred_rhythm_ops_mask"),
        "melodic_rhythm_operations",
        "MELODIC_SECONDARY",
    ),
    (
        "TONAL_MELODIC_CONTOUR",
        ("melodic_allowed_contours_mask", "melodic_preferred_contours_mask"),
        "melodic_contour_allowed_and_preferred",
        "MELODIC_SECONDARY",
    ),
    (
        "TONAL_MELODIC_MOTIF_OPERATION",
        ("melodic_allowed_motif_ops_mask", "melodic_preferred_motif_ops_mask"),
        "melodic_motif_operations",
        "MELODIC_SECONDARY",
    ),
    (
        "TONAL_BASS_REGISTER",
        ("bass_register_min", "bass_register_max", "bass_register_max_leap"),
        "bass_register_corridor",
        "BASS",
    ),
    (
        "TONAL_SECONDARY_REGISTER",
        (
            "secondary_register_min",
            "secondary_register_max",
            "secondary_register_max_leap",
        ),
        "secondary_register_corridor",
        "CHORD_MELODIC",
    ),
]

BASE_COLUMNS = [
    "exact_base",
    "c1rf_head",
    "genre_a",
    "genre_b",
    "original_c1_classification",
    "classification",
    "declared_difference_domains",
    "reachable_difference_domains",
    "unreachable_difference_domains",
    "unreliable_difference_domains",
    "unmapped_trace_domains",
    "shared_single_option_domains",
    "shared_policy_restrictions",
    "owner_unresolved_domains",
    "failure_sensitive_domains",
    "policy_inconsistency_domains",
    "original_c1_changed_domains",
    "fallback_trace_difference_domains",
    "notes",
]

RECIPE_COLUMNS = [
    "exact_base",
    "c1rf_head",
    "genre",
    "recipe_id",
    "recipe",
    "original_c1_classification",
    "classification",
    "declared_difference_domains",
    "reachable_difference_domains",
    "unreachable_difference_domains",
    "unreliable_difference_domains",
    "unmapped_trace_domains",
    "shared_single_option_domains",
    "shared_policy_restrictions",
    "owner_unresolved_domains",
    "failure_sensitive_domains",
    "policy_inconsistency_domains",
    "fallback_trace_difference_domains",
    "runtime_trace_difference_domains",
    "notes",
]


@dataclass(frozen=True)
class Difference:
    token: str
    reachability: str


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def render_tsv(columns: list[str], rows: list[dict[str, str]]) -> str:
    output = io.StringIO(newline="")
    writer = csv.DictWriter(
        output,
        fieldnames=columns,
        delimiter="\t",
        lineterminator="\n",
    )
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue()


def reachability_index(rows: list[dict[str, str]]) -> dict[tuple[str, str], str]:
    result: dict[tuple[str, str], str] = {}
    for row in rows:
        key = (row["semantic_field"], row["role"])
        require(key not in result, f"duplicate C1RF reachability key: {key}")
        result[key] = row["status"]
    return result


def status_for(
    index: dict[tuple[str, str], str], semantic_field: str, role: str
) -> str:
    key = (semantic_field, role)
    require(key in index, f"unmapped C1 domain: {semantic_field}/{role}")
    status = index[key]
    require(
        status in CONNECTED | UNREACHABLE | UNRELIABLE,
        f"unsupported C1RF status for {key}: {status}",
    )
    return status


def candidate_support(value: str) -> tuple[int, ...]:
    if not value:
        return ()
    return tuple(
        sorted(int(entry.split(":", 1)[0]) for entry in value.split(";"))
    )


def list_value(values: list[str] | set[str]) -> str:
    ordered = sorted(set(values))
    return ";".join(ordered) if ordered else "NONE"


def split_list(value: str) -> set[str]:
    if not value or value == "NONE":
        return set()
    return set(value.split(";"))


def bag_difference(
    left: dict[str, str],
    right: dict[str, str],
    field: str,
    domain: str,
    semantic_field: str,
    role: str,
    reachability: dict[tuple[str, str], str],
) -> Difference | None:
    if left[field] == right[field]:
        return None
    kind = (
        "WEIGHTS"
        if candidate_support(left[field]) == candidate_support(right[field])
        else "MEMBERSHIP"
    )
    return Difference(
        f"{domain}:{kind}", status_for(reachability, semantic_field, role)
    )


def phrase_differences(
    left: dict[str, str],
    right: dict[str, str],
    reachability: dict[tuple[str, str], str],
) -> list[Difference]:
    if left["phrases"] == right["phrases"]:
        return []

    kind = (
        "WEIGHTS"
        if candidate_support(left["phrases"]) == candidate_support(right["phrases"])
        else "MEMBERSHIP"
    )
    result = [
        Difference(
            f"PHRASE_EVOLUTION:{kind}",
            status_for(reachability, "profile_phrase_law", "ALL"),
        )
    ]

    left_lengths = {
        identity & 0x0F for identity in candidate_support(left["phrases"])
    }
    right_lengths = {
        identity & 0x0F for identity in candidate_support(right["phrases"])
    }
    if left_lengths != right_lengths:
        result.append(
            Difference(
                "PHRASE_LENGTH:MEMBERSHIP",
                status_for(reachability, "profile_phrase_bars", "ALL"),
            )
        )
    return result


def suggested_bpm_reachability(
    left: dict[str, str],
    right: dict[str, str],
    reachability: dict[tuple[str, str], str],
) -> str:
    catalog_status = status_for(reachability, "suggested_bpm", "TEMPO")
    require(
        catalog_status == "PARTIALLY_CONNECTED",
        f"unexpected catalog suggested-BPM status: {catalog_status}",
    )
    # C1RF's catalog-wide partial status is mode-specific: the non-Atlas branch
    # consumes requestedBpm, while Atlas-backed generation replaces it with
    # AtlasRuntimeMetadata.bpm. C1DF can resolve that branch from each C1F row.
    if left["atlas_backed"] == "1" or right["atlas_backed"] == "1":
        return catalog_status
    return "CONNECTED"


def profile_differences(
    left: dict[str, str],
    right: dict[str, str],
    reachability: dict[tuple[str, str], str],
) -> list[Difference]:
    differences: list[Difference] = []
    for field, domain, semantic_field, role in BAG_FIELDS:
        difference = bag_difference(
            left,
            right,
            field,
            domain,
            semantic_field,
            role,
            reachability,
        )
        if difference is not None:
            differences.append(difference)

    differences.extend(phrase_differences(left, right, reachability))

    if left["bpm_suggested"] != right["bpm_suggested"]:
        differences.append(
            Difference(
                "CORRIDOR_SUGGESTED_BPM",
                suggested_bpm_reachability(left, right, reachability),
            )
        )
    if (left["bpm_min"], left["bpm_max"]) != (
        right["bpm_min"],
        right["bpm_max"],
    ):
        differences.append(
            Difference(
                "CORRIDOR_BPM_BOUNDS",
                status_for(reachability, "bpm_min_and_max", "TEMPO"),
            )
        )
    if (left["density_min"], left["density_max"]) != (
        right["density_min"],
        right["density_max"],
    ):
        differences.append(
            Difference(
                "CORRIDOR_DENSITY",
                status_for(reachability, "density_min_and_max", "ALL"),
            )
        )
    if left["grid_steps"] != right["grid_steps"]:
        differences.append(
            Difference(
                "CORRIDOR_GRID",
                status_for(reachability, "grid_steps", "ALL"),
            )
        )
    if left["secondary_role"] != right["secondary_role"]:
        differences.append(
            Difference(
                "SECONDARY_ROLE",
                status_for(reachability, "secondary_role", "CHORD_MELODIC"),
            )
        )

    for domain, fields, semantic_field, role in TONAL_FIELDS:
        if any(left[field] != right[field] for field in fields):
            differences.append(
                Difference(domain, status_for(reachability, semantic_field, role))
            )

    if left["canonical_drum_fingerprint"] != right["canonical_drum_fingerprint"]:
        differences.append(
            Difference(
                "DRUM_POLICY",
                status_for(
                    reachability, "weighted_archetype_drum_grammar", "DRUMS"
                ),
            )
        )

    require(
        len({difference.token for difference in differences}) == len(differences),
        f"duplicate difference token for {left['genre_key']} vs {right['genre_key']}",
    )
    return sorted(differences, key=lambda item: item.token)


def classify(differences: list[Difference], extra_unreliable: bool) -> list[str]:
    tags: list[str] = []
    if not differences:
        return ["DECLARATIVE_COLLISION"]

    connected = [item for item in differences if item.reachability in CONNECTED]
    unavailable = [item for item in differences if item.reachability in UNREACHABLE]
    unreliable = [item for item in differences if item.reachability in UNRELIABLE]

    if connected:
        tags.append("REACHABLE_POLICY_DIFFERENCE")
        if unavailable or unreliable or extra_unreliable:
            tags.append("MIXED_REACHABILITY")
    else:
        tags.append("REACHABILITY_DEPENDENT")
    if any(item.reachability == "PARTIALLY_CONNECTED" for item in differences):
        tags.append("PARTIALLY_REACHABLE")
    return tags


def shared_restrictions(
    left: dict[str, str], right: dict[str, str]
) -> tuple[str, str]:
    shared = split_list(left["single_option_axes"]) & split_list(
        right["single_option_axes"]
    )
    restrictions = set(shared)
    if left["tonal_source"] == right["tonal_source"]:
        restrictions.add(f"tonal.source={left['tonal_source']}")
    return list_value(shared), list_value(restrictions)


def fallback_trace_differences(
    left: dict[str, str], right: dict[str, str]
) -> list[str]:
    fields = [
        ("legacy_params_fingerprint", "LEGACY_PARAMS"),
        ("legacy_drum_fingerprint", "LEGACY_DRUM"),
        ("legacy_behavior_fingerprint", "LEGACY_BEHAVIOR"),
        ("legacy_timbre_fingerprint", "LEGACY_TIMBRE"),
    ]
    return [label for field, label in fields if left[field] != right[field]]


def runtime_trace_differences(
    left: dict[str, str], right: dict[str, str]
) -> list[str]:
    return (
        ["ATLAS_METADATA"]
        if left["atlas_metadata_fingerprint"] != right["atlas_metadata_fingerprint"]
        else []
    )


def domain_lists(differences: list[Difference]) -> tuple[str, str, str, str]:
    declared = list_value([item.token for item in differences])
    reachable = list_value(
        [item.token for item in differences if item.reachability in CONNECTED]
    )
    unreachable = list_value(
        [item.token for item in differences if item.reachability in UNREACHABLE]
    )
    unreliable = list_value(
        [item.token for item in differences if item.reachability in UNRELIABLE]
    )
    return declared, reachable, unreachable, unreliable


def owner_ambiguity(
    differences: list[Difference], reachability: dict[tuple[str, str], str]
) -> list[str]:
    tokens = {item.token.split(":", 1)[0] for item in differences}
    if not ({"CHORD_RHYTHM", "CHORD_PROGRESSION"} & tokens):
        return []
    status = status_for(reachability, "harmonic_event_timing", "CHORD_BASS_MELODIC")
    require(status == "CONNECTED", f"unexpected harmonic owner status: {status}")
    return []


def failure_sensitivity(
    differences: list[Difference], reachability: dict[tuple[str, str], str]
) -> list[str]:
    if not differences:
        return []
    masked = status_for(
        reachability, "normal_genre_strong_migration_result", "ALL"
    )
    fallback = status_for(reachability, "upstream_material_fallback", "ALL")
    require(masked == "FAILURE_MASKED", f"unexpected migration status: {masked}")
    require(fallback == "LEGACY_FALLBACK", f"unexpected fallback status: {fallback}")
    return ["NORMAL_GENRE_STRONG_MIGRATION_PUBLICATION"]


def policy_inconsistency(
    left: dict[str, str],
    right: dict[str, str],
    reachability: dict[tuple[str, str], str],
) -> list[str]:
    identities = {(left["genre_key"], left["recipe_id"]), (right["genre_key"], right["recipe_id"])}
    if ("Reggae", "11") not in identities:
        return []
    status = status_for(
        reachability, "minimal_space_atlas_bpm_vs_corridor", "ALL"
    )
    require(status == "DUPLICATE_OWNER", f"unexpected Minimal Space status: {status}")
    return ["MINIMAL_SPACE_TEMPO"]


def comparison_notes(
    differences: list[Difference],
    owner_domains: list[str],
    policy_domains: list[str],
) -> str:
    reachable_roots = {
        item.token.split(":", 1)[0]
        for item in differences
        if item.reachability in CONNECTED
    }
    notes = ["GF2_C2=BLOCKED"]
    if reachable_roots == {"RHYTHM_COMPATIBILITY", "DRUM_POLICY"}:
        notes.append("REACHABLE_ONLY_VIA_RHYTHM_AND_DRUM")
    if owner_domains:
        notes.append("OWNER_INTERPRETATION_UNRESOLVED")
    if policy_domains:
        notes.append("DECLARATIVE_POLICY_INCONSISTENCY_OPEN")
    return ";".join(notes)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail if checked-in artifacts differ from deterministic output",
    )
    args = parser.parse_args()

    census = read_tsv(CENSUS_PATH)
    c1_pairs = read_tsv(C1_PAIR_PATH)
    reachability_rows = read_tsv(REACHABILITY_PATH)
    reachability = reachability_index(reachability_rows)

    require(len(census) == 33, f"expected 33 C1 profiles, found {len(census)}")
    bases = [row for row in census if row["is_base"] == "1"]
    recipes = [row for row in census if row["is_base"] == "0"]
    require(len(bases) == 16, f"expected 16 BASE profiles, found {len(bases)}")
    require(len(recipes) == 17, f"expected 17 recipe profiles, found {len(recipes)}")
    require(len(c1_pairs) == 120, f"expected 120 C1 BASE pairs, found {len(c1_pairs)}")
    require(len(reachability_rows) == 39, "expected 39 C1RF reachability traces")
    require(all(row["exact_base"] == GF2_BASE for row in census), "C1 base mismatch")
    require(
        status_for(reachability, "profile_phrase_law", "ALL") == "DROPPED"
        and status_for(reachability, "profile_phrase_bars", "ALL")
        == "PARTIALLY_CONNECTED"
        and status_for(reachability, "explicit_phrase_length_request", "ALL")
        == "CONNECTED",
        "final phrase reachability contract mismatch",
    )

    c1_pair_by_key = {
        (row["genre_a"], row["genre_b"]): row for row in c1_pairs
    }
    require(len(c1_pair_by_key) == 120, "duplicate C1 BASE pair")

    base_rows: list[dict[str, str]] = []
    for left_index, left in enumerate(bases):
        for right in bases[left_index + 1 :]:
            key = (left["genre_key"], right["genre_key"])
            require(key in c1_pair_by_key, f"C1 BASE pair missing: {key}")
            c1_pair = c1_pair_by_key[key]
            differences = profile_differences(left, right, reachability)
            declared, reachable, unreachable, unreliable = domain_lists(differences)
            owner_domains = owner_ambiguity(differences, reachability)
            failure_domains = failure_sensitivity(differences, reachability)
            policy_domains = policy_inconsistency(left, right, reachability)
            fallbacks = fallback_trace_differences(left, right)
            runtime = runtime_trace_differences(left, right)
            classes = classify(
                differences,
                extra_unreliable=bool(owner_domains or policy_domains),
            )
            if failure_domains:
                classes.append("FAILURE_SENSITIVE")
            if owner_domains:
                classes.append("OWNER_UNRESOLVED")
            if policy_domains:
                classes.append("POLICY_INCONSISTENCY_AFFECTED")
            single, restrictions = shared_restrictions(left, right)
            base_rows.append(
                {
                    "exact_base": GF2_BASE,
                    "c1rf_head": C1RF_HEAD,
                    "genre_a": left["genre_display"],
                    "genre_b": right["genre_display"],
                    "original_c1_classification": c1_pair["classification"],
                    "classification": ";".join(classes),
                    "declared_difference_domains": declared,
                    "reachable_difference_domains": reachable,
                    "unreachable_difference_domains": unreachable,
                    "unreliable_difference_domains": unreliable,
                    "unmapped_trace_domains": list_value(runtime),
                    "shared_single_option_domains": single,
                    "shared_policy_restrictions": restrictions,
                    "owner_unresolved_domains": list_value(owner_domains),
                    "failure_sensitive_domains": list_value(failure_domains),
                    "policy_inconsistency_domains": list_value(policy_domains),
                    "original_c1_changed_domains": c1_pair["changed_domains"],
                    "fallback_trace_difference_domains": list_value(fallbacks),
                    "notes": comparison_notes(
                        differences, owner_domains, policy_domains
                    ),
                }
            )

    base_by_genre = {row["genre_id"]: row for row in bases}
    require(len(base_by_genre) == 16, "duplicate BASE genre identity")
    recipe_rows: list[dict[str, str]] = []
    for recipe in recipes:
        require(recipe["genre_id"] in base_by_genre, "recipe has no own BASE")
        base = base_by_genre[recipe["genre_id"]]
        differences = profile_differences(base, recipe, reachability)
        declared, reachable, unreachable, unreliable = domain_lists(differences)
        owner_domains = owner_ambiguity(differences, reachability)
        failure_domains = failure_sensitivity(differences, reachability)
        policy_domains = policy_inconsistency(base, recipe, reachability)
        fallbacks = fallback_trace_differences(base, recipe)
        runtime = runtime_trace_differences(base, recipe)
        classes = classify(
            differences,
            extra_unreliable=bool(owner_domains or policy_domains),
        )
        if failure_domains:
            classes.append("FAILURE_SENSITIVE")
        if owner_domains:
            classes.append("OWNER_UNRESOLVED")
        if policy_domains:
            classes.append("POLICY_INCONSISTENCY_AFFECTED")
        single, restrictions = shared_restrictions(base, recipe)
        recipe_rows.append(
            {
                "exact_base": GF2_BASE,
                "c1rf_head": C1RF_HEAD,
                "genre": recipe["genre_display"],
                "recipe_id": recipe["recipe_id"],
                "recipe": recipe["recipe_name"],
                "original_c1_classification": recipe["classification_vs_base"],
                "classification": ";".join(classes),
                "declared_difference_domains": declared,
                "reachable_difference_domains": reachable,
                "unreachable_difference_domains": unreachable,
                "unreliable_difference_domains": unreliable,
                "unmapped_trace_domains": list_value(runtime),
                "shared_single_option_domains": single,
                "shared_policy_restrictions": restrictions,
                "owner_unresolved_domains": list_value(owner_domains),
                "failure_sensitive_domains": list_value(failure_domains),
                "policy_inconsistency_domains": list_value(policy_domains),
                "fallback_trace_difference_domains": list_value(fallbacks),
                "runtime_trace_difference_domains": list_value(runtime),
                "notes": comparison_notes(
                    differences, owner_domains, policy_domains
                ),
            }
        )

    require(len(base_rows) == 120, "generated BASE pair count mismatch")
    require(len(recipe_rows) == 17, "generated recipe comparison count mismatch")

    base_text = render_tsv(BASE_COLUMNS, base_rows)
    recipe_text = render_tsv(RECIPE_COLUMNS, recipe_rows)
    outputs = [(BASE_OUTPUT, base_text), (RECIPE_OUTPUT, recipe_text)]
    if args.check:
        mismatches = []
        for path, expected in outputs:
            actual = path.read_text(encoding="utf-8") if path.exists() else ""
            if actual != expected:
                mismatches.append(str(path.relative_to(ROOT)))
        if mismatches:
            print(
                "GF2-C1DF artifacts are stale: " + ", ".join(mismatches),
                file=sys.stderr,
            )
            return 1
    else:
        for path, text in outputs:
            path.write_text(text, encoding="utf-8")

    print(
        f"GF2-C1DF dependency maps: {len(base_rows)} BASE pairs, "
        f"{len(recipe_rows)} recipe comparisons"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
