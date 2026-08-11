#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import Counter, defaultdict
from fractions import Fraction
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import harmony_atlas_h5 as h5

SCHEMA_VERSION = "1.0.0"
STAGE = "H6_CURATED_RUNTIME_CANDIDATES"
EXPECTED_H1_SHA256 = "4783be1f77784a978f2239d8527214b3912eaf262e92430bff755679fec0734e"
EXPECTED_H3_SHA256 = "f15d127722691789f6c1d1a003028da755e06a1cba1d2c1014c1232697cc456d"
EXPECTED_H4_SHA256 = "68c51f4ca8827167f2b6ff12543ed226164c9c8aa691816caac9102058849e81"
EXPECTED_H5_SHA256 = "df6b05edafd7d0fee34714fe957637ce3c1f5e392829fcfd66bdec7746395060"
TARGET_STAGE15_COMMIT = "fc42763e7798866e61895bf1b8d62339ec59e0a7"

QUALITY_RENDER = "QUALITY_RENDERING_CONSUMPTION"
TRIAD_POLARITY = "TRIAD_POLARITY_OR_EXPLICIT_CONTEXT"
EXTRA_QUALITY = "ADDITIONAL_CHORD_QUALITY_VOCABULARY"
ALTERED_REACH = "GENERIC_ALTERED_DEGREE_REACHABILITY"
HARMONIC_GT8 = "SOURCE_HARMONIC_FORM_GT8"
MULTI_BAR = "MULTI_BAR_CHORD_RHYTHM_IDENTITY"
RETRIGGER = "SAME_CHORD_RETRIGGER_WITHOUT_HARMONIC_ADVANCE"
RHYTHM_GT4 = "CHORD_RHYTHM_BEYOND_GENERIC_4_BAR_CONTAINER"
ONSETS_GT8 = "MORE_THAN_8_HARMONIC_ONSETS"

CAPABILITY_COMPLEXITY_POINTS = {
    QUALITY_RENDER: 4,
    TRIAD_POLARITY: 2,
    EXTRA_QUALITY: 4,
    ALTERED_REACH: 2,
    HARMONIC_GT8: 3,
    MULTI_BAR: 4,
    RETRIGGER: 2,
    RHYTHM_GT4: 3,
    ONSETS_GT8: 3,
}

# These are source-neutral candidate labels. The source style name remains only
# provenance; no runtime recommendation is allowed to use incidence as weight.
RHYTHM_GRAMMARS = {
    "default": {
        "candidate_name": "HELD_PER_CHORD",
        "segment_count": 1,
        "contains_retrigger": False,
        "contains_rest": False,
    },
    "pop": {
        "candidate_name": "ASYMMETRIC_6_10",
        "segment_count": 4,
        "contains_retrigger": False,
        "contains_rest": False,
    },
    "pop2": {
        "candidate_name": "ASYMMETRIC_7_9",
        "segment_count": 4,
        "contains_retrigger": False,
        "contains_rest": False,
    },
    "hiphop2": {
        "candidate_name": "GAPPED_RETRIGGER",
        "segment_count": 6,
        "contains_retrigger": True,
        "contains_rest": True,
    },
    "soul": {
        "candidate_name": "RETRIGGERED_COMP",
        "segment_count": 5,
        "contains_retrigger": True,
        "contains_rest": False,
    },
}

BUNDLES = {
    "MINIMAL": {
        "capabilities": [QUALITY_RENDER, TRIAD_POLARITY, MULTI_BAR],
        "harmonic_template_budget": 4,
        "rhythm_styles": ["default", "pop2"],
    },
    "BALANCED": {
        "capabilities": [QUALITY_RENDER, TRIAD_POLARITY, MULTI_BAR, RETRIGGER],
        "harmonic_template_budget": 8,
        "rhythm_styles": ["default", "pop2", "hiphop2"],
    },
    "WIDE": {
        "capabilities": [
            QUALITY_RENDER,
            TRIAD_POLARITY,
            EXTRA_QUALITY,
            ALTERED_REACH,
            HARMONIC_GT8,
            MULTI_BAR,
            RETRIGGER,
            RHYTHM_GT4,
            ONSETS_GT8,
        ],
        "harmonic_template_budget": 12,
        "rhythm_styles": ["default", "pop", "pop2", "hiphop2", "soul"],
    },
}

# H6 bootstrap deliberately does not name a winner. The measured artifact is
# reviewed first; a recommendation is frozen only after that review.
RECOMMENDED_BUNDLE: str | None = None


class H6Error(RuntimeError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_verified_json(path: Path, expected: str, label: str) -> tuple[dict[str, Any], str]:
    data = path.read_bytes()
    digest = sha256_bytes(data)
    if digest != expected:
        raise H6Error(f"{label} digest mismatch: expected {expected}, got {digest}")
    try:
        value = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise H6Error(f"{label} is not valid UTF-8 JSON: {exc}") from exc
    if not isinstance(value, dict):
        raise H6Error(f"{label} must contain an object")
    return value, digest


def fraction(value: dict[str, Any]) -> Fraction:
    return Fraction(int(value["numerator"]), int(value["denominator"]))


def require_inputs(h1: dict[str, Any], h3: dict[str, Any], h4: dict[str, Any], h5_report: dict[str, Any]) -> None:
    if h1.get("stage") != "H1_CANONICAL_PARSER_NORMALIZATION":
        raise H6Error("H6 requires frozen H1 normalization")
    if h3.get("stage") != "H3_FUNCTIONAL_ANALYSIS":
        raise H6Error("H6 requires frozen H3 functional evidence")
    if h4.get("stage") != "H4_CHORD_RHYTHM_EXTRACTION":
        raise H6Error("H6 requires frozen H4 rhythm evidence")
    if h5_report.get("stage") != "H5_STAGE15_REPRESENTABILITY":
        raise H6Error("H6 requires frozen H5 representability evidence")
    if h5_report.get("target_contract", {}).get("commit") != TARGET_STAGE15_COMMIT:
        raise H6Error("H5 target commit drift")
    if h5_report.get("combined", {}).get("current_exact_f6_unique_count") != 0:
        raise H6Error("H6 baseline changed: current exact F6 is no longer zero")
    if h5_report.get("methodology", {}).get("support_unit") != "LogicalProgressionDefinition":
        raise H6Error("H5 support unit drift")
    if h5_report.get("methodology", {}).get("style_observations_are_support") is not False:
        raise H6Error("H5 style-support boundary drift")
    if h5_report.get("methodology", {}).get("key_materializations_are_support") is not False:
        raise H6Error("H5 key-support boundary drift")


def materialize_h1_events(h1: dict[str, Any]) -> dict[str, list[dict[str, Any]]]:
    tokens = {row["token_id"]: row for row in h1["token_vocabulary"]}
    result: dict[str, list[dict[str, Any]]] = {}
    for definition in h1["definitions"]:
        events: list[dict[str, Any]] = []
        for ref in definition["event_refs"]:
            if ref == "REST":
                events.append({"kind": "REST"})
            else:
                token = tokens[ref]
                events.append({
                    "kind": "CHORD",
                    "root": {
                        "degree": int(token["root"]["diatonic_degree"]),
                        "alteration": int(token["root"]["alteration_semitones"]),
                    },
                    "quality_signature": list(h5.qsig(token)),
                })
        result[definition["source_id"]] = events
    return result


def organization_class(h3_row: dict[str, Any], harmonic_event_count: int) -> str:
    features = h3_row["features"]
    if features["chromatic_root_count"]:
        return "ALTERED_COLOR"
    if h3_row["source_family"] == "Modal":
        return "MODAL_LOOP"
    closure = features["closure_class"]
    if closure == "CADENTIAL":
        return "CADENCE"
    if closure == "TURNAROUND":
        return "TURNAROUND"
    if harmonic_event_count <= 2:
        return "TWO_CHORD_LOOP"
    if harmonic_event_count == 3:
        return "THREE_CHORD_LOOP"
    if harmonic_event_count == 4:
        return "FOUR_CHORD_LOOP"
    return "EXTENDED_PHRASE"


def harmonic_groups(
    h1: dict[str, Any], h3: dict[str, Any], h5_report: dict[str, Any]
) -> list[dict[str, Any]]:
    h3_rows = {row["source_id"]: row for row in h3["definitions"]}
    h5_rows = {row["source_id"]: row for row in h5_report["harmonic"]["rows"]}
    events_by_source = materialize_h1_events(h1)
    grouped: dict[str, list[str]] = defaultdict(list)
    for source_id, row in h3_rows.items():
        grouped[row["fingerprints"]["F3"]].append(source_id)

    result: list[dict[str, Any]] = []
    for f3 in sorted(grouped):
        source_ids = sorted(grouped[f3])
        representative = source_ids[0]
        h3_row = h3_rows[representative]
        h5_row = h5_rows[representative]
        events = events_by_source[representative]
        chord_events = [event for event in events if event["kind"] == "CHORD"]
        quality_classes = h5_row["quality_classes"]
        requirements = {QUALITY_RENDER}
        if quality_classes.get("CONTEXT_DEPENDENT_TRIAD", 0):
            requirements.add(TRIAD_POLARITY)
        if quality_classes.get("UNREPRESENTABLE_QUALITY", 0):
            requirements.add(EXTRA_QUALITY)
        if h5_row["altered_degree"]:
            requirements.add(ALTERED_REACH)
        if h5_row["harmonic_event_count"] > 8:
            requirements.add(HARMONIC_GT8)

        root_matches = sorted(h5_row["current_catalog_root_only_matches"])
        strategy = "ROOT_PATH_OVERLAY" if root_matches else "NEW_GENERIC_TEMPLATE"
        # Reference payload only: one-byte header, then either quality-only overlay
        # or degree/offset/quality triples. This is not compiled firmware size.
        reference_bytes = 1 + len(chord_events) if strategy == "ROOT_PATH_OVERLAY" else 1 + 3 * len(chord_events)
        result.append({
            "F3": f3,
            "source_ids": source_ids,
            "logical_definition_support": len(source_ids),
            "support_used_as_popularity_weight": False,
            "representative_source_id": representative,
            "source_family": h3_row["source_family"],
            "organization_class": organization_class(h3_row, len(chord_events)),
            "closure_class": h3_row["features"]["closure_class"],
            "cadence_class": h3_row["features"]["cadence_class"],
            "harmonic_event_count": len(chord_events),
            "requirements": sorted(requirements),
            "encoding_strategy": strategy,
            "current_root_path_matches": root_matches,
            "reference_payload_bytes": reference_bytes,
            "events": chord_events,
        })
    return result


def eligible_harmonic(group: dict[str, Any], capabilities: set[str]) -> bool:
    return set(group["requirements"]).issubset(capabilities)


def select_harmonic_candidates(
    groups: list[dict[str, Any]], capabilities: set[str], budget: int
) -> list[dict[str, Any]]:
    pool = [row for row in groups if eligible_harmonic(row, capabilities)]
    selected: list[dict[str, Any]] = []
    seen_family: set[str] = set()
    seen_org: set[str] = set()
    seen_closure: set[str] = set()
    seen_cadence: set[str] = set()
    seen_length: set[int] = set()

    while pool and len(selected) < budget:
        ranked: list[tuple[tuple[int, int, int, int, int, int, int, str], dict[str, Any]]] = []
        for row in pool:
            key = (
                int(row["source_family"] not in seen_family),
                int(row["organization_class"] not in seen_org),
                int(row["closure_class"] not in seen_closure),
                int(row["cadence_class"] not in seen_cadence),
                int(row["harmonic_event_count"] not in seen_length),
                int(row["encoding_strategy"] == "ROOT_PATH_OVERLAY"),
                -int(row["reference_payload_bytes"]),
                # lexical tie-break only; source incidence/support is deliberately absent
                row["representative_source_id"],
            )
            ranked.append((key, row))
        ranked.sort(key=lambda item: item[0], reverse=True)
        chosen = ranked[0][1]
        selected.append(chosen)
        pool.remove(chosen)
        seen_family.add(chosen["source_family"])
        seen_org.add(chosen["organization_class"])
        seen_closure.add(chosen["closure_class"])
        seen_cadence.add(chosen["cadence_class"])
        seen_length.add(chosen["harmonic_event_count"])

    output: list[dict[str, Any]] = []
    org_counts: Counter[str] = Counter()
    for row in selected:
        org_counts[row["organization_class"]] += 1
        candidate = dict(row)
        candidate["candidate_id"] = f"R2H_{row['organization_class']}_{org_counts[row['organization_class']]:02d}"
        output.append(candidate)
    return output


def rhythm_observation_exact(
    observation: dict[str, Any], capabilities: set[str], *, style_allowed: bool
) -> bool:
    if not style_allowed:
        return False
    if MULTI_BAR not in capabilities:
        return False
    phrase = fraction(observation["phrase_length_beats"])
    if phrase > 16 and RHYTHM_GT4 not in capabilities:
        return False
    if int(observation["same_chord_retrigger_count"]) > 0 and RETRIGGER not in capabilities:
        return False
    if int(observation["note_onset_count"]) > 8 and ONSETS_GT8 not in capabilities:
        return False
    return True


def rhythm_proposal(h4: dict[str, Any], capabilities: set[str], styles: list[str]) -> dict[str, Any]:
    observations = h4["observations"]
    requested = set(styles)
    if not requested.issubset(RHYTHM_GRAMMARS):
        raise H6Error(f"unknown rhythm style candidates: {sorted(requested - set(RHYTHM_GRAMMARS))}")

    proposal_exact = [
        row for row in observations
        if rhythm_observation_exact(row, capabilities, style_allowed=row["source_style"] in requested)
    ]
    envelope_exact = [
        row for row in observations
        if rhythm_observation_exact(row, capabilities, style_allowed=True)
    ]
    proposal_f5 = {row["fingerprints"]["F5"] for row in proposal_exact}
    envelope_f5 = {row["fingerprints"]["F5"] for row in envelope_exact}

    candidates: list[dict[str, Any]] = []
    for style in styles:
        grammar = RHYTHM_GRAMMARS[style]
        style_rows = [row for row in proposal_exact if row["source_style"] == style]
        candidates.append({
            "candidate_id": f"R2R_{grammar['candidate_name']}",
            "candidate_name": grammar["candidate_name"],
            "source_style_provenance": style,
            "source_incidence_used_as_weight": False,
            "base_segment_count": grammar["segment_count"],
            "contains_retrigger": grammar["contains_retrigger"],
            "contains_rest": grammar["contains_rest"],
            "reference_payload_bytes": 1 + 3 * int(grammar["segment_count"]),
            "exact_observation_count_under_bundle": len(style_rows),
            "exact_unique_f5_under_bundle": len({row["fingerprints"]["F5"] for row in style_rows}),
        })
    return {
        "candidate_grammars": candidates,
        "proposal_exact_observation_count": len(proposal_exact),
        "proposal_exact_unique_f5_count": len(proposal_f5),
        "envelope_exact_observation_count": len(envelope_exact),
        "envelope_exact_unique_f5_count": len(envelope_f5),
        "proposal_exact_observation_ids": [row["observation_id"] for row in proposal_exact],
        "envelope_exact_observation_ids": [row["observation_id"] for row in envelope_exact],
    }


def bundle_report(
    name: str,
    spec: dict[str, Any],
    groups: list[dict[str, Any]],
    h4: dict[str, Any],
) -> dict[str, Any]:
    capabilities = set(spec["capabilities"])
    selected = select_harmonic_candidates(groups, capabilities, int(spec["harmonic_template_budget"]))
    selected_f3 = {row["F3"] for row in selected}
    eligible_f3 = {row["F3"] for row in groups if eligible_harmonic(row, capabilities)}
    rhythm = rhythm_proposal(h4, capabilities, list(spec["rhythm_styles"]))
    proposal_obs = set(rhythm["proposal_exact_observation_ids"])
    envelope_obs = set(rhythm["envelope_exact_observation_ids"])

    proposal_f6: set[str] = set()
    proposal_f6_obs = 0
    envelope_f6: set[str] = set()
    envelope_f6_obs = 0
    h4_by_source = defaultdict(list)
    for observation in h4["observations"]:
        h4_by_source[observation["source_id"]].append(observation)

    f3_by_source: dict[str, str] = {}
    for group in groups:
        for source_id in group["source_ids"]:
            f3_by_source[source_id] = group["F3"]

    for observation in h4["observations"]:
        f3 = f3_by_source[observation["source_id"]]
        oid = observation["observation_id"]
        if f3 in selected_f3 and oid in proposal_obs:
            proposal_f6_obs += 1
            proposal_f6.add(observation["fingerprints"]["F6"])
        if f3 in eligible_f3 and oid in envelope_obs:
            envelope_f6_obs += 1
            envelope_f6.add(observation["fingerprints"]["F6"])

    harmonic_bytes = sum(int(row["reference_payload_bytes"]) for row in selected)
    rhythm_bytes = sum(int(row["reference_payload_bytes"]) for row in rhythm["candidate_grammars"])
    complexity = sum(CAPABILITY_COMPLEXITY_POINTS[cap] for cap in capabilities)
    return {
        "name": name,
        "capabilities": sorted(capabilities),
        "harmonic_template_budget": int(spec["harmonic_template_budget"]),
        "rhythm_style_candidates": list(spec["rhythm_styles"]),
        "capability_envelope": {
            "exact_unique_f3": len(eligible_f3),
            "exact_unique_f5": rhythm["envelope_exact_unique_f5_count"],
            "exact_f5_observations": rhythm["envelope_exact_observation_count"],
            "exact_unique_f6": len(envelope_f6),
            "exact_f6_observations": envelope_f6_obs,
        },
        "bounded_r2_proposal": {
            "exact_unique_f3": len(selected_f3),
            "exact_unique_f5": rhythm["proposal_exact_unique_f5_count"],
            "exact_f5_observations": rhythm["proposal_exact_observation_count"],
            "exact_unique_f6": len(proposal_f6),
            "exact_f6_observations": proposal_f6_obs,
            "harmonic_candidates": selected,
            "rhythm_candidates": rhythm["candidate_grammars"],
        },
        "reference_cost": {
            "harmonic_candidate_payload_bytes": harmonic_bytes,
            "rhythm_candidate_payload_bytes": rhythm_bytes,
            "total_candidate_payload_bytes": harmonic_bytes + rhythm_bytes,
            "capability_complexity_points": complexity,
            "production_compiled_flash_bytes": "NOT_MEASURED_H6_NO_PRODUCTION",
            "production_runtime_ram_bytes": "NOT_MEASURED_H6_NO_PRODUCTION",
            "reference_payload_schema": "harmonic overlay=1+N bytes; new template=1+3N bytes; rhythm grammar=1+3*segment_count bytes",
        },
    }


def dominance(a: dict[str, Any], b: dict[str, Any]) -> bool:
    ac = a["bounded_r2_proposal"]
    bc = b["bounded_r2_proposal"]
    acost = a["reference_cost"]
    bcost = b["reference_cost"]
    coverage_not_worse = (
        ac["exact_unique_f3"] >= bc["exact_unique_f3"]
        and ac["exact_unique_f5"] >= bc["exact_unique_f5"]
        and ac["exact_unique_f6"] >= bc["exact_unique_f6"]
    )
    cost_not_worse = (
        acost["total_candidate_payload_bytes"] <= bcost["total_candidate_payload_bytes"]
        and acost["capability_complexity_points"] <= bcost["capability_complexity_points"]
    )
    strictly_better = (
        ac["exact_unique_f3"] > bc["exact_unique_f3"]
        or ac["exact_unique_f5"] > bc["exact_unique_f5"]
        or ac["exact_unique_f6"] > bc["exact_unique_f6"]
        or acost["total_candidate_payload_bytes"] < bcost["total_candidate_payload_bytes"]
        or acost["capability_complexity_points"] < bcost["capability_complexity_points"]
    )
    return coverage_not_worse and cost_not_worse and strictly_better


def build_report(
    h1: dict[str, Any], h3: dict[str, Any], h4: dict[str, Any], h5_report: dict[str, Any],
    *, h1_digest: str, h3_digest: str, h4_digest: str, h5_digest: str,
) -> dict[str, Any]:
    require_inputs(h1, h3, h4, h5_report)
    groups = harmonic_groups(h1, h3, h5_report)
    bundle_rows = [bundle_report(name, spec, groups, h4) for name, spec in BUNDLES.items()]
    pareto = []
    for row in bundle_rows:
        if not any(dominance(other, row) for other in bundle_rows if other is not row):
            pareto.append(row["name"])

    return {
        "schema_version": SCHEMA_VERSION,
        "stage": STAGE,
        "dependencies": {
            "h1_normalized_json_sha256": h1_digest,
            "h3_functional_json_sha256": h3_digest,
            "h4_chord_rhythm_json_sha256": h4_digest,
            "h5_representability_json_sha256": h5_digest,
            "stage15_target_commit": TARGET_STAGE15_COMMIT,
        },
        "methodology": {
            "support_unit": "LogicalProgressionDefinition",
            "source_incidence_to_runtime_weight": "FORBIDDEN",
            "style_observations_are_popularity": False,
            "f4_is_dedup_authority": False,
            "capability_envelope_is_runtime_admission": False,
            "bounded_proposal_is_production_code": False,
            "production_files_changed": 0,
            "runtime_admission": "R2_PROPOSAL_ONLY_NOT_PRODUCTION",
            "compiled_memory_measurement": "DEFERRED_TO_PRODUCTION_INTEGRATION_PR",
        },
        "baseline": {
            "logical_definitions": int(h5_report["harmonic"]["definition_count"]),
            "unique_f3": len(groups),
            "unique_f5": int(h4["F5"]["unique_class_count"]),
            "unique_f6": int(h4["F6"]["unique_class_count"]),
            "current_exact_f3": int(h5_report["harmonic"]["audible_f3_exact_definition_count"]),
            "current_exact_f5": int(h5_report["rhythm"]["exact_unique_f5_count"]),
            "current_exact_f6": int(h5_report["combined"]["current_exact_f6_unique_count"]),
        },
        "harmonic_candidate_pool": {
            "unique_f3_count": len(groups),
            "root_path_overlay_candidate_count": sum(row["encoding_strategy"] == "ROOT_PATH_OVERLAY" for row in groups),
            "new_generic_template_candidate_count": sum(row["encoding_strategy"] == "NEW_GENERIC_TEMPLATE" for row in groups),
            "organization_distribution": dict(sorted(Counter(row["organization_class"] for row in groups).items())),
        },
        "bundle_results": bundle_rows,
        "pareto_frontier": pareto,
        "recommendation": {
            "status": "BOOTSTRAP_UNREVIEWED" if RECOMMENDED_BUNDLE is None else "HUMAN_REVIEWED_R2_PROPOSAL",
            "bundle": RECOMMENDED_BUNDLE,
            "production_integration": "SEPARATE_PR_REQUIRED",
        },
    }


def markdown(report: dict[str, Any]) -> str:
    lines = [
        "# Harmony Atlas H6 — Curated Runtime Candidate Simulation",
        "",
        "**Status:** generated R2-proposal research evidence; no production changes  ",
        f"**Stage15 target:** `{report['dependencies']['stage15_target_commit']}`",
        "",
        "## Boundary",
        "",
        "H6 separates a capability envelope from the bounded candidate vocabulary actually proposed. Envelope coverage is not runtime admission.",
        "",
        "## Baseline",
        "",
        "| Identity | Atlas unique | Current exact |",
        "|---|---:|---:|",
        f"| F3 | {report['baseline']['unique_f3']} | {report['baseline']['current_exact_f3']} |",
        f"| F5 | {report['baseline']['unique_f5']} | {report['baseline']['current_exact_f5']} |",
        f"| F6 | {report['baseline']['unique_f6']} | {report['baseline']['current_exact_f6']} |",
        "",
        "## Bundle comparison",
        "",
        "| Bundle | Envelope F3/F5/F6 | Proposal F3/F5/F6 | Payload bytes | Complexity |",
        "|---|---|---|---:|---:|",
    ]
    for row in report["bundle_results"]:
        env = row["capability_envelope"]
        prop = row["bounded_r2_proposal"]
        cost = row["reference_cost"]
        lines.append(
            f"| `{row['name']}` | {env['exact_unique_f3']}/{env['exact_unique_f5']}/{env['exact_unique_f6']} | "
            f"{prop['exact_unique_f3']}/{prop['exact_unique_f5']}/{prop['exact_unique_f6']} | "
            f"{cost['total_candidate_payload_bytes']} | {cost['capability_complexity_points']} |"
        )
    lines += ["", "## Bounded candidate details", ""]
    for row in report["bundle_results"]:
        lines += [f"### {row['name']}", "", "Capabilities:", "", "```text"]
        lines += row["capabilities"]
        lines += ["```", "", "Harmonic candidates:", ""]
        for candidate in row["bounded_r2_proposal"]["harmonic_candidates"]:
            lines.append(
                f"- `{candidate['candidate_id']}` — {candidate['organization_class']}; "
                f"family={candidate['source_family']}; events={candidate['harmonic_event_count']}; "
                f"encoding={candidate['encoding_strategy']}; provenance={','.join(candidate['source_ids'])}"
            )
        lines += ["", "Rhythm candidates:", ""]
        for candidate in row["bounded_r2_proposal"]["rhythm_candidates"]:
            lines.append(
                f"- `{candidate['candidate_id']}` — source-neutral `{candidate['candidate_name']}`; "
                f"provenance-style={candidate['source_style_provenance']}; "
                f"F5={candidate['exact_unique_f5_under_bundle']}"
            )
        lines.append("")
    lines += [
        "## Cost boundary",
        "",
        "Reference payload byte counts are measured against the H6 reference encoding only. They are not compiled firmware flash/RAM measurements. Actual linker/map/DRAM impact is mandatory in a later production integration PR.",
        "",
        "## Recommendation",
        "",
        f"Status: **{report['recommendation']['status']}**  ",
        f"Bundle: **{report['recommendation']['bundle'] or 'not selected in bootstrap'}**",
        "",
        "H6 never converts source incidence or style materialization multiplicity into runtime probability.",
    ]
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Harmony Atlas H6 bounded candidate simulator")
    parser.add_argument("--normalization", type=Path, required=True)
    parser.add_argument("--functional", type=Path, required=True)
    parser.add_argument("--rhythm", type=Path, required=True)
    parser.add_argument("--representability", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--markdown-output", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    h1, h1_digest = load_verified_json(args.normalization, EXPECTED_H1_SHA256, "H1")
    h3, h3_digest = load_verified_json(args.functional, EXPECTED_H3_SHA256, "H3")
    h4, h4_digest = load_verified_json(args.rhythm, EXPECTED_H4_SHA256, "H4")
    h5_report, h5_digest = load_verified_json(args.representability, EXPECTED_H5_SHA256, "H5")
    report = build_report(
        h1, h3, h4, h5_report,
        h1_digest=h1_digest, h3_digest=h3_digest, h4_digest=h4_digest, h5_digest=h5_digest,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if args.markdown_output:
        args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_output.write_text(markdown(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
