#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import harmony_atlas_h6 as h6

SELECTED_SWEEP_ID = "S00617"
RAW_COUNT_WINNER_SWEEP_ID = "S00803"
TIED_ASYMMETRIC_ALTERNATE_SWEEP_ID = "S00618"
SELECTED_SPEC = {
    "capabilities": [
        h6.MULTI_BAR,
        h6.QUALITY_RENDER,
        h6.RETRIGGER,
        h6.TRIAD_POLARITY,
    ],
    "harmonic_template_budget": 8,
    "rhythm_styles": ["default", "pop", "hiphop2", "soul"],
}
EXPECTED_SELECTED = {
    "envelope_f3": 98,
    "envelope_f5": 18,
    "envelope_f6": 465,
    "proposal_f3": 8,
    "proposal_f5": 13,
    "proposal_f5_observations": 675,
    "proposal_f6": 25,
    "proposal_f6_observations": 25,
    "payload_bytes": 148,
    "complexity_points": 12,
}


class DecisionError(RuntimeError):
    pass


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text())
    if not isinstance(value, dict):
        raise DecisionError(f"{path} must contain an object")
    return value


def spec_matches(row: dict[str, Any], spec: dict[str, Any]) -> bool:
    return (
        sorted(row["capabilities"]) == sorted(spec["capabilities"])
        and int(row["harmonic_template_budget"]) == int(spec["harmonic_template_budget"])
        and list(row["rhythm_style_candidates"]) == list(spec["rhythm_styles"])
    )


def find_compact(sweep: dict[str, Any], sweep_id: str) -> dict[str, Any]:
    for field in ("phase1_pareto", "global_pareto"):
        for row in sweep[field]:
            if row["sweep_id"] == sweep_id:
                return row
    raise DecisionError(f"cannot find {sweep_id} on a Pareto frontier")


def find_top(sweep: dict[str, Any], sweep_id: str) -> dict[str, Any]:
    for row in sweep["top_phase1"]:
        if row["sweep_id"] == sweep_id:
            return row
    raise DecisionError(f"cannot find {sweep_id} in top_phase1")


def build_decision(
    h1: dict[str, Any], h3: dict[str, Any], h4: dict[str, Any], h5_report: dict[str, Any], sweep: dict[str, Any]
) -> dict[str, Any]:
    h6.require_inputs(h1, h3, h4, h5_report)
    if sweep.get("stage") != "H6_BOUNDED_BUNDLE_SWEEP":
        raise DecisionError("H6 decision requires bounded sweep evidence")
    if sweep.get("selection_status") != "MEASURED_NOT_YET_HUMAN_REVIEWED":
        raise DecisionError("unexpected sweep selection status")
    if sweep.get("phase1_limits", {}).get("require_distinct_rhythm_macro_families") is not True:
        raise DecisionError("phase1 macro-diversity gate is not active")

    groups = h6.harmonic_groups(h1, h3, h5_report)
    selected = h6.bundle_report(SELECTED_SWEEP_ID, SELECTED_SPEC, groups, h4)
    selected["sweep_id"] = SELECTED_SWEEP_ID
    selected["rhythm_macro_families"] = ["HELD", "ASYMMETRIC_CHANGE", "GAPPED_RETRIGGER", "RETRIGGERED_COMP"]

    env = selected["capability_envelope"]
    prop = selected["bounded_r2_proposal"]
    cost = selected["reference_cost"]
    measured = {
        "envelope_f3": env["exact_unique_f3"],
        "envelope_f5": env["exact_unique_f5"],
        "envelope_f6": env["exact_unique_f6"],
        "proposal_f3": prop["exact_unique_f3"],
        "proposal_f5": prop["exact_unique_f5"],
        "proposal_f5_observations": prop["exact_f5_observations"],
        "proposal_f6": prop["exact_unique_f6"],
        "proposal_f6_observations": prop["exact_f6_observations"],
        "payload_bytes": cost["total_candidate_payload_bytes"],
        "complexity_points": cost["capability_complexity_points"],
    }
    if measured != EXPECTED_SELECTED:
        raise DecisionError(f"selected bundle drift: {measured}")

    selected_compact = find_compact(sweep, SELECTED_SWEEP_ID)
    if not spec_matches(selected_compact, SELECTED_SPEC):
        raise DecisionError("selected sweep spec drift")

    raw = find_top(sweep, RAW_COUNT_WINNER_SWEEP_ID)
    if raw["bounded_r2_proposal"]["exact_unique_f6"] != 28:
        raise DecisionError("raw count winner F6 drift")
    if h6.RHYTHM_GT4 not in raw["capabilities"]:
        raise DecisionError("raw count winner no longer crosses the >4-bar boundary")
    if raw["reference_cost"]["total_candidate_payload_bytes"] != 148:
        raise DecisionError("raw count winner payload drift")
    if raw["reference_cost"]["capability_complexity_points"] != 15:
        raise DecisionError("raw count winner complexity drift")

    alternate = find_top(sweep, TIED_ASYMMETRIC_ALTERNATE_SWEEP_ID)
    if alternate["bounded_r2_proposal"]["exact_unique_f6"] != 25:
        raise DecisionError("asymmetric alternate F6 drift")
    if alternate["reference_cost"]["total_candidate_payload_bytes"] != 148:
        raise DecisionError("asymmetric alternate payload drift")

    harmonic_ids = [row["candidate_id"] for row in prop["harmonic_candidates"]]
    if len(harmonic_ids) != 8 or len(set(harmonic_ids)) != 8:
        raise DecisionError("harmonic candidate IDs are not eight unique IDs")

    return {
        "stage": "H6_HUMAN_REVIEWED_R2_DECISION",
        "decision": "RECOMMEND_R2_PHASE1",
        "selected_sweep_id": SELECTED_SWEEP_ID,
        "selected_bundle": selected,
        "selection_rationale": {
            "primary": "Stay inside current generic <=4-bar and <=8-harmonic-event boundaries while recovering a macro-diverse first exact F3/F5/F6 batch.",
            "raw_count_winner": {
                "sweep_id": RAW_COUNT_WINNER_SWEEP_ID,
                "proposal_f6": raw["bounded_r2_proposal"]["exact_unique_f6"],
                "complexity_points": raw["reference_cost"]["capability_complexity_points"],
                "payload_bytes": raw["reference_cost"]["total_candidate_payload_bytes"],
                "additional_capability": h6.RHYTHM_GT4,
                "incremental_f6_over_selected": raw["bounded_r2_proposal"]["exact_unique_f6"] - prop["exact_unique_f6"],
                "decision": "HOLD_PHASE2",
                "reason": "Crosses the project-wide generic 4-bar container for only three additional exact F6 identities in this bounded batch; compiled RAM/flash cost is still unmeasured.",
            },
            "asymmetric_pair_choice": {
                "selected": "ASYMMETRIC_6_10",
                "alternate": "ASYMMETRIC_7_9",
                "alternate_sweep_id": TIED_ASYMMETRIC_ALTERNATE_SWEEP_ID,
                "coverage_tie": True,
                "payload_tie": True,
                "distance_from_even_8_8_steps": {"ASYMMETRIC_6_10": 4, "ASYMMETRIC_7_9": 2},
                "reason": "With equal measured coverage/cost, 6/10 is retained as the more timing-distinct phase1 representative; this is an editorial diversity choice, not corpus popularity evidence.",
            },
        },
        "admission_boundary": {
            "evidence_level": "R2_CURATED_RUNTIME_CANDIDATE_PROPOSAL",
            "production_runtime_admission": False,
            "production_code_changes": 0,
            "compiled_flash_measurement": "REQUIRED_IN_SEPARATE_PRODUCTION_PR",
            "runtime_ram_measurement": "REQUIRED_IN_SEPARATE_PRODUCTION_PR",
            "source_incidence_to_runtime_weight": "FORBIDDEN",
            "style_materialization_to_runtime_weight": "FORBIDDEN",
        },
        "production_workstreams": [
            "P1_QUALITY_RENDERING_AND_TRIAD_POLARITY",
            "P2_MULTI_BAR_CHORD_RHYTHM_UP_TO_4_BARS",
            "P3_SAME_CHORD_RETRIGGER_WITHOUT_HARMONIC_ADVANCE",
            "P4_BOUNDED_8_HARMONIC_PLUS_4_RHYTHM_R2_VOCABULARY",
        ],
        "deferred_phase2": [
            h6.RHYTHM_GT4,
            h6.ONSETS_GT8,
            h6.ALTERED_REACH,
            h6.EXTRA_QUALITY,
            h6.HARMONIC_GT8,
            "ASYMMETRIC_7_9_SECOND_VARIANT",
        ],
    }


def markdown(report: dict[str, Any]) -> str:
    selected = report["selected_bundle"]
    prop = selected["bounded_r2_proposal"]
    env = selected["capability_envelope"]
    cost = selected["reference_cost"]
    raw = report["selection_rationale"]["raw_count_winner"]
    lines = [
        "# Harmony Atlas H6 — Human-reviewed R2 Phase 1 Decision",
        "",
        f"**Decision:** `{report['decision']}`  ",
        f"**Selected sweep:** `{report['selected_sweep_id']}`  ",
        "**Production impact:** none in H6",
        "",
        "## Selected bounded bundle",
        "",
        "```text",
        *selected["capabilities"],
        "```",
        "",
        f"Harmonic templates: **{selected['harmonic_template_budget']}**  ",
        f"Rhythm grammars: **{len(selected['rhythm_style_candidates'])}**  ",
        f"Macro families: **{', '.join(selected['rhythm_macro_families'])}**",
        "",
        "| Coverage | F3 | F5 | F6 |",
        "|---|---:|---:|---:|",
        f"| Capability envelope | {env['exact_unique_f3']} | {env['exact_unique_f5']} | {env['exact_unique_f6']} |",
        f"| Bounded R2 proposal | {prop['exact_unique_f3']} | {prop['exact_unique_f5']} | {prop['exact_unique_f6']} |",
        "",
        f"Exact F5 observations in proposal: **{prop['exact_f5_observations']}**.  ",
        f"Reference candidate payload: **{cost['total_candidate_payload_bytes']} B** (harmonic {cost['harmonic_candidate_payload_bytes']} B + rhythm {cost['rhythm_candidate_payload_bytes']} B).  ",
        f"Capability complexity: **{cost['capability_complexity_points']} points**.",
        "",
        "## Why not the raw count winner",
        "",
        f"`{raw['sweep_id']}` reaches {raw['proposal_f6']} F6 but adds `{raw['additional_capability']}`. It gains only **{raw['incremental_f6_over_selected']}** exact F6 over the selected bundle while crossing the generic 4-bar architecture boundary. It is held for phase 2 until production cost is measured.",
        "",
        "## Harmonic R2 candidates",
        "",
    ]
    for row in prop["harmonic_candidates"]:
        lines.append(
            f"- `{row['candidate_id']}` — {row['organization_class']}; family={row['source_family']}; "
            f"events={row['harmonic_event_count']}; encoding={row['encoding_strategy']}; provenance={','.join(row['source_ids'])}"
        )
    lines += ["", "## Rhythm R2 candidates", ""]
    for row in prop["rhythm_candidates"]:
        lines.append(f"- `{row['candidate_id']}` — `{row['candidate_name']}`; provenance-style={row['source_style_provenance']}")
    lines += [
        "",
        "`ASYMMETRIC_7_9` is not rejected musically; it is deferred because it ties 6/10 on measured phase1 coverage/cost but is closer to an even 8/8 split. The choice is diversity curation, not popularity weighting.",
        "",
        "## Production boundary",
        "",
        "H6 admits no production code. Actual compiled flash, linker map, DRAM, runtime CPU and hardware musical acceptance remain mandatory in separate production PRs.",
    ]
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Freeze the human-reviewed Harmony Atlas H6 R2 phase1 decision")
    p.add_argument("--normalization", type=Path, required=True)
    p.add_argument("--functional", type=Path, required=True)
    p.add_argument("--rhythm", type=Path, required=True)
    p.add_argument("--representability", type=Path, required=True)
    p.add_argument("--sweep", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--markdown-output", type=Path)
    return p.parse_args()


def main() -> int:
    args = parse_args()
    h1, _ = h6.load_verified_json(args.normalization, h6.EXPECTED_H1_SHA256, "H1")
    h3, _ = h6.load_verified_json(args.functional, h6.EXPECTED_H3_SHA256, "H3")
    h4, _ = h6.load_verified_json(args.rhythm, h6.EXPECTED_H4_SHA256, "H4")
    h5_report, _ = h6.load_verified_json(args.representability, h6.EXPECTED_H5_SHA256, "H5")
    sweep = load_json(args.sweep)
    report = build_decision(h1, h3, h4, h5_report, sweep)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if args.markdown_output:
        args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_output.write_text(markdown(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
