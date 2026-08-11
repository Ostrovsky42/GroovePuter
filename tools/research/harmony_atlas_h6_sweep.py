#!/usr/bin/env python3
from __future__ import annotations

import argparse
import itertools
import json
import sys
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import harmony_atlas_h6 as h6

PHASE1_LIMITS = {
    "max_harmonic_templates": 8,
    "max_rhythm_grammars": 4,
    "max_capability_complexity_points": 16,
    "max_reference_payload_bytes": 160,
    "require_distinct_rhythm_macro_families": True,
}

HARMONY_OPTIONAL = [
    h6.TRIAD_POLARITY,
    h6.EXTRA_QUALITY,
    h6.ALTERED_REACH,
    h6.HARMONIC_GT8,
]
RHYTHM_OPTIONAL = [h6.RETRIGGER, h6.RHYTHM_GT4, h6.ONSETS_GT8]
HARMONIC_BUDGETS = [4, 8]
STYLE_ORDER = ["default", "pop", "pop2", "hiphop2", "soul"]
RHYTHM_MACRO_FAMILY = {
    "default": "HELD",
    "pop": "ASYMMETRIC_CHANGE",
    "pop2": "ASYMMETRIC_CHANGE",
    "hiphop2": "GAPPED_RETRIGGER",
    "soul": "RETRIGGERED_COMP",
}


class SweepError(RuntimeError):
    pass


def subsets(values: list[str], *, include_empty: bool = True) -> list[tuple[str, ...]]:
    result: list[tuple[str, ...]] = []
    start = 0 if include_empty else 1
    for size in range(start, len(values) + 1):
        result.extend(itertools.combinations(values, size))
    return result


def compact(row: dict[str, Any], sweep_id: str) -> dict[str, Any]:
    proposal = row["bounded_r2_proposal"]
    envelope = row["capability_envelope"]
    cost = row["reference_cost"]
    macros = [RHYTHM_MACRO_FAMILY[style] for style in row["rhythm_style_candidates"]]
    return {
        "sweep_id": sweep_id,
        "capabilities": row["capabilities"],
        "harmonic_template_budget": row["harmonic_template_budget"],
        "rhythm_style_candidates": row["rhythm_style_candidates"],
        "rhythm_macro_families": macros,
        "rhythm_grammar_count": len(row["rhythm_style_candidates"]),
        "rhythm_macro_family_count": len(set(macros)),
        "proposal": {
            "exact_unique_f3": proposal["exact_unique_f3"],
            "exact_unique_f5": proposal["exact_unique_f5"],
            "exact_unique_f6": proposal["exact_unique_f6"],
            "exact_f5_observations": proposal["exact_f5_observations"],
            "exact_f6_observations": proposal["exact_f6_observations"],
        },
        "envelope": {
            "exact_unique_f3": envelope["exact_unique_f3"],
            "exact_unique_f5": envelope["exact_unique_f5"],
            "exact_unique_f6": envelope["exact_unique_f6"],
        },
        "cost": {
            "candidate_payload_bytes": cost["total_candidate_payload_bytes"],
            "capability_complexity_points": cost["capability_complexity_points"],
        },
    }


def dominates(a: dict[str, Any], b: dict[str, Any]) -> bool:
    ap, bp = a["proposal"], b["proposal"]
    ac, bc = a["cost"], b["cost"]
    no_worse = (
        ap["exact_unique_f3"] >= bp["exact_unique_f3"]
        and ap["exact_unique_f5"] >= bp["exact_unique_f5"]
        and ap["exact_unique_f6"] >= bp["exact_unique_f6"]
        and ac["candidate_payload_bytes"] <= bc["candidate_payload_bytes"]
        and ac["capability_complexity_points"] <= bc["capability_complexity_points"]
    )
    strict = (
        ap["exact_unique_f3"] > bp["exact_unique_f3"]
        or ap["exact_unique_f5"] > bp["exact_unique_f5"]
        or ap["exact_unique_f6"] > bp["exact_unique_f6"]
        or ac["candidate_payload_bytes"] < bc["candidate_payload_bytes"]
        or ac["capability_complexity_points"] < bc["capability_complexity_points"]
    )
    return no_worse and strict


def pareto(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    frontier: list[dict[str, Any]] = []
    for row in rows:
        if any(dominates(other, row) for other in frontier):
            continue
        frontier = [other for other in frontier if not dominates(row, other)]
        frontier.append(row)
    return sorted(
        frontier,
        key=lambda row: (
            -row["proposal"]["exact_unique_f6"],
            -row["proposal"]["exact_unique_f5"],
            -row["proposal"]["exact_unique_f3"],
            row["cost"]["capability_complexity_points"],
            row["cost"]["candidate_payload_bytes"],
            row["sweep_id"],
        ),
    )


def phase1_ok(row: dict[str, Any]) -> bool:
    return (
        row["harmonic_template_budget"] <= PHASE1_LIMITS["max_harmonic_templates"]
        and row["rhythm_grammar_count"] <= PHASE1_LIMITS["max_rhythm_grammars"]
        and row["rhythm_macro_family_count"] == row["rhythm_grammar_count"]
        and row["cost"]["capability_complexity_points"] <= PHASE1_LIMITS["max_capability_complexity_points"]
        and row["cost"]["candidate_payload_bytes"] <= PHASE1_LIMITS["max_reference_payload_bytes"]
    )


def phase1_rank(row: dict[str, Any]) -> tuple[Any, ...]:
    # Predeclared objective after applying the musical-diversity gate: maximize
    # combined exact F6 first, then F5/F3, then lower implementation complexity
    # and smaller reference payload. Exact-count cannot buy duplicate macro slots.
    return (
        -row["proposal"]["exact_unique_f6"],
        -row["proposal"]["exact_unique_f5"],
        -row["proposal"]["exact_unique_f3"],
        row["cost"]["capability_complexity_points"],
        row["cost"]["candidate_payload_bytes"],
        row["rhythm_grammar_count"],
        row["sweep_id"],
    )


def build_sweep(h1: dict[str, Any], h3: dict[str, Any], h4: dict[str, Any], h5_report: dict[str, Any]) -> dict[str, Any]:
    h6.require_inputs(h1, h3, h4, h5_report)
    groups = h6.harmonic_groups(h1, h3, h5_report)
    rows: list[dict[str, Any]] = []
    counter = 0

    harmony_sets = subsets(HARMONY_OPTIONAL)
    rhythm_sets = subsets(RHYTHM_OPTIONAL)
    style_sets = subsets(STYLE_ORDER, include_empty=False)

    for harmony_optional in harmony_sets:
        for rhythm_optional in rhythm_sets:
            capabilities = {
                h6.QUALITY_RENDER,
                h6.MULTI_BAR,
                *harmony_optional,
                *rhythm_optional,
            }
            for budget in HARMONIC_BUDGETS:
                for styles in style_sets:
                    counter += 1
                    spec = {
                        "capabilities": sorted(capabilities),
                        "harmonic_template_budget": budget,
                        "rhythm_styles": list(styles),
                    }
                    full = h6.bundle_report(f"SWEEP_{counter:05d}", spec, groups, h4)
                    rows.append(compact(full, f"S{counter:05d}"))

    phase1 = [row for row in rows if phase1_ok(row)]
    phase1_sorted = sorted(phase1, key=phase1_rank)
    global_frontier = pareto(rows)
    phase1_frontier = pareto(phase1)

    top_full: list[dict[str, Any]] = []
    for compact_row in phase1_sorted[:10]:
        spec = {
            "capabilities": compact_row["capabilities"],
            "harmonic_template_budget": compact_row["harmonic_template_budget"],
            "rhythm_styles": compact_row["rhythm_style_candidates"],
        }
        full = h6.bundle_report(compact_row["sweep_id"], spec, groups, h4)
        full["sweep_id"] = compact_row["sweep_id"]
        full["rhythm_macro_families"] = compact_row["rhythm_macro_families"]
        top_full.append(full)

    return {
        "stage": "H6_BOUNDED_BUNDLE_SWEEP",
        "search_space": {
            "evaluated_bundle_count": len(rows),
            "harmony_optional_capabilities": HARMONY_OPTIONAL,
            "rhythm_optional_capabilities": RHYTHM_OPTIONAL,
            "harmonic_template_budgets": HARMONIC_BUDGETS,
            "rhythm_style_candidates": STYLE_ORDER,
            "rhythm_macro_family_map": RHYTHM_MACRO_FAMILY,
            "quality_render_always_enabled": True,
            "multi_bar_always_enabled": True,
        },
        "phase1_limits": PHASE1_LIMITS,
        "phase1_candidate_count": len(phase1),
        "global_pareto_count": len(global_frontier),
        "phase1_pareto_count": len(phase1_frontier),
        "phase1_rank_objective": "MAX_F6_THEN_F5_THEN_F3_THEN_MIN_COMPLEXITY_THEN_MIN_REFERENCE_PAYLOAD_AFTER_MACRO_DIVERSITY_GATE",
        "top_phase1": top_full,
        "phase1_pareto": phase1_frontier,
        "global_pareto": global_frontier,
        "selection_status": "MEASURED_NOT_YET_HUMAN_REVIEWED",
    }


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Exhaustive bounded H6 candidate bundle sweep")
    p.add_argument("--normalization", type=Path, required=True)
    p.add_argument("--functional", type=Path, required=True)
    p.add_argument("--rhythm", type=Path, required=True)
    p.add_argument("--representability", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    return p.parse_args()


def main() -> int:
    args = parse_args()
    h1, _ = h6.load_verified_json(args.normalization, h6.EXPECTED_H1_SHA256, "H1")
    h3, _ = h6.load_verified_json(args.functional, h6.EXPECTED_H3_SHA256, "H3")
    h4, _ = h6.load_verified_json(args.rhythm, h6.EXPECTED_H4_SHA256, "H4")
    h5_report, _ = h6.load_verified_json(args.representability, h6.EXPECTED_H5_SHA256, "H5")
    report = build_sweep(h1, h3, h4, h5_report)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
