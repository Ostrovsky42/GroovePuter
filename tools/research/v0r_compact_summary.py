#!/usr/bin/env python3
"""Create deterministic committed V0R compact CSV/JSON snapshots."""

import argparse
import csv
import hashlib
import json
from pathlib import Path

OPS = ["KEEP", "ADD", "DROP", "DISPLACE", "ACCENT", "GHOST"]
LEGAL_OPS = ["ADD", "GHOST", "DROP", "DISPLACE", "ACCENT"]

CSV_FIELDS = [
    "archetype",
    "level",
    "nodes",
    "alternatives",
    "canonical_outdegree",
    "mean_outdegree",
    "median_outdegree",
    "min_outdegree",
    "max_outdegree",
    "dead_end_count",
    "dead_end_rate",
    "weak_component_count",
    "largest_weak_component",
    "SCC_count",
    "largest_SCC",
    "canonical_reachable_count",
    "canonical_reachable_rate",
    "reverse_reachable_to_canonical_count",
    "reverse_reachable_to_canonical_rate",
    "max_depth",
    "zero_proposal_nodes",
    "proposals_but_zero_legal_alternatives",
    "combined_no_alternative_count",
    "combined_no_alternative_rate",
    "raw_KEEP_proposals",
    "raw_ADD_proposals",
    "raw_DROP_proposals",
    "raw_DISPLACE_proposals",
    "raw_ACCENT_proposals",
    "raw_GHOST_proposals",
    "raw_proposals",
    "materializable_proposals",
    "materialization_failure",
    "structural_rejection",
    "canonical_budget_rejection",
    "duplicate_target",
    "legal_transition_count",
    "legal_ADD_edges",
    "legal_GHOST_edges",
    "legal_DROP_edges",
    "legal_DISPLACE_edges",
    "legal_ACCENT_edges",
    "identity_preserving_legal_nodes",
    "identity_violating_nodes",
    "identity_preservation_rate",
    "classification",
    "canonical_node_digest",
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fmt(value) -> str:
    return f"{float(value):.6f}"


def compact_row(graph: dict) -> dict:
    raw = graph["raw_proposal_counts_by_operation"]
    legal = graph["legal_transition_counts_by_operation"]
    rejection = graph["proposal_rejection_accounting"]
    return {
        "archetype": graph["report_family"],
        "level": graph["level"],
        "nodes": graph["node_count_including_canonical"],
        "alternatives": graph["alternative_count"],
        "canonical_outdegree": graph["canonical_outdegree"],
        "mean_outdegree": fmt(graph["outdegree_mean"]),
        "median_outdegree": fmt(graph["outdegree_median"]),
        "min_outdegree": graph["outdegree_min"],
        "max_outdegree": graph["outdegree_max"],
        "dead_end_count": graph["dead_end_count"],
        "dead_end_rate": fmt(graph["dead_end_rate"]),
        "weak_component_count": graph["weak_component_count"],
        "largest_weak_component": graph["largest_weak_component"],
        "SCC_count": graph["scc_count"],
        "largest_SCC": graph["largest_scc"],
        "canonical_reachable_count": graph["canonical_reachable_count"],
        "canonical_reachable_rate": fmt(graph["canonical_reachable_rate"]),
        "reverse_reachable_to_canonical_count":
            graph["reverse_reachable_to_canonical_count"],
        "reverse_reachable_to_canonical_rate":
            fmt(graph["reverse_reachable_to_canonical_rate"]),
        "max_depth": graph["maximum_shortest_path_depth"],
        "zero_proposal_nodes": graph["zero_proposal_nodes"],
        "proposals_but_zero_legal_alternatives":
            graph["proposals_but_zero_legal_alternatives"],
        "combined_no_alternative_count": graph["combined_no_alternative_count"],
        "combined_no_alternative_rate": fmt(graph["combined_no_alternative_rate"]),
        **{f"raw_{op}_proposals": raw[op] for op in OPS},
        "raw_proposals": rejection["raw_proposals"],
        "materializable_proposals": rejection["materializable_proposals"],
        "materialization_failure": rejection["materialization_failure"],
        "structural_rejection": rejection["structural_rejection"],
        "canonical_budget_rejection": rejection["canonical_budget_rejection"],
        "duplicate_target": rejection["duplicate_target"],
        "legal_transition_count": graph["legal_transition_count"],
        **{f"legal_{op}_edges": legal[op] for op in LEGAL_OPS},
        "identity_preserving_legal_nodes": graph["identity_preserving_legal_nodes"],
        "identity_violating_nodes": graph["identity_violating_nodes"],
        "identity_preservation_rate": fmt(graph["identity_preservation_rate"]),
        "classification": graph["classification"],
        "canonical_node_digest": graph["canonical_node_digest"],
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", required=True, type=Path)
    parser.add_argument("--authority-csv", required=True, type=Path)
    parser.add_argument("--full-json", required=True, type=Path)
    parser.add_argument("--output-csv", required=True, type=Path)
    parser.add_argument("--output-json", required=True, type=Path)
    args = parser.parse_args()

    full = json.loads(args.full_json.read_text(encoding="utf-8"))
    graphs = full.get("graphs", [])
    if len(graphs) != 18:
        raise SystemExit(f"expected 18 graph metrics, found {len(graphs)}")

    rows = [compact_row(graph) for graph in graphs]
    args.output_csv.parent.mkdir(parents=True, exist_ok=True)
    with args.output_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    detailed = dict(full["detailed_artifacts"])
    detailed["full_metrics_json"] = {
        "bytes": args.full_json.stat().st_size,
        "sha256": sha256(args.full_json),
    }

    payload = {
        "schema": "0.9.9-V0R-compact-snapshot-v2",
        "authority": full["authority"],
        "previous_v0_numeric_baseline": full["previous_v0_numeric_baseline"],
        "graph_count": 18,
        "raw_dataset_sha256": sha256(args.raw),
        "authoritative_summary_csv_sha256": sha256(args.authority_csv),
        "detailed_artifacts": detailed,
        "columns": CSV_FIELDS,
        "graphs": [[str(row[field]) for field in CSV_FIELDS] for row in rows],
    }
    args.output_json.write_text(
        json.dumps(
            payload,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        ) + "\n",
        encoding="utf-8",
    )

    print(f"V0R_COMPACT_CSV_SHA256 {sha256(args.output_csv)}")
    print(f"V0R_COMPACT_JSON_SHA256 {sha256(args.output_json)}")


if __name__ == "__main__":
    main()
