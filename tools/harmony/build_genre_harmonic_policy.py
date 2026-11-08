#!/usr/bin/env python3
"""Validate and emit the bounded HGS style-policy table."""

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
STYLE_ENUMS = [
    "Acid", "DetroitTechno", "Electro", "EBM", "UkGarage", "DubTechno",
    "Ambient", "LoFi", "Chillout", "DeepHouse", "Jungle", "Trap",
    "HipHop", "Footwork", "DnB", "Salsa", "Synthwave",
]
STYLE_ORDER = [
    "Acid", "Detroit Techno", "Electro", "EBM", "UK Garage", "Dub Techno",
    "Ambient", "Lo-Fi", "Chillout", "Deep House", "Jungle", "Trap",
    "Hip-Hop", "Footwork", "DnB", "Salsa", "Synthwave",
]
HOST_ONLY = {
    "Detroit Techno", "EBM", "Ambient", "Chillout", "Deep House", "Jungle",
    "Trap", "Salsa",
}


def _mask(ids):
    return sum(1 << value for value in ids)


def _feature_weights(weights):
    dia, prl, functional, modal, chromatic, augmented, _poly, mobility, home = weights
    return {
        "common_tone": 2 + prl,
        "pcvl_penalty": 1 + home,
        "fifths_distance_penalty": functional,
        "out_of_scale_penalty": 5 - chromatic,
        "large_root_motion_penalty": 5 - mobility,
        "diatonic_tag": dia * 4,
        "prl_tag": prl * 3,
        "functional_tag": functional * 4,
        "modal_tag": modal * 4,
        "chromatic_mediant_tag": chromatic * 2,
        "portal_tag": augmented * 4,
        "interval_vector": 1,
        "complexity": 2,
        "corpus": 2,
    }


def compile_policy(source, quality_registry, scale_registry):
    if source.get("schema_version") != 1:
        raise ValueError("unsupported genre policy source schema")
    if source.get("style_order") != STYLE_ORDER:
        raise ValueError("genre style order does not match stable HarmonicStyleId IDs")
    quality_count = quality_registry.get("runtime_bootstrap_count")
    scale_count = scale_registry.get("runtime_bootstrap_count")
    if not isinstance(quality_count, int) or not 0 < quality_count <= len(quality_registry["qualities"]):
        raise ValueError("quality runtime capability prefix is invalid")
    if not isinstance(scale_count, int) or not 0 < scale_count <= len(scale_registry["scales"]):
        raise ValueError("scale runtime capability prefix is invalid")

    quality_tiers = source.get("quality_tier_runtime_ids")
    scale_tiers = source.get("scale_tier_runtime_ids")
    if not isinstance(quality_tiers, dict) or not isinstance(scale_tiers, dict):
        raise ValueError("quality and scale tier tables are required")
    for tiers, count, label in ((quality_tiers, quality_count, "quality"),
                                (scale_tiers, scale_count, "scale")):
        if set(tiers) != {"0", "1", "2", "3"}:
            raise ValueError(f"{label} tier table must contain exactly tiers 0..3")
        for tier, ids in tiers.items():
            if ids != sorted(set(ids)) or not ids or any(
                not isinstance(value, int) or value < 0 or value >= count for value in ids
            ):
                raise ValueError(f"{label} tier {tier} exceeds downstream capability")

    rows = source.get("profiles")
    if not isinstance(rows, list) or len(rows) != 17:
        raise ValueError("exactly 17 explicit style profiles are required")
    rows = sorted(rows, key=lambda row: row.get("id", -1))
    compiled = []
    for expected_id, row in enumerate(rows):
        if row.get("id") != expected_id or row.get("name") != STYLE_ORDER[expected_id]:
            raise ValueError("profile IDs/names must match the append-only style registry")
        weights = row.get("weights")
        if not isinstance(weights, list) or len(weights) != 9 or any(
            not isinstance(value, int) or not 0 <= value <= 4 for value in weights
        ):
            raise ValueError(f"{row['name']} weights must contain nine integers in 0..4")
        q_tier = row.get("quality_tier")
        s_tier = row.get("scale_tier")
        if q_tier not in range(4) or s_tier not in range(4):
            raise ValueError(f"{row['name']} has an invalid vocabulary tier")
        q_ids = quality_tiers[str(q_tier)]
        s_ids = scale_tiers[str(s_tier)]
        budgets = ("outside_take_keep", "outside_develop", "outside_break",
                   "modulations_develop", "modulations_break", "color_budget",
                   "changes_per_section")
        for name in budgets:
            value = row.get(name)
            limit = 4 if name.startswith("outside_") else 2 if name.startswith("modulations_") else 4
            if not isinstance(value, int) or not 0 <= value <= limit:
                raise ValueError(f"{row['name']} {name} outside allowed range 0..{limit}")
        for name in ("allow_portals", "functional_preparation", "host_profile_only"):
            if not isinstance(row.get(name), bool):
                raise ValueError(f"{row['name']} {name} must be explicit boolean capability")
        if row["outside_take_keep"] > 1 or row["changes_per_section"] < 1:
            raise ValueError(f"{row['name']} violates TAKE/KEEP or section-change hard limits")
        if row["host_profile_only"] != (row["name"] in HOST_ONLY):
            raise ValueError(f"{row['name']} host-profile-only contract changed")
        if expected_id == 0 and row["functional_preparation"]:
            raise ValueError("Acid must not enable functional-preparation grammar")

        item = dict(row)
        item["quality_ids"] = list(q_ids)
        item["scale_ids"] = list(s_ids)
        item["quality_mask"] = _mask(q_ids)
        item["scale_mask"] = _mask(s_ids)
        item["feature_weights"] = _feature_weights(weights)
        item["enum_name"] = STYLE_ENUMS[expected_id]
        compiled.append(item)

    # The sorted runtime lists must be subsets of the actual bootstrap prefixes,
    # and the source declares that the wider offline vocabulary was clipped.
    if not source.get("quality_grouping_capability_clipped") or not source.get(
        "scale_grouping_capability_clipped"
    ):
        raise ValueError("runtime capability clipping must remain explicit")
    canonical = {
        "schema_version": source["schema_version"],
        "quality_tier_runtime_ids": quality_tiers,
        "scale_tier_runtime_ids": scale_tiers,
        "profiles": compiled,
    }
    encoded = json.dumps(canonical, sort_keys=True, separators=(",", ":")) + "\n"
    return {
        "schema_version": 1,
        "content_sha256": hashlib.sha256(encoded.encode()).hexdigest(),
        "quality_runtime_count": quality_count,
        "scale_runtime_count": scale_count,
        "profiles": compiled,
    }


def render_header(compiled):
    lines = [
        "#ifndef GROOVEPUTER_GENERATION_HARMONY_GENERATED_GENRE_HARMONIC_POLICY_DATA_H",
        "#define GROOVEPUTER_GENERATION_HARMONY_GENERATED_GENRE_HARMONIC_POLICY_DATA_H",
        "",
        '#include "../genre_harmonic_policy.h"',
        "",
        "namespace GroovePuterHarmony {",
        "namespace GeneratedGenrePolicy {",
        "",
        "inline constexpr GenreHarmonicPolicy kPolicies[] = {",
    ]
    for row in compiled["profiles"]:
        fw = row["feature_weights"]
        lines += [
            "    {",
            f"        HarmonicStyleId::{row['enum_name']}, {row['quality_tier']}, "
            f"{row['scale_tier']}, 0x{row['quality_mask']:08x}u, "
            f"0x{row['scale_mask']:08x}u,",
            "        {" + ", ".join(str(value) for value in row["weights"]) + "},",
            "        {" + ", ".join(str(fw[name]) for name in (
                "common_tone", "pcvl_penalty", "fifths_distance_penalty",
                "out_of_scale_penalty", "large_root_motion_penalty", "diatonic_tag",
                "prl_tag", "functional_tag", "modal_tag", "chromatic_mediant_tag",
                "portal_tag", "interval_vector", "complexity", "corpus")) + "},",
            f"        {row['outside_take_keep']}, {row['outside_develop']}, "
            f"{row['outside_break']}, {row['modulations_develop']}, "
            f"{row['modulations_break']}, {1 if row['allow_portals'] else 0}, "
            f"{row['color_budget']}, {row['changes_per_section']}, "
            f"{1 if row['functional_preparation'] else 0}, "
            f"{1 if row['host_profile_only'] else 0}",
            "    },",
        ]
    lines += [
        "};",
        "inline constexpr uint8_t kPolicyCount = "
        "static_cast<uint8_t>(sizeof(kPolicies) / sizeof(kPolicies[0]));",
        "inline constexpr char kPolicyContentSha256[] = \"" +
        compiled["content_sha256"] + "\";",
        "",
        "}  // namespace GeneratedGenrePolicy",
        "}  // namespace GroovePuterHarmony",
        "",
        "#endif  // GROOVEPUTER_GENERATION_HARMONY_GENERATED_GENRE_HARMONIC_POLICY_DATA_H",
        "",
    ]
    return "\n".join(lines)


def render_report(compiled):
    fields = [
        "id", "name", "host_profile_only", "weights", "quality_tier", "quality_ids",
        "scale_tier", "scale_ids", "outside_take_keep", "outside_develop",
        "outside_break", "modulations_develop", "modulations_break", "allow_portals",
        "color_budget", "changes_per_section", "functional_preparation", "feature_weights",
    ]
    return {
        "schema_version": 1,
        "content_sha256": compiled["content_sha256"],
        "quality_runtime_count": compiled["quality_runtime_count"],
        "scale_runtime_count": compiled["scale_runtime_count"],
        "quality_capability_clipped": True,
        "scale_capability_clipped": True,
        "profiles": [{field: row[field] for field in fields}
                     for row in compiled["profiles"]],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path,
                        default=ROOT / "tools/harmony/genre_policy_source.json")
    parser.add_argument("--quality-registry", type=Path,
                        default=ROOT / "tools/harmony/quality_registry.json")
    parser.add_argument("--scale-registry", type=Path,
                        default=ROOT / "tools/harmony/scale_registry.json")
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    source = json.loads(args.source.read_text(encoding="utf-8"))
    quality = json.loads(args.quality_registry.read_text(encoding="utf-8"))
    scale = json.loads(args.scale_registry.read_text(encoding="utf-8"))
    compiled = compile_policy(source, quality, scale)
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text(render_header(compiled), encoding="utf-8")
    args.report.write_text(json.dumps(render_report(compiled), indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")


if __name__ == "__main__":
    main()
