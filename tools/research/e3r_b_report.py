#!/usr/bin/env python3
import argparse
import csv
import hashlib
import json
import re
from collections import Counter, deque
from pathlib import Path

OPS = ["KEEP", "ADD", "DROP", "DISPLACE", "ACCENT", "GHOST"]
ROLES = [
    "Kick", "Backbeat", "ClosedHat", "OpenHat",
    "Percussion", "BassRhythm", "ChordRhythm", "MelodicRhythm",
]
SCENARIOS = ["BASE", "DROP", "DISPLACE", "DROP_DISPLACE"]
LEVELS = ["P2", "P3"]
FAMILIES = [
    "StraightFour", "OffbeatPulse", "Breakbeat",
    "HalfTime", "Sparse", "Rolling",
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
    return {str(key): counts[key] for key in sorted(counts)}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


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
        component = reachable(start, undirected)
        seen.update(component)
        sizes.append(len(component))
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
    depths = [-1] * node_count
    depths[0] = 0
    todo = deque([0])
    while todo:
        source = todo.popleft()
        for target in sorted(adjacency[source]):
            if depths[target] == -1:
                depths[target] = depths[source] + 1
                todo.append(target)
    return depths


def parse_raw(path):
    graphs = {}
    with path.open(encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.rstrip("\n")
            if not line:
                continue
            fields = line.split("\t")
            kind = fields[0]
            if kind in ("E3RB", "E3RB_END"):
                continue
            if kind == "GRAPH":
                if len(fields) != 13:
                    raise ValueError(f"bad GRAPH columns={len(fields)}: {line}")
                gi = int(fields[1])
                graphs[gi] = {
                    "graph_index": gi,
                    "graph_id": fields[2],
                    "report_family": fields[3],
                    "enum_name": fields[4],
                    "production_name": fields[5],
                    "archetype_id": int(fields[6]),
                    "level": fields[7],
                    "scenario": fields[8],
                    "drop_cap": int(fields[9]),
                    "displace_cap": int(fields[10]),
                    "node_ceiling": int(fields[11]),
                    "transition_ceiling": int(fields[12]),
                    "nodes": {},
                    "edges": [],
                    "end": None,
                }
            elif kind == "EDGE":
                if len(fields) != 15:
                    raise ValueError(f"bad EDGE columns={len(fields)}: {line}")
                gi = int(fields[1])
                graphs[gi]["edges"].append({
                    "from": int(fields[2]),
                    "to": int(fields[3]),
                    "operation": fields[4],
                    "role": int(fields[5]),
                    "source_step": int(fields[6]),
                    "target_step": int(fields[7]),
                    "source_class": fields[8],
                    "source_kind": fields[9],
                    "distance": int(fields[10]),
                    "source_canonical_anchor": bool(int(fields[11])),
                    "source_accented": bool(int(fields[12])),
                    "canonical_diff_count": int(fields[13]),
                    "canonical_diff": fields[14],
                })
            elif kind == "NODE":
                if len(fields) != 31:
                    raise ValueError(
                        f"bad NODE columns={len(fields)} row={line[:200]}")
                gi = int(fields[1])
                node_id = int(fields[2])
                graphs[gi]["nodes"][node_id] = {
                    "id": node_id,
                    "depth": int(fields[3]),
                    "canonical_delta_layer": int(fields[4]),
                    "identity_preserving": bool(int(fields[5])),
                    "total_occupied_onsets": int(fields[6]),
                    "ghost_count": int(fields[7]),
                    "accent_count": int(fields[8]),
                    "role_occupied": [int(v) for v in fields[9:17]],
                    "raw": dict(zip(OPS, (int(v) for v in fields[17:23]))),
                    "adapter_reject": int(fields[23]),
                    "executor_reject": int(fields[24]),
                    "materializable": int(fields[25]),
                    "structural_rejection": int(fields[26]),
                    "canonical_budget_rejection": int(fields[27]),
                    "legal_transition_records": int(fields[28]),
                    "duplicate_target": int(fields[29]),
                    "key": fields[30],
                }
            elif kind == "GRAPH_END":
                if len(fields) != 4:
                    raise ValueError(f"bad GRAPH_END row: {line}")
                gi = int(fields[1])
                graphs[gi]["end"] = {
                    "node_count": int(fields[2]),
                    "transition_records": int(fields[3]),
                }
            else:
                raise ValueError(f"unknown raw record: {kind}")

    if sorted(graphs) != list(range(48)):
        raise ValueError(f"expected 48 graphs, found {sorted(graphs)}")
    expected = []
    for family in FAMILIES:
        for level in LEVELS:
            for scenario in SCENARIOS:
                expected.append((family, level, scenario))
    actual = [
        (graphs[i]["report_family"], graphs[i]["level"], graphs[i]["scenario"])
        for i in sorted(graphs)
    ]
    if actual != expected:
        raise ValueError("graph order/scenario set drift")
    return graphs


def analyze_graph(graph):
    nodes = graph["nodes"]
    end = graph["end"]
    if end is None:
        raise ValueError(f"missing GRAPH_END {graph['graph_id']}")
    if len(nodes) != end["node_count"] or sorted(nodes) != list(range(len(nodes))):
        raise ValueError(f"node accounting mismatch {graph['graph_id']}")
    if len(graph["edges"]) != end["transition_records"]:
        raise ValueError(f"edge accounting mismatch {graph['graph_id']}")

    node_count = len(nodes)
    adjacency = [set() for _ in range(node_count)]
    edge_lookup = set()
    legal_ops = Counter({op: 0 for op in OPS[1:]})
    for edge in graph["edges"]:
        if edge["operation"] in ("KEEP", "ACCENT"):
            raise ValueError(f"forbidden graph edge {edge['operation']}")
        adjacency[edge["from"]].add(edge["to"])
        edge_lookup.add((edge["from"], edge["to"], edge["operation"]))
        legal_ops[edge["operation"]] += 1

    outdegrees = [len(adjacency[node]) for node in range(node_count)]
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
        raise ValueError(f"unreachable enumerated node {graph['graph_id']}")
    for node_id, node in nodes.items():
        if node["depth"] != depths[node_id]:
            raise ValueError(f"BFS depth mismatch {graph['graph_id']} node={node_id}")

    raw_ops = Counter({op: 0 for op in OPS})
    totals = Counter()
    zero_proposals = 0
    proposals_but_zero = 0
    for node_id in range(node_count):
        node = nodes[node_id]
        raw_ops.update(node["raw"])
        raw_total = sum(node["raw"].values())
        for name in (
            "adapter_reject", "executor_reject", "materializable",
            "structural_rejection", "canonical_budget_rejection",
            "legal_transition_records", "duplicate_target",
        ):
            totals[name] += node[name]
        if raw_total == 0:
            zero_proposals += 1
        elif outdegrees[node_id] == 0:
            proposals_but_zero += 1
    if raw_ops["KEEP"] != 0 or raw_ops["ACCENT"] != 0:
        raise ValueError(f"forbidden proposals in {graph['graph_id']}")
    if totals["legal_transition_records"] != len(graph["edges"]):
        raise ValueError(f"legal-record accounting mismatch {graph['graph_id']}")

    identity_preserving = sum(
        1 for node in nodes.values() if node["identity_preserving"])
    identity_violating = node_count - identity_preserving
    total_density_values = [nodes[i]["total_occupied_onsets"] for i in range(node_count)]
    scc_nontrivial_nodes = sum(size for size in scc if size > 1)

    op_detail = {
        "DROP": {
            "source_class": Counter(),
            "source_kind": Counter(),
            "accented": 0,
            "canonical_anchor": 0,
            "density_decrease": 0,
            "density_same": 0,
            "density_increase": 0,
            "reverse_ADD_edges": 0,
            "reverse_GHOST_edges": 0,
        },
        "DISPLACE": {
            "distance": Counter(),
            "source_kind": Counter(),
            "source_class": Counter(),
            "accented": 0,
            "canonical_anchor": 0,
            "density_decrease": 0,
            "density_same": 0,
            "density_increase": 0,
            "reverse_DISPLACE_edges": 0,
        },
    }
    for edge in graph["edges"]:
        if edge["operation"] not in op_detail:
            continue
        detail = op_detail[edge["operation"]]
        detail["source_kind"][edge["source_kind"]] += 1
        detail["source_class"][edge["source_class"]] += 1
        if edge["source_accented"]:
            detail["accented"] += 1
        if edge["source_canonical_anchor"]:
            detail["canonical_anchor"] += 1
        before = nodes[edge["from"]]["total_occupied_onsets"]
        after = nodes[edge["to"]]["total_occupied_onsets"]
        if after < before:
            detail["density_decrease"] += 1
        elif after > before:
            detail["density_increase"] += 1
        else:
            detail["density_same"] += 1
        if edge["operation"] == "DROP":
            if (edge["to"], edge["from"], "ADD") in edge_lookup:
                detail["reverse_ADD_edges"] += 1
            if (edge["to"], edge["from"], "GHOST") in edge_lookup:
                detail["reverse_GHOST_edges"] += 1
        else:
            detail["distance"][str(edge["distance"])] += 1
            if (edge["to"], edge["from"], "DISPLACE") in edge_lookup:
                detail["reverse_DISPLACE_edges"] += 1

    for operation in op_detail:
        for key in ("source_class", "source_kind", "distance"):
            if key in op_detail[operation]:
                op_detail[operation][key] = {
                    name: op_detail[operation][key][name]
                    for name in sorted(op_detail[operation][key])
                }

    dead_ends = sum(1 for degree in outdegrees if degree == 0)
    metrics = {
        "graph_id": graph["graph_id"],
        "report_family": graph["report_family"],
        "archetype_enum": graph["enum_name"],
        "production_name": graph["production_name"],
        "archetype_id": graph["archetype_id"],
        "level": graph["level"],
        "scenario": graph["scenario"],
        "drop_cap": graph["drop_cap"],
        "displace_cap": graph["displace_cap"],
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
        "nodes_in_nontrivial_sccs": scc_nontrivial_nodes,
        "canonical_reachable_count": len(canonical_reachable),
        "canonical_reachable_rate": rate(len(canonical_reachable), node_count),
        "reverse_reachable_to_canonical_count": len(reverse_to_canonical),
        "reverse_reachable_to_canonical_rate": rate(len(reverse_to_canonical), node_count),
        "maximum_shortest_path_depth": max(depths),
        "zero_proposal_nodes": zero_proposals,
        "proposals_but_zero_legal_alternatives": proposals_but_zero,
        "combined_no_alternative_count": zero_proposals + proposals_but_zero,
        "combined_no_alternative_rate": rate(zero_proposals + proposals_but_zero, node_count),
        "raw_proposal_counts_by_operation": {op: raw_ops[op] for op in OPS},
        "legal_transition_counts_by_operation": {op: legal_ops[op] for op in OPS[1:]},
        "proposal_rejection_accounting": {
            "raw_proposals": sum(raw_ops.values()),
            "adapter_reject": totals["adapter_reject"],
            "executor_reject": totals["executor_reject"],
            "materializable": totals["materializable"],
            "structural_rejection": totals["structural_rejection"],
            "canonical_budget_rejection": totals["canonical_budget_rejection"],
            "legal_transition_records": totals["legal_transition_records"],
            "duplicate_target": totals["duplicate_target"],
        },
        "material_density": {
            "total_occupied_onsets_histogram": histogram(total_density_values),
            "total_occupied_min": min(total_density_values),
            "total_occupied_max": max(total_density_values),
            "total_occupied_spread": max(total_density_values) - min(total_density_values),
            "ghost_count_histogram": histogram([nodes[i]["ghost_count"] for i in range(node_count)]),
            "accent_count_histogram": histogram([nodes[i]["accent_count"] for i in range(node_count)]),
            "per_role_occupied_onsets_histogram": {
                ROLES[role]: histogram([nodes[i]["role_occupied"][role] for i in range(node_count)])
                for role in range(len(ROLES))
            },
        },
        "identity_preserving_legal_nodes": identity_preserving,
        "identity_violating_nodes": identity_violating,
        "identity_preservation_rate": rate(identity_preserving, node_count),
        "operation_detail": op_detail,
        "node_ceiling": graph["node_ceiling"],
        "transition_ceiling": graph["transition_ceiling"],
        "ceiling_reached": False,
    }
    return metrics, adjacency


def load_frozen_baseline(path):
    rows = {}
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            rows[(row["archetype"], row["level"])] = row
    return rows


def assert_base_matches_frozen(metrics, frozen):
    checks = {
        "nodes": str(metrics["node_count_including_canonical"]),
        "alternatives": str(metrics["alternative_count"]),
        "canonical_outdegree": str(metrics["canonical_outdegree"]),
        "mean_outdegree": fmt(metrics["outdegree_mean"]),
        "median_outdegree": fmt(metrics["outdegree_median"]),
        "min_outdegree": str(metrics["outdegree_min"]),
        "max_outdegree": str(metrics["outdegree_max"]),
        "dead_end_count": str(metrics["dead_end_count"]),
        "dead_end_rate": fmt(metrics["dead_end_rate"]),
        "weak_component_count": str(metrics["weak_component_count"]),
        "largest_weak_component": str(metrics["largest_weak_component"]),
        "SCC_count": str(metrics["scc_count"]),
        "largest_SCC": str(metrics["largest_scc"]),
        "reverse_reachable_to_canonical_count": str(metrics["reverse_reachable_to_canonical_count"]),
        "reverse_reachable_to_canonical_rate": fmt(metrics["reverse_reachable_to_canonical_rate"]),
        "max_depth": str(metrics["maximum_shortest_path_depth"]),
        "legal_transition_count": str(metrics["legal_transition_count"]),
        "legal_ADD_edges": str(metrics["legal_transition_counts_by_operation"]["ADD"]),
        "legal_GHOST_edges": str(metrics["legal_transition_counts_by_operation"]["GHOST"]),
        "legal_DROP_edges": str(metrics["legal_transition_counts_by_operation"]["DROP"]),
        "legal_DISPLACE_edges": str(metrics["legal_transition_counts_by_operation"]["DISPLACE"]),
        "identity_violating_nodes": str(metrics["identity_violating_nodes"]),
        "canonical_node_digest": metrics["canonical_node_digest"],
    }
    for frozen_field, actual in checks.items():
        expected = frozen[frozen_field]
        if actual != expected:
            raise ValueError(
                f"BASE drift {metrics['report_family']}/{metrics['level']} "
                f"field={frozen_field} expected={expected} actual={actual}")


def comparison_delta(candidate, baseline):
    return {
        "delta_nodes": candidate["node_count_including_canonical"] - baseline["node_count_including_canonical"],
        "delta_largest_scc": candidate["largest_scc"] - baseline["largest_scc"],
        "delta_nodes_in_nontrivial_sccs": candidate["nodes_in_nontrivial_sccs"] - baseline["nodes_in_nontrivial_sccs"],
        "delta_reverse_reachability_rate": candidate["reverse_reachable_to_canonical_rate"] - baseline["reverse_reachable_to_canonical_rate"],
        "delta_dead_end_rate": candidate["dead_end_rate"] - baseline["dead_end_rate"],
        "delta_max_depth": candidate["maximum_shortest_path_depth"] - baseline["maximum_shortest_path_depth"],
        "delta_density_spread": candidate["material_density"]["total_occupied_spread"] - baseline["material_density"]["total_occupied_spread"],
        "delta_identity_violations": candidate["identity_violating_nodes"] - baseline["identity_violating_nodes"],
    }


def role_name(index):
    return ROLES[index] if 0 <= index < len(ROLES) else f"Role{index}"


def mask_steps(hex_text):
    value = int(hex_text, 16)
    return [step for step in range(16) if value & (1 << (15 - step))]


def steps_text(hex_text):
    values = mask_steps(hex_text)
    return "-" if not values else ",".join(str(v) for v in values)


ROLE_RE = re.compile(
    r"\|r(\d+):s=([0-9a-f]{4}),q=([0-9a-f]{4}),g=([0-9a-f]{4}),"
    r"sh=([0-9a-f]{4}),he=([0-9a-f]{4}),ti=([0-9a-f]{4}),a=([0-9a-f]{4})(?=\|r|\|b|$)")


def human_plan(node_key):
    pieces = []
    for match in ROLE_RE.finditer(node_key):
        role = int(match.group(1))
        masks = match.groups()[1:]
        if all(int(value, 16) == 0 for value in masks):
            continue
        labels = ("S", "Q", "G", "SH", "HE", "TI", "A")
        rendered = " ".join(
            f"{label}[{steps_text(value)}]" for label, value in zip(labels, masks))
        pieces.append(f"{role_name(role)}{{{rendered}}}")
    return " | ".join(pieces) if pieces else "<empty>"


def edge_sort_key(graph, edge):
    return (
        FAMILIES.index(graph["report_family"]),
        LEVELS.index(graph["level"]),
        edge["source_class"], edge["source_kind"], edge["distance"],
        int(not edge["source_accented"]), edge["role"],
        edge["source_step"], edge["target_step"], edge["from"], edge["to"],
    )


def select_diverse(candidates, limit, minimum, dimensions):
    selected = []
    seen = {dimension: set() for dimension in dimensions}
    remaining = sorted(candidates, key=lambda item: edge_sort_key(item[0], item[1]))
    while remaining and len(selected) < limit:
        best_index = 0
        best_score = -1
        for index, (graph, edge) in enumerate(remaining):
            values = {
                "family": graph["report_family"],
                "level": graph["level"],
                "source_class": edge["source_class"],
                "source_kind": edge["source_kind"],
                "distance": edge["distance"],
                "accented": edge["source_accented"],
            }
            score = sum(values[name] not in seen[name] for name in dimensions)
            if score > best_score:
                best_score = score
                best_index = index
        graph, edge = remaining.pop(best_index)
        selected.append((graph, edge))
        values = {
            "family": graph["report_family"],
            "level": graph["level"],
            "source_class": edge["source_class"],
            "source_kind": edge["source_kind"],
            "distance": edge["distance"],
            "accented": edge["source_accented"],
        }
        for name in dimensions:
            seen[name].add(values[name])
    if len(selected) < minimum and candidates:
        raise ValueError(f"review corpus too small: required {minimum}, got {len(selected)}")
    return selected


def canonical_diff_ops(text):
    return {item.split(":", 1)[0] for item in text.split(";") if item}


def build_review_corpus(graphs):
    drop_candidates = []
    displace_candidates = []
    combined_candidates = []
    combined_fallback = []
    for gi in sorted(graphs):
        graph = graphs[gi]
        for edge in graph["edges"]:
            if graph["scenario"] == "DROP" and edge["operation"] == "DROP":
                drop_candidates.append((graph, edge))
            if graph["scenario"] == "DISPLACE" and edge["operation"] == "DISPLACE":
                displace_candidates.append((graph, edge))
            if graph["scenario"] == "DROP_DISPLACE":
                ops = canonical_diff_ops(edge["canonical_diff"])
                if "DROP" in ops and "DISPLACE" in ops:
                    combined_candidates.append((graph, edge))
                elif edge["canonical_diff_count"] >= 2 and edge["operation"] in ("DROP", "DISPLACE"):
                    combined_fallback.append((graph, edge))

    drop_selected = select_diverse(
        drop_candidates, 12, 6,
        ("family", "level", "source_class", "source_kind", "accented"))
    displace_selected = select_diverse(
        displace_candidates, 12, 6,
        ("family", "level", "source_kind", "distance", "accented"))
    combined_pool = combined_candidates if len(combined_candidates) >= 4 else combined_candidates + combined_fallback
    combined_selected = select_diverse(
        combined_pool, 8, 4,
        ("family", "level", "source_class", "source_kind", "distance"))

    cases = []
    for category, selected in (
        ("DROP", drop_selected),
        ("DISPLACE", displace_selected),
        ("COMBINED", combined_selected),
    ):
        for index, (graph, edge) in enumerate(selected, 1):
            before = graph["nodes"][edge["from"]]
            after = graph["nodes"][edge["to"]]
            canonical = graph["nodes"][0]
            cases.append({
                "case_id": f"{category}-{index:02d}",
                "category": category,
                "archetype": graph["report_family"],
                "level": graph["level"],
                "scenario": graph["scenario"],
                "operation": edge["operation"],
                "role": role_name(edge["role"]),
                "role_index": edge["role"],
                "source_step": edge["source_step"],
                "target_step": edge["target_step"],
                "source_class": edge["source_class"],
                "source_kind": edge["source_kind"],
                "distance": edge["distance"],
                "source_accented": int(edge["source_accented"]),
                "source_canonical_anchor": int(edge["source_canonical_anchor"]),
                "canonical_relative_diff": edge["canonical_diff"],
                "canonical_relative_diff_count": edge["canonical_diff_count"],
                "density_before": before["total_occupied_onsets"],
                "density_after": after["total_occupied_onsets"],
                "canonical_C": human_plan(canonical["key"]),
                "before_V": human_plan(before["key"]),
                "candidate_W": human_plan(after["key"]),
            })
    return cases


def write_summary_csv(summaries, path):
    fields = [
        "archetype", "level", "scenario", "nodes", "alternatives",
        "legal_transitions", "canonical_outdegree", "outdegree_min",
        "outdegree_max", "outdegree_mean", "outdegree_median",
        "dead_ends", "dead_end_rate", "SCC_count", "largest_SCC",
        "nodes_in_nontrivial_SCCs", "reverse_reachable_to_canonical",
        "reverse_reachability_rate", "max_depth", "weak_components",
        "ADD_edges", "DROP_edges", "DISPLACE_edges", "GHOST_edges",
        "canonical_budget_rejects", "structural_rejects", "executor_rejects",
        "adapter_rejects", "duplicate_targets", "identity_violations",
        "identity_rate", "density_min", "density_max", "density_spread",
        "DROP_canonical", "DROP_added", "DROP_ghost",
        "DROP_reverse_ADD_edges", "DROP_reverse_GHOST_edges",
        "DISPLACE_distance1", "DISPLACE_distance2",
        "DISPLACE_structural", "DISPLACE_secondary", "DISPLACE_ghost",
        "DISPLACE_reverse_edges", "DISPLACE_canonical_anchor",
        "canonical_node_digest",
    ]
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for item in summaries:
            legal = item["legal_transition_counts_by_operation"]
            rej = item["proposal_rejection_accounting"]
            drop = item["operation_detail"]["DROP"]
            disp = item["operation_detail"]["DISPLACE"]
            density = item["material_density"]
            writer.writerow({
                "archetype": item["report_family"],
                "level": item["level"],
                "scenario": item["scenario"],
                "nodes": item["node_count_including_canonical"],
                "alternatives": item["alternative_count"],
                "legal_transitions": item["legal_transition_count"],
                "canonical_outdegree": item["canonical_outdegree"],
                "outdegree_min": item["outdegree_min"],
                "outdegree_max": item["outdegree_max"],
                "outdegree_mean": fmt(item["outdegree_mean"]),
                "outdegree_median": fmt(item["outdegree_median"]),
                "dead_ends": item["dead_end_count"],
                "dead_end_rate": fmt(item["dead_end_rate"]),
                "SCC_count": item["scc_count"],
                "largest_SCC": item["largest_scc"],
                "nodes_in_nontrivial_SCCs": item["nodes_in_nontrivial_sccs"],
                "reverse_reachable_to_canonical": item["reverse_reachable_to_canonical_count"],
                "reverse_reachability_rate": fmt(item["reverse_reachable_to_canonical_rate"]),
                "max_depth": item["maximum_shortest_path_depth"],
                "weak_components": item["weak_component_count"],
                "ADD_edges": legal["ADD"],
                "DROP_edges": legal["DROP"],
                "DISPLACE_edges": legal["DISPLACE"],
                "GHOST_edges": legal["GHOST"],
                "canonical_budget_rejects": rej["canonical_budget_rejection"],
                "structural_rejects": rej["structural_rejection"],
                "executor_rejects": rej["executor_reject"],
                "adapter_rejects": rej["adapter_reject"],
                "duplicate_targets": rej["duplicate_target"],
                "identity_violations": item["identity_violating_nodes"],
                "identity_rate": fmt(item["identity_preservation_rate"]),
                "density_min": density["total_occupied_min"],
                "density_max": density["total_occupied_max"],
                "density_spread": density["total_occupied_spread"],
                "DROP_canonical": drop["source_class"].get("CANONICAL", 0),
                "DROP_added": drop["source_class"].get("ADDED", 0),
                "DROP_ghost": drop["source_kind"].get("GHOST", 0),
                "DROP_reverse_ADD_edges": drop["reverse_ADD_edges"],
                "DROP_reverse_GHOST_edges": drop["reverse_GHOST_edges"],
                "DISPLACE_distance1": disp["distance"].get("1", 0),
                "DISPLACE_distance2": disp["distance"].get("2", 0),
                "DISPLACE_structural": disp["source_kind"].get("STRUCTURAL", 0),
                "DISPLACE_secondary": disp["source_kind"].get("SECONDARY", 0),
                "DISPLACE_ghost": disp["source_kind"].get("GHOST", 0),
                "DISPLACE_reverse_edges": disp["reverse_DISPLACE_edges"],
                "DISPLACE_canonical_anchor": disp["canonical_anchor"],
                "canonical_node_digest": item["canonical_node_digest"],
            })


def write_details(graphs, out_dir):
    nodes_path = out_dir / "e3r_b_graph_nodes.csv"
    edges_path = out_dir / "e3r_b_graph_edges.csv"
    with nodes_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow([
            "graph_id", "family", "level", "scenario", "node_id", "depth",
            "canonical_delta_layer", "identity_preserving", "total_occupied",
            "ghost_count", "accent_count", *[f"{r}_occupied" for r in ROLES],
            "node_key",
        ])
        for gi in sorted(graphs):
            graph = graphs[gi]
            for node_id in sorted(graph["nodes"]):
                node = graph["nodes"][node_id]
                writer.writerow([
                    graph["graph_id"], graph["report_family"], graph["level"],
                    graph["scenario"], node_id, node["depth"],
                    node["canonical_delta_layer"], int(node["identity_preserving"]),
                    node["total_occupied_onsets"], node["ghost_count"],
                    node["accent_count"], *node["role_occupied"], node["key"],
                ])
    with edges_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow([
            "graph_id", "family", "level", "scenario", "from", "to",
            "operation", "role", "source_step", "target_step", "source_class",
            "source_kind", "distance", "source_canonical_anchor",
            "source_accented", "canonical_diff_count", "canonical_diff",
        ])
        for gi in sorted(graphs):
            graph = graphs[gi]
            for edge in graph["edges"]:
                writer.writerow([
                    graph["graph_id"], graph["report_family"], graph["level"],
                    graph["scenario"], edge["from"], edge["to"], edge["operation"],
                    role_name(edge["role"]), edge["source_step"], edge["target_step"],
                    edge["source_class"], edge["source_kind"], edge["distance"],
                    int(edge["source_canonical_anchor"]), int(edge["source_accented"]),
                    edge["canonical_diff_count"], edge["canonical_diff"],
                ])
    return nodes_path, edges_path


def write_corpus(cases, out_dir):
    csv_path = out_dir / "e3r_b_review_corpus.csv"
    fields = list(cases[0].keys()) if cases else []
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        if fields:
            writer.writeheader()
            writer.writerows(cases)

    md_path = out_dir / "e3r_b_review_corpus.md"
    lines = [
        "# E3R-B deterministic musical review corpus",
        "",
        "MUSICAL LISTENING: PENDING",
        "",
        "Each case preserves canonical C, before V, candidate W and the exact canonical-relative diff.",
        "Step numbers are logical 0..15; `S/Q/G` are structural/secondary/ghost, followed by gate/accent masks.",
        "",
    ]
    for case in cases:
        lines.extend([
            f"## {case['case_id']} — {case['archetype']} {case['level']} {case['operation']}",
            "",
            f"- scenario: `{case['scenario']}`",
            f"- role/source/target: `{case['role']} {case['source_step']} -> {case['target_step']}`",
            f"- source class/kind: `{case['source_class']} / {case['source_kind']}`",
            f"- distance/accented/canonical-anchor: `{case['distance']} / {case['source_accented']} / {case['source_canonical_anchor']}`",
            f"- canonical-relative diff: `{case['canonical_relative_diff']}`",
            f"- density: `{case['density_before']} -> {case['density_after']}`",
            f"- C: `{case['canonical_C']}`",
            f"- V: `{case['before_V']}`",
            f"- W: `{case['candidate_W']}`",
            "",
        ])
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return csv_path, md_path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--frozen-summary", required=True, type=Path)
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)

    graphs = parse_raw(args.input)
    frozen = load_frozen_baseline(args.frozen_summary)
    summaries = []
    summary_by_key = {}
    for gi in sorted(graphs):
        metrics, _ = analyze_graph(graphs[gi])
        summaries.append(metrics)
        summary_by_key[(metrics["report_family"], metrics["level"], metrics["scenario"])] = metrics
        if metrics["scenario"] == "BASE":
            assert_base_matches_frozen(
                metrics, frozen[(metrics["report_family"], metrics["level"])])

    comparisons = []
    for family in FAMILIES:
        for level in LEVELS:
            base = summary_by_key[(family, level, "BASE")]
            drop = summary_by_key[(family, level, "DROP")]
            disp = summary_by_key[(family, level, "DISPLACE")]
            both = summary_by_key[(family, level, "DROP_DISPLACE")]
            for label, candidate, baseline in (
                ("DROP_vs_BASE", drop, base),
                ("DISPLACE_vs_BASE", disp, base),
                ("BOTH_vs_BASE", both, base),
                ("BOTH_vs_DROP", both, drop),
                ("BOTH_vs_DISPLACE", both, disp),
            ):
                comparisons.append({
                    "archetype": family,
                    "level": level,
                    "comparison": label,
                    **comparison_delta(candidate, baseline),
                })

    summary_csv = args.out_dir / "e3r_b_graph_summary.csv"
    write_summary_csv(summaries, summary_csv)
    nodes_path, edges_path = write_details(graphs, args.out_dir)
    cases = build_review_corpus(graphs)
    corpus_csv, corpus_md = write_corpus(cases, args.out_dir)

    payload = {
        "schema": "0.9.9-E3R-B",
        "authority": "E2a counterfactual proposal + E3a exact executor + E2b legal(C,W)",
        "production_policy": "UNCHANGED",
        "counterfactual_cap": 1,
        "graph_count": 48,
        "base_v0r": "BYTE-IDENTICAL (P2/P3 metrics checked here; full frozen gate external)",
        "musical_listening": "PENDING",
        "graphs": summaries,
        "comparisons": comparisons,
        "review_corpus": cases,
    }
    summary_json = args.out_dir / "e3r_b_graph_summary.json"
    summary_json.write_text(
        json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")

    digest_path = args.out_dir / "e3r_b_digests.txt"
    artifacts = [summary_csv, summary_json, nodes_path, edges_path, corpus_csv, corpus_md]
    digest_path.write_text(
        "\n".join(f"{sha256(path)}  {path.name}" for path in artifacts) + "\n",
        encoding="utf-8")

    counts = Counter(case["category"] for case in cases)
    print(f"E3R_B_REPORT graphs={len(summaries)}")
    print(f"E3R_B_SUMMARY_CSV_SHA256 {sha256(summary_csv)}")
    print(f"E3R_B_SUMMARY_JSON_SHA256 {sha256(summary_json)}")
    print(f"E3R_B_CORPUS DROP={counts['DROP']} DISPLACE={counts['DISPLACE']} COMBINED={counts['COMBINED']}")
    print("E3R-B report: OK")


if __name__ == "__main__":
    main()
