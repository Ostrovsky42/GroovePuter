#!/usr/bin/env python3
import argparse
import csv
import hashlib
import json
from collections import Counter, deque
from pathlib import Path

OPS = ["KEEP", "ADD", "DROP", "DISPLACE", "ACCENT", "GHOST"]
ROLES = [
    "Kick", "Backbeat", "ClosedHat", "OpenHat",
    "Percussion", "BassRhythm", "ChordRhythm", "MelodicRhythm",
]


def rate(n, d):
    return 0.0 if d == 0 else n / d


def fmt(value):
    return f"{value:.6f}"


def median_int(values):
    if not values:
        return 0.0
    data = sorted(values)
    n = len(data)
    mid = n // 2
    if n % 2:
        return float(data[mid])
    return (data[mid - 1] + data[mid]) / 2.0


def histogram(values):
    counts = Counter(values)
    return {str(k): counts[k] for k in sorted(counts)}


def reachable(start, adjacency):
    seen = {start}
    todo = deque([start])
    while todo:
        node = todo.popleft()
        for target in sorted(adjacency[node]):
            if target not in seen:
                seen.add(target)
                todo.append(target)
    return seen


def weak_components(node_count, adjacency):
    undirected = [set() for _ in range(node_count)]
    for source in range(node_count):
        for target in adjacency[source]:
            undirected[source].add(target)
            undirected[target].add(source)
    seen = set()
    sizes = []
    for start in range(node_count):
        if start in seen:
            continue
        comp = reachable(start, undirected)
        seen.update(comp)
        sizes.append(len(comp))
    return sizes


def scc_sizes(node_count, adjacency):
    seen = set()
    finish = []
    for start in range(node_count):
        if start in seen:
            continue
        stack = [(start, 0, sorted(adjacency[start]))]
        seen.add(start)
        while stack:
            node, index, neighbors = stack[-1]
            if index < len(neighbors):
                nxt = neighbors[index]
                stack[-1] = (node, index + 1, neighbors)
                if nxt not in seen:
                    seen.add(nxt)
                    stack.append((nxt, 0, sorted(adjacency[nxt])))
            else:
                finish.append(node)
                stack.pop()

    reverse = [set() for _ in range(node_count)]
    for source in range(node_count):
        for target in adjacency[source]:
            reverse[target].add(source)

    seen.clear()
    sizes = []
    for start in reversed(finish):
        if start in seen:
            continue
        size = 0
        stack = [start]
        seen.add(start)
        while stack:
            node = stack.pop()
            size += 1
            for nxt in sorted(reverse[node], reverse=True):
                if nxt not in seen:
                    seen.add(nxt)
                    stack.append(nxt)
        sizes.append(size)
    return sizes


def shortest_depths(node_count, adjacency):
    depth = [-1] * node_count
    depth[0] = 0
    todo = deque([0])
    while todo:
        source = todo.popleft()
        for target in sorted(adjacency[source]):
            if depth[target] == -1:
                depth[target] = depth[source] + 1
                todo.append(target)
    return depth


def classification(metrics):
    if metrics["alternative_count"] == 0:
        return "NO ALTERNATIVES"
    if metrics["identity_violating_nodes"] != 0:
        return "IDENTITY VIOLATION"
    if metrics["weak_component_count"] != 1:
        return "FRAGMENTED"
    legal = {
        op for op, count in metrics["legal_transition_counts_by_operation"].items()
        if count
    }
    if legal == {"GHOST"}:
        return "GHOST-ONLY NEIGHBORHOOD"
    if legal == {"ADD"}:
        return "ADD-ONLY NEIGHBORHOOD"
    if legal and legal <= {"ADD", "GHOST"}:
        if (metrics["largest_scc"] == 1 and
                metrics["reverse_reachable_to_canonical_count"] == 1):
            return "ONE-WAY ADD/GHOST NEIGHBORHOOD"
        return "ADD/GHOST NEIGHBORHOOD"
    return "MIXED OPERATION NEIGHBORHOOD"


def parse_raw(path):
    graphs = {}
    with path.open(encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.rstrip("\n")
            if not line:
                continue
            fields = line.split("\t")
            kind = fields[0]
            if kind == "V0R" or kind == "V0R_END":
                continue
            if kind == "GRAPH":
                if len(fields) != 10:
                    raise ValueError(f"bad GRAPH row: {line}")
                gi = int(fields[1])
                graphs[gi] = {
                    "graph_index": gi,
                    "graph_id": fields[2],
                    "report_family": fields[3],
                    "enum_name": fields[4],
                    "production_name": fields[5],
                    "archetype_id": int(fields[6]),
                    "level": fields[7],
                    "node_ceiling": int(fields[8]),
                    "transition_ceiling": int(fields[9]),
                    "nodes": {},
                    "edges": [],
                    "end": None,
                }
            elif kind == "EDGE":
                if len(fields) != 7:
                    raise ValueError(f"bad EDGE row: {line}")
                gi = int(fields[1])
                graphs[gi]["edges"].append({
                    "from": int(fields[2]),
                    "to": int(fields[3]),
                    "operation": fields[4],
                    "source_step": int(fields[5]),
                    "target_step": int(fields[6]),
                })
            elif kind == "NODE":
                if len(fields) != 30:
                    raise ValueError(
                        f"bad NODE columns={len(fields)} row: {line[:180]}")
                gi = int(fields[1])
                node_id = int(fields[2])
                graph = graphs[gi]
                graph["nodes"][node_id] = {
                    "id": node_id,
                    "depth": int(fields[3]),
                    "canonical_delta_layer": int(fields[4]),
                    "identity_preserving": bool(int(fields[5])),
                    "total_occupied_onsets": int(fields[6]),
                    "ghost_count": int(fields[7]),
                    "accent_count": int(fields[8]),
                    "role_occupied": [int(v) for v in fields[9:17]],
                    "raw": dict(zip(OPS, (int(v) for v in fields[17:23]))),
                    "materialization_failure": int(fields[23]),
                    "materializable": int(fields[24]),
                    "structural_rejection": int(fields[25]),
                    "canonical_budget_rejection": int(fields[26]),
                    "legal_transition_records": int(fields[27]),
                    "duplicate_target": int(fields[28]),
                    "key": fields[29],
                }
            elif kind == "GRAPH_END":
                if len(fields) != 5:
                    raise ValueError(f"bad GRAPH_END row: {line}")
                gi = int(fields[1])
                graphs[gi]["end"] = {
                    "node_count": int(fields[2]),
                    "transition_records": int(fields[3]),
                    "unsupported_operation": bool(int(fields[4])),
                }
            else:
                raise ValueError(f"unknown raw record: {kind}")
    if sorted(graphs) != list(range(18)):
        raise ValueError(f"expected 18 graphs, found {sorted(graphs)}")
    return graphs


def analyze_graph(graph):
    nodes = graph["nodes"]
    end = graph["end"]
    if end is None or end["unsupported_operation"]:
        raise ValueError(f"incomplete/unsupported graph {graph['graph_id']}")
    if len(nodes) != end["node_count"] or sorted(nodes) != list(range(len(nodes))):
        raise ValueError(f"node accounting mismatch {graph['graph_id']}")

    node_count = len(nodes)
    adjacency = [set() for _ in range(node_count)]
    legal_ops = Counter({op: 0 for op in OPS[1:]})
    for edge in graph["edges"]:
        if edge["operation"] == "KEEP":
            raise ValueError("KEEP graph edge")
        adjacency[edge["from"]].add(edge["to"])
        legal_ops[edge["operation"]] += 1
    if len(graph["edges"]) != end["transition_records"]:
        raise ValueError(f"edge accounting mismatch {graph['graph_id']}")

    outdegrees = [len(adjacency[n]) for n in range(node_count)]
    canonical_reachable = reachable(0, adjacency)
    reverse = [set() for _ in range(node_count)]
    for source in range(node_count):
        for target in adjacency[source]:
            reverse[target].add(source)
    reverse_to_canonical = reachable(0, reverse)
    weak = weak_components(node_count, adjacency)
    scc = scc_sizes(node_count, adjacency)
    depths = shortest_depths(node_count, adjacency)
    if any(depth < 0 for depth in depths):
        raise ValueError(f"closure produced unreachable node {graph['graph_id']}")
    for node_id, node in nodes.items():
        if node["depth"] != depths[node_id]:
            raise ValueError(
                f"stored BFS depth mismatch {graph['graph_id']} node={node_id}")

    raw_ops = Counter({op: 0 for op in OPS})
    materialization_failure = 0
    materializable = 0
    structural_rejection = 0
    budget_rejection = 0
    legal_records = 0
    duplicate_target = 0
    zero_proposals = 0
    proposals_but_zero = 0
    for node_id in range(node_count):
        node = nodes[node_id]
        raw_total = sum(node["raw"].values())
        raw_ops.update(node["raw"])
        materialization_failure += node["materialization_failure"]
        materializable += node["materializable"]
        structural_rejection += node["structural_rejection"]
        budget_rejection += node["canonical_budget_rejection"]
        legal_records += node["legal_transition_records"]
        duplicate_target += node["duplicate_target"]
        if raw_total == 0:
            zero_proposals += 1
        elif outdegrees[node_id] == 0:
            proposals_but_zero += 1

    if legal_records != len(graph["edges"]):
        raise ValueError(f"legal record mismatch {graph['graph_id']}")
    if raw_ops["KEEP"] != 0:
        raise ValueError(f"KEEP proposal found {graph['graph_id']}")

    identity_preserving = sum(
        1 for node in nodes.values() if node["identity_preserving"])
    identity_violating = node_count - identity_preserving

    total_hist = histogram(
        [nodes[i]["total_occupied_onsets"] for i in range(node_count)])
    ghost_hist = histogram(
        [nodes[i]["ghost_count"] for i in range(node_count)])
    accent_hist = histogram(
        [nodes[i]["accent_count"] for i in range(node_count)])
    role_hist = {
        ROLES[role]: histogram(
            [nodes[i]["role_occupied"][role] for i in range(node_count)])
        for role in range(len(ROLES))
    }
    layer_hist = histogram(
        [nodes[i]["canonical_delta_layer"] for i in range(node_count)])

    dead_ends = sum(1 for degree in outdegrees if degree == 0)
    no_alt = zero_proposals + proposals_but_zero
    metrics = {
        "graph_id": graph["graph_id"],
        "report_family": graph["report_family"],
        "archetype_enum": graph["enum_name"],
        "production_name": graph["production_name"],
        "archetype_id": graph["archetype_id"],
        "level": graph["level"],
        "canonical_node_digest": hashlib.sha256(
            nodes[0]["key"].encode("utf-8")).hexdigest(),
        "node_count_including_canonical": node_count,
        "alternative_count": node_count - 1,
        "legal_transition_count": len(graph["edges"]),
        "canonical_outdegree": outdegrees[0],
        "outdegree_min": min(outdegrees),
        "outdegree_max": max(outdegrees),
        "outdegree_mean": sum(outdegrees) / node_count,
        "outdegree_median": median_int(outdegrees),
        "dead_end_count": dead_ends,
        "dead_end_rate": rate(dead_ends, node_count),
        "weak_component_count": len(weak),
        "largest_weak_component": max(weak),
        "scc_count": len(scc),
        "largest_scc": max(scc),
        "canonical_reachable_count": len(canonical_reachable),
        "canonical_reachable_rate": rate(len(canonical_reachable), node_count),
        "reverse_reachable_to_canonical_count": len(reverse_to_canonical),
        "reverse_reachable_to_canonical_rate": rate(
            len(reverse_to_canonical), node_count),
        "maximum_shortest_path_depth": max(depths),
        "canonical_delta_layer_histogram": layer_hist,
        "zero_proposal_nodes": zero_proposals,
        "proposals_but_zero_legal_alternatives": proposals_but_zero,
        "combined_no_alternative_count": no_alt,
        "combined_no_alternative_rate": rate(no_alt, node_count),
        "raw_proposal_counts_by_operation": {
            op: raw_ops[op] for op in OPS
        },
        "legal_transition_counts_by_operation": {
            op: legal_ops[op] for op in OPS[1:]
        },
        "proposal_rejection_accounting": {
            "raw_proposals": sum(raw_ops.values()),
            "materializable_proposals": materializable,
            "materialization_failure": materialization_failure,
            "structural_rejection": structural_rejection,
            "canonical_budget_rejection": budget_rejection,
            "legal_transition_records": legal_records,
            "duplicate_target": duplicate_target,
        },
        "material_density": {
            "total_occupied_onsets_histogram": total_hist,
            "per_role_occupied_onsets_histogram": role_hist,
            "ghost_count_histogram": ghost_hist,
            "accent_count_histogram": accent_hist,
        },
        "identity_preserving_legal_nodes": identity_preserving,
        "identity_violating_nodes": identity_violating,
        "identity_preservation_rate": rate(identity_preserving, node_count),
        "node_ceiling": graph["node_ceiling"],
        "transition_ceiling": graph["transition_ceiling"],
        "ceiling_reached": False,
    }
    metrics["classification"] = classification(metrics)
    return metrics, adjacency


def write_details(graphs, out_dir):
    nodes_path = out_dir / "v0r_e2_variant_graph_nodes.csv"
    edges_path = out_dir / "v0r_e2_variant_graph_edges.csv"
    with nodes_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow([
            "graph_id", "node_id", "depth", "canonical_delta_layer",
            "identity_preserving", "total_occupied_onsets", "ghost_count",
            "accent_count", *[f"{r}_occupied" for r in ROLES], "node_key",
        ])
        for gi in sorted(graphs):
            graph = graphs[gi]
            for node_id in sorted(graph["nodes"]):
                node = graph["nodes"][node_id]
                writer.writerow([
                    graph["graph_id"], node_id, node["depth"],
                    node["canonical_delta_layer"],
                    int(node["identity_preserving"]),
                    node["total_occupied_onsets"], node["ghost_count"],
                    node["accent_count"], *node["role_occupied"], node["key"],
                ])
    with edges_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow([
            "graph_id", "from_node_id", "to_node_id", "operation",
            "source_step", "target_step",
        ])
        for gi in sorted(graphs):
            graph = graphs[gi]
            for edge in graph["edges"]:
                writer.writerow([
                    graph["graph_id"], edge["from"], edge["to"],
                    edge["operation"], edge["source_step"], edge["target_step"],
                ])
    return nodes_path, edges_path


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)

    graphs = parse_raw(args.input)
    summaries = []
    for gi in sorted(graphs):
        metrics, _ = analyze_graph(graphs[gi])
        summaries.append(metrics)

    nodes_path, edges_path = write_details(graphs, args.out_dir)
    detailed = {
        "nodes_csv": {
            "file": nodes_path.name,
            "sha256": sha256(nodes_path),
            "bytes": nodes_path.stat().st_size,
        },
        "edges_csv": {
            "file": edges_path.name,
            "sha256": sha256(edges_path),
            "bytes": edges_path.stat().st_size,
        },
    }

    json_path = args.out_dir / "v0r_e2_variant_graph_summary.json"
    payload = {
        "schema": "0.9.9-V0R",
        "authority": "E2c + E2a + E2b canonical-relative legality",
        "graph_count": 18,
        "previous_v0_numeric_baseline":
            "NOT AVAILABLE — BRANCH CONTAINED NO TOOLING DELTA",
        "detailed_artifacts": detailed,
        "graphs": summaries,
    }
    json_path.write_text(
        json.dumps(payload, sort_keys=True, indent=2) + "\n",
        encoding="utf-8",
    )

    csv_path = args.out_dir / "v0r_e2_variant_graph_summary.csv"
    fields = [
        "archetype", "level", "nodes", "alternatives",
        "canonical_outdegree", "mean_outdegree", "dead_ends",
        "dead_end_rate", "SCCs", "largest_SCC", "max_depth",
        "no_alternative_rate", "ADD_edges", "GHOST_edges", "DROP_edges",
        "DISPLACE_edges", "ACCENT_edges", "identity_rate",
        "classification", "canonical_node_digest",
        "raw_proposals", "materializable_proposals",
        "materialization_failure", "structural_rejection",
        "canonical_budget_rejection", "duplicate_target",
        "weak_components", "largest_weak_component",
        "canonical_reachable_count", "canonical_reachable_rate",
        "reverse_reachable_count", "reverse_reachable_rate",
        "outdegree_min", "outdegree_max", "outdegree_median",
        "zero_proposal_nodes", "proposals_but_zero_legal_alternatives",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for item in summaries:
            rej = item["proposal_rejection_accounting"]
            legal = item["legal_transition_counts_by_operation"]
            writer.writerow({
                "archetype": item["report_family"],
                "level": item["level"],
                "nodes": item["node_count_including_canonical"],
                "alternatives": item["alternative_count"],
                "canonical_outdegree": item["canonical_outdegree"],
                "mean_outdegree": fmt(item["outdegree_mean"]),
                "dead_ends": item["dead_end_count"],
                "dead_end_rate": fmt(item["dead_end_rate"]),
                "SCCs": item["scc_count"],
                "largest_SCC": item["largest_scc"],
                "max_depth": item["maximum_shortest_path_depth"],
                "no_alternative_rate": fmt(
                    item["combined_no_alternative_rate"]),
                "ADD_edges": legal["ADD"],
                "GHOST_edges": legal["GHOST"],
                "DROP_edges": legal["DROP"],
                "DISPLACE_edges": legal["DISPLACE"],
                "ACCENT_edges": legal["ACCENT"],
                "identity_rate": fmt(item["identity_preservation_rate"]),
                "classification": item["classification"],
                "canonical_node_digest": item["canonical_node_digest"],
                "raw_proposals": rej["raw_proposals"],
                "materializable_proposals": rej["materializable_proposals"],
                "materialization_failure": rej["materialization_failure"],
                "structural_rejection": rej["structural_rejection"],
                "canonical_budget_rejection":
                    rej["canonical_budget_rejection"],
                "duplicate_target": rej["duplicate_target"],
                "weak_components": item["weak_component_count"],
                "largest_weak_component": item["largest_weak_component"],
                "canonical_reachable_count":
                    item["canonical_reachable_count"],
                "canonical_reachable_rate":
                    fmt(item["canonical_reachable_rate"]),
                "reverse_reachable_count":
                    item["reverse_reachable_to_canonical_count"],
                "reverse_reachable_rate":
                    fmt(item["reverse_reachable_to_canonical_rate"]),
                "outdegree_min": item["outdegree_min"],
                "outdegree_max": item["outdegree_max"],
                "outdegree_median": fmt(item["outdegree_median"]),
                "zero_proposal_nodes": item["zero_proposal_nodes"],
                "proposals_but_zero_legal_alternatives":
                    item["proposals_but_zero_legal_alternatives"],
            })

    digest_path = args.out_dir / "v0r_e2_variant_graph_digests.txt"
    rows = [
        f"{sha256(csv_path)}  {csv_path.name}",
        f"{sha256(json_path)}  {json_path.name}",
        f"{detailed['nodes_csv']['sha256']}  {nodes_path.name}",
        f"{detailed['edges_csv']['sha256']}  {edges_path.name}",
    ]
    digest_path.write_text("\n".join(rows) + "\n", encoding="utf-8")

    print(f"V0R_REPORT graphs={len(summaries)}")
    print(f"V0R_SUMMARY_CSV_SHA256 {sha256(csv_path)}")
    print(f"V0R_SUMMARY_JSON_SHA256 {sha256(json_path)}")
    print("V0R report: OK")


if __name__ == "__main__":
    main()
