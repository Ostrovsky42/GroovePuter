#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import math
import statistics
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

EXPECTED_ATLAS_SHA256 = "5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd"
EXPECTED_SCHEMA_VERSION = "2.6.0"
EXPECTED_PATTERN_COUNT = 413
EXPECTED_EVENT_COUNT = 9377
EXPECTED_BAR_COUNTS = {1: 302, 2: 93, 4: 17, 5: 1}
ROLE_MAPPING_SCHEMA = "ATLAS_ROLE_MAPPING_V2"
GRAMMAR_SCHEMA = "GROOVEPUTER_RUNTIME_RHYTHM_TOPOLOGY_V2"
SUPPORT_SCHEMA = "ATLAS_PASS2_SUPPORT_V2"
DISTANCE_SCHEMA = "ATLAS_PASS2_DISTANCE_V2"

ROLE_NAMES = {
    0: "Kick", 1: "Backbeat", 2: "ClosedHat", 3: "OpenHat",
    4: "Percussion", 5: "BassRhythm", 6: "ChordRhythm", 7: "MelodicRhythm",
}
DRUM_ROLES = (0, 1, 2, 3, 4)
DRUM_WEIGHTS = {0: 3.0, 1: 3.0, 2: 1.5, 3: 1.0, 4: 1.0}
PERCUSSION_IDS = {
    "PERC1", "PERC2", "PERCUSSION", "RIMSHOT", "RIM_CLICK",
    "MID_TOM", "HIGH_TOM", "LOW_TOM", "COWBELL", "SHAKER",
    "TAMBOURINE", "LOW_CONGA", "MID_CONGA", "HIGH_CONGA", "CLAVES", "MARACAS",
}
AMBIGUOUS_IDS = {"CYMBAL", "RIDE", "MID_HAT", "HAT_FOOT"}
SENSITIVE_TOKENS = (
    "pattern_id", "structural_group_id", "structural_hash", "expressive_hash",
    "source_locator", "source_artifact_id", "artifact_id", "content_hash",
    "exact_mask", "kick_mask", "backbeat_mask",
)


def rows(zf: zipfile.ZipFile, root: str, rel: str) -> list[dict[str, str]]:
    return list(csv.DictReader(io.StringIO(zf.read(root + rel).decode("utf-8-sig"))))


def mask_steps(mask: int) -> set[int]:
    return {step for step in range(16) if mask & (1 << (15 - step))}


def jaccard(a: set[int], b: set[int]) -> float:
    union = a | b
    return 0.0 if not union else 1.0 - len(a & b) / len(union)


def observation_distance(a: dict[int, set[int]], b: dict[int, set[int]]) -> float:
    total = weight_sum = 0.0
    for role in DRUM_ROLES:
        weight = DRUM_WEIGHTS[role]
        total += weight * jaccard(a[role], b[role])
        weight_sum += weight
    return total / weight_sum


def quantile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    xs = sorted(values)
    if len(xs) == 1:
        return xs[0]
    pos = (len(xs) - 1) * p
    lo = math.floor(pos)
    hi = math.ceil(pos)
    return xs[lo] if lo == hi else xs[lo] * (hi - pos) + xs[hi] * (pos - lo)


def distribution(metric: str, values: list[float]) -> dict[str, object]:
    xs = sorted(values)
    return {
        "metric": metric,
        "count": len(xs),
        "min": round(xs[0], 6) if xs else 0.0,
        "p05": round(quantile(xs, 0.05), 6),
        "p25": round(quantile(xs, 0.25), 6),
        "p50": round(quantile(xs, 0.50), 6),
        "p75": round(quantile(xs, 0.75), 6),
        "p90": round(quantile(xs, 0.90), 6),
        "p95": round(quantile(xs, 0.95), 6),
        "max": round(xs[-1], 6) if xs else 0.0,
    }


def semantic_role(track: dict[str, str], pattern: dict[str, str]) -> tuple[int | None, str]:
    """Conservative source-aware semantic role mapping.

    Exact track IDs win. Ambiguous cymbal/hat-foot names remain UNMAPPED rather
    than being guessed. Research CLAP/HAND_CLAP stays Percussion unless an
    explicit source role proves backbeat ownership.
    """
    track_id = track["track_id"].upper()
    role = track["track_role"].lower()
    if track_id == "KICK":
        return 0, "EXACT_TRACK_ID"
    if track_id == "SNARE":
        return 1, "EXACT_TRACK_ID"
    if track_id in {"CLOSED_HAT", "HAT1"}:
        return 2, "EXACT_TRACK_ID"
    if track_id in {"OPEN_HAT", "HAT2"}:
        return 3, "EXACT_TRACK_ID"
    if track_id in PERCUSSION_IDS:
        return 4, "EXACT_TRACK_ID"
    if track_id in {"CLAP", "HAND_CLAP"}:
        if pattern["pattern_kind"] != "SOURCE_OBSERVATION" and role in {"drums", "clap"}:
            return 1, "EDITORIAL_BACKBEAT_ROLE"
        if role == "clap" and pattern["source_id"] != "SRC_POCKET_OPS_31":
            return 1, "EXPLICIT_CLAP_ROLE"
        return 4, "CONSERVATIVE_RESEARCH_CLAP"
    if track_id in AMBIGUOUS_IDS:
        return None, "AMBIGUOUS_TRACK_ID"
    if role == "bass":
        return 5, "EXPLICIT_TRACK_ROLE"
    if role == "harmony":
        return 6, "EXPLICIT_TRACK_ROLE"
    if role == "melody":
        return 7, "EXPLICIT_TRACK_ROLE"
    if role in {"annotation", "sample"} or "UNKNOWN" in track["track_role"].upper():
        return None, "NON_RHYTHM_OR_UNKNOWN"
    return None, "UNMAPPED"


def load_runtime_topology(path: Path) -> dict[int, dict[str, object]]:
    runtime: dict[int, dict[str, object]] = {}
    format_id = None
    count = None
    for raw in path.read_text(encoding="utf-8").splitlines():
        if not raw:
            continue
        parts = raw.split("\t")
        kind = parts[0]
        if kind == "FORMAT":
            format_id = parts[1]
        elif kind == "A":
            archetype_id = int(parts[1])
            runtime[archetype_id] = {
                "id": archetype_id,
                "name": parts[2],
                "family": int(parts[3]),
                "active_roles": int(parts[4]),
                "density": tuple(map(int, parts[5:9])),
                "lanes": {},
                "spaces": [],
                "relationships": [],
            }
        elif kind == "L":
            archetype_id = int(parts[1])
            role = int(parts[2])
            immutable, canonical, preferred, optional, forbidden, minimum, maximum, ornament_max = map(int, parts[3:11])
            runtime[archetype_id]["lanes"][role] = {
                "required": mask_steps(immutable | canonical),
                "support": mask_steps(immutable | canonical | preferred | optional),
                "forbidden": mask_steps(forbidden),
                "min": minimum,
                "max": maximum,
                "ornament_max": ornament_max,
            }
        elif kind == "S":
            runtime[int(parts[1])]["spaces"].append({
                "steps": mask_steps(int(parts[2])),
                "roles": int(parts[3]),
            })
        elif kind == "R":
            runtime[int(parts[1])]["relationships"].append({
                "source": int(parts[2]),
                "target": int(parts[3]),
                "op": int(parts[4]),
                "strength": int(parts[5]),
                "scope": int(parts[6]),
                "zone": mask_steps(int(parts[7])),
                "min_offset": int(parts[8]),
                "max_offset": int(parts[9]),
                "min_matches": int(parts[10]),
                "max_matches": int(parts[11]),
                "min_responses": int(parts[12]),
                "max_responses": int(parts[13]),
                "weight": int(parts[14]),
            })
        elif kind == "COUNT":
            count = int(parts[1])
        else:
            raise ValueError(f"unsupported runtime topology row: {kind}")
    if format_id != GRAMMAR_SCHEMA:
        raise ValueError(f"unexpected runtime topology format: {format_id}")
    if count != 20 or len(runtime) != 20:
        raise ValueError("runtime topology incomplete")
    return runtime


def hard_relationship_ok(relationship: dict[str, object], observation: dict[int, set[int]]) -> bool:
    if relationship["strength"] != 1:
        return True
    source_role = int(relationship["source"])
    target_role = int(relationship["target"])
    if source_role not in DRUM_ROLES or target_role not in DRUM_ROLES:
        return True
    source = observation[source_role] & set(relationship["zone"])
    target = observation[target_role] & set(relationship["zone"])
    op = int(relationship["op"])
    if op == 0:  # Exclude
        return not (source & target)
    if op == 1:  # Coincide
        matches = len(source & target)
        return matches >= int(relationship["min_matches"]) and (
            not relationship["max_matches"] or matches <= int(relationship["max_matches"])
        )
    if op == 2:  # Offset
        return all(any(
            int(relationship["min_offset"]) <= target_step - source_step <= int(relationship["max_offset"])
            for source_step in source
        ) for target_step in target)
    if op == 3:  # Respond, same nearest-source ownership as runtime resolver.
        sources = sorted(source)
        response_counts = [0] * len(sources)
        for target_step in sorted(target):
            candidates = []
            for index, source_step in enumerate(sources):
                delta = target_step - source_step
                if int(relationship["min_offset"]) <= delta <= int(relationship["max_offset"]):
                    candidates.append((abs(delta), source_step, index))
            if candidates:
                response_counts[min(candidates)[2]] += 1
        for count in response_counts:
            if count < int(relationship["min_responses"]):
                return False
            if relationship["max_responses"] and count > int(relationship["max_responses"]):
                return False
        return True
    if op == 4:  # hard FillGaps is rejected by runtime catalog validation.
        return True
    return False


def grammar_coverage(observation: dict[int, set[int]], archetype: dict[str, object]) -> dict[str, object]:
    required_misses = outside_legal = density_deviation = protected_hits = relationship_violations = 0
    typicality = []
    for role in DRUM_ROLES:
        lane = archetype["lanes"].get(role)
        actual = observation[role]
        if lane is None:
            outside_legal += len(actual)
            continue
        blocked = set(lane["forbidden"])
        for space in archetype["spaces"]:
            if int(space["roles"]) & (1 << role):
                blocked |= set(space["steps"])
        legal = set(lane["support"]) - blocked
        required_misses += len(set(lane["required"]) - actual)
        outside_legal += len(actual - legal)
        protected_hits += len(actual & blocked)
        if len(actual) < int(lane["min"]):
            density_deviation += int(lane["min"]) - len(actual)
        elif len(actual) > int(lane["max"]):
            density_deviation += len(actual) - int(lane["max"])
        typicality.append(jaccard(actual, legal))
    for relationship in archetype["relationships"]:
        if not hard_relationship_ok(relationship, observation):
            relationship_violations += 1
    covered = (
        required_misses == 0 and outside_legal == 0 and density_deviation == 0
        and protected_hits == 0 and relationship_violations == 0
    )
    return {
        "hard_covered": covered,
        "required_misses": required_misses,
        "outside_legal_hits": outside_legal,
        "density_deviation": density_deviation,
        "protected_space_hits": protected_hits,
        "hard_relationship_violations": relationship_violations,
        # Lane-only lower bound: relationship repair may require additional edits.
        "minimum_lane_edit_lower_bound": required_misses + outside_legal + density_deviation,
        "typicality_support_jaccard": round(statistics.mean(typicality), 6) if typicality else 1.0,
    }


def pattern_masks_with_mapping(
    pattern_id: str, pattern_map, tracks_by_pattern, events_by_key
) -> tuple[dict[int, set[int]], int]:
    pattern = pattern_map[pattern_id]
    observation = {role: set() for role in range(8)}
    ambiguous = 0
    for track in tracks_by_pattern[pattern_id]:
        role, _reason = semantic_role(track, pattern)
        track_events = events_by_key[(pattern_id, track["track_id"])]
        if role is None:
            ambiguous += len(track_events)
            continue
        for event in track_events:
            step = int(event["step_index"]) - 1
            if 0 <= step < 16:
                observation[role].add(step)
    return observation, ambiguous


def write_csv(path: Path, output_rows: list[dict[str, object]], fieldnames: list[str]) -> None:
    lowered = {name.lower() for name in fieldnames}
    if any(token in name for name in lowered for token in SENSITIVE_TOKENS):
        raise ValueError("rights-sensitive field name in repo-safe output")
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(output_rows)


def extract_hardening(atlas_zip: Path, runtime_topology: Path, output_dir: Path) -> dict[str, object]:
    digest = hashlib.sha256(atlas_zip.read_bytes()).hexdigest()
    if digest != EXPECTED_ATLAS_SHA256:
        raise ValueError("Atlas SHA mismatch")
    with zipfile.ZipFile(atlas_zip) as zf:
        roots = {name.split("/", 1)[0] for name in zf.namelist() if "/" in name}
        if len(roots) != 1:
            raise ValueError("unexpected ZIP roots")
        root = next(iter(roots)) + "/"
        validation = json.loads(zf.read(root + "reports/validation_summary.json"))
        patterns = rows(zf, root, "core/patterns.csv")
        tracks = rows(zf, root, "core/pattern_tracks.csv")
        events = rows(zf, root, "core/pattern_events.csv")
        direction_links = rows(zf, root, "core/pattern_direction_links.csv")
        artifacts = rows(zf, root, "core/source_artifacts.csv")
    if validation.get("schema_version") != EXPECTED_SCHEMA_VERSION or validation.get("failures") != 0:
        raise ValueError("Atlas validation gate failed")
    if len(patterns) != EXPECTED_PATTERN_COUNT or len(events) != EXPECTED_EVENT_COUNT:
        raise ValueError("corpus size changed")
    bar_counts = Counter(int(pattern["bars"]) for pattern in patterns)
    if dict(bar_counts) != EXPECTED_BAR_COUNTS:
        raise ValueError(f"bar accounting changed: {dict(bar_counts)}")

    pattern_map = {pattern["pattern_id"]: pattern for pattern in patterns}
    artifact_hash = {artifact["artifact_id"]: artifact["content_hash"] for artifact in artifacts}
    tracks_by_pattern = defaultdict(list)
    events_by_key = defaultdict(list)
    directions_by_pattern = defaultdict(list)
    for track in tracks:
        tracks_by_pattern[track["pattern_id"]].append(track)
    for event in events:
        events_by_key[(event["pattern_id"], event["track_id"])].append(event)
    for link in direction_links:
        directions_by_pattern[link["pattern_id"]].append(link["direction_id"])

    mapping_counts = Counter()
    ambiguous_events = 0
    masks = {}
    eligible = []
    for pattern in patterns:
        if (
            pattern["bars"] != "1" or pattern["steps_per_bar"] != "16"
            or pattern["pattern_kind"] != "SOURCE_OBSERVATION"
            or pattern["publication_status"] == "SUPERSEDED"
        ):
            continue
        pattern_id = pattern["pattern_id"]
        observation = {role: set() for role in range(8)}
        ambiguous = 0
        for track in tracks_by_pattern[pattern_id]:
            role, reason = semantic_role(track, pattern)
            track_events = events_by_key[(pattern_id, track["track_id"])]
            mapping_counts[(ROLE_NAMES.get(role, "UNMAPPED"), reason)] += len(track_events)
            if role is None:
                ambiguous += len(track_events)
                continue
            for event in track_events:
                step = int(event["step_index"]) - 1
                if 0 <= step < 16:
                    observation[role].add(step)
        ambiguous_events += ambiguous
        masks[pattern_id] = observation
        eligible.append(pattern)

    # structural_group_id is used only for source lineage/dedupe. The musical
    # comparison is re-normalized after semantic role mapping.
    by_source_group = defaultdict(list)
    for pattern in eligible:
        by_source_group[pattern["structural_group_id"]].append(pattern["pattern_id"])
    representatives = []
    for pattern_ids in by_source_group.values():
        declared = pattern_map[pattern_ids[0]].get("structure_representative_pattern_id", "")
        representatives.append(declared if declared in pattern_ids else sorted(pattern_ids)[0])
    representatives = sorted(representatives)

    runtime = load_runtime_topology(runtime_topology)
    skeletons = defaultdict(list)
    for pattern_id in representatives:
        observation = masks[pattern_id]
        skeletons[(tuple(sorted(observation[0])), tuple(sorted(observation[1])))].append(pattern_id)
    recurring = sorted(
        (item for item in skeletons.items() if len(item[1]) >= 3),
        key=lambda item: (-len(item[1]), item[0]),
    )

    candidate_rows = []
    for index, (_skeleton, pattern_ids) in enumerate(recurring, 1):
        provenance_roots = {pattern_map[pattern_id]["source_id"] for pattern_id in pattern_ids}
        content_hashes = {
            artifact_hash.get(pattern_map[pattern_id]["source_artifact_id"], "")
            for pattern_id in pattern_ids if pattern_map[pattern_id]["source_artifact_id"]
        }
        content_hashes.discard("")
        directions = {direction for pattern_id in pattern_ids for direction in directions_by_pattern[pattern_id]}
        coverage_names = Counter()
        covered_members = 0
        nearest_edit_lower_bounds = []
        typicality = []
        candidate_ambiguous_events = 0
        for pattern_id in pattern_ids:
            per_archetype = []
            fits = []
            for archetype in runtime.values():
                fit = grammar_coverage(masks[pattern_id], archetype)
                per_archetype.append((float(fit["minimum_lane_edit_lower_bound"]), str(archetype["name"]), fit))
                if fit["hard_covered"]:
                    fits.append(str(archetype["name"]))
                    coverage_names[str(archetype["name"])] += 1
                    typicality.append(float(fit["typicality_support_jaccard"]))
            nearest_edit_lower_bounds.append(min(per_archetype, key=lambda item: (item[0], item[1]))[0])
            _unused, ambiguous_here = pattern_masks_with_mapping(
                pattern_id, pattern_map, tracks_by_pattern, events_by_key
            )
            candidate_ambiguous_events += ambiguous_here
            if fits:
                covered_members += 1
        pair_distances = [
            observation_distance(masks[left], masks[right])
            for position, left in enumerate(pattern_ids)
            for right in pattern_ids[position + 1:]
        ]
        if covered_members == len(pattern_ids):
            decision = "NEAR_EXISTING"
        elif len(provenance_roots) >= 2 and len(content_hashes) >= 2:
            decision = "AUDITION_REVIEW"
        elif covered_members:
            decision = "BOUNDARY_REVIEW"
        else:
            decision = "HOLD_SINGLE_ROOT"
        candidate_rows.append({
            "candidate_id": f"HARD_{index:02d}",
            "structural_group_count": len(pattern_ids),
            "independent_provenance_root_count": len(provenance_roots),
            "content_deduped_artifact_count": len(content_hashes),
            "direction_count": len(directions),
            "directions": "|".join(sorted(directions)),
            "hard_covered_member_count": covered_members,
            "hard_uncovered_member_count": len(pattern_ids) - covered_members,
            "covering_runtime_archetype_count": len(coverage_names),
            "cluster_distance_median": round(statistics.median(pair_distances), 6) if pair_distances else 0.0,
            "cluster_distance_max": round(max(pair_distances), 6) if pair_distances else 0.0,
            "nearest_lane_edit_lower_bound_median": round(statistics.median(nearest_edit_lower_bounds), 3),
            "covered_typicality_median": round(statistics.median(typicality), 6) if typicality else "",
            "ambiguous_or_unmapped_event_count": candidate_ambiguous_events,
            "decision": decision,
        })

    duplicate_null = []
    for pattern_ids in by_source_group.values():
        for position, left in enumerate(pattern_ids):
            if left not in masks:
                continue
            for right in pattern_ids[position + 1:]:
                if right in masks:
                    duplicate_null.append(observation_distance(masks[left], masks[right]))
    within_direction = []
    by_direction = defaultdict(list)
    for pattern_id in representatives:
        for direction in directions_by_pattern[pattern_id]:
            by_direction[direction].append(pattern_id)
    for pattern_ids in by_direction.values():
        unique = sorted(set(pattern_ids))
        for position, left in enumerate(unique):
            for right in unique[position + 1:]:
                within_direction.append(observation_distance(masks[left], masks[right]))
    candidate_internal = []
    for _key, pattern_ids in recurring:
        for position, left in enumerate(pattern_ids):
            for right in pattern_ids[position + 1:]:
                candidate_internal.append(observation_distance(masks[left], masks[right]))
    calibration_rows = [
        distribution("atlas_duplicate_null_same_source_structural_group", duplicate_null),
        distribution("atlas_within_direction_research_drum_jaccard", within_direction),
        distribution("atlas_recurring_candidate_internal_drum_jaccard", candidate_internal),
    ]

    role_rows = [
        {"semantic_role": role, "mapping_reason": reason, "event_count": count}
        for (role, reason), count in sorted(mapping_counts.items())
    ]
    output_dir.mkdir(parents=True, exist_ok=True)
    write_csv(output_dir / "ATLAS_PASS2_HARDENED_CANDIDATES.csv", candidate_rows, [
        "candidate_id", "structural_group_count", "independent_provenance_root_count",
        "content_deduped_artifact_count", "direction_count", "directions",
        "hard_covered_member_count", "hard_uncovered_member_count",
        "covering_runtime_archetype_count", "cluster_distance_median", "cluster_distance_max",
        "nearest_lane_edit_lower_bound_median", "covered_typicality_median",
        "ambiguous_or_unmapped_event_count", "decision",
    ])
    write_csv(output_dir / "ATLAS_PASS2_CALIBRATION_DISTRIBUTIONS.csv", calibration_rows, [
        "metric", "count", "min", "p05", "p25", "p50", "p75", "p90", "p95", "max",
    ])
    write_csv(output_dir / "ATLAS_PASS2_ROLE_MAPPING_AUDIT.csv", role_rows, [
        "semantic_role", "mapping_reason", "event_count",
    ])

    summary = {
        "atlas_sha256": digest,
        "schema_version": EXPECTED_SCHEMA_VERSION,
        "role_mapping_schema": ROLE_MAPPING_SCHEMA,
        "runtime_topology_schema": GRAMMAR_SCHEMA,
        "support_schema": SUPPORT_SCHEMA,
        "distance_schema": DISTANCE_SCHEMA,
        "patterns": len(patterns),
        "events": len(events),
        "bar_counts": {str(key): bar_counts[key] for key in sorted(bar_counts)},
        "research_one_bar_patterns": len(eligible),
        "research_source_structural_groups": len(representatives),
        "ambiguous_or_unmapped_event_count": ambiguous_events,
        "recurring_hardened_candidates": len(candidate_rows),
        "candidate_decisions": dict(Counter(row["decision"] for row in candidate_rows)),
        "stage7_production_admission": "CLOSED",
        "stage7_audition_policy": (
            "Up to five aggregate candidates may enter a temporary listening harness; "
            "audition is not runtime admission."
        ),
        "claim_boundary": (
            "NOVEL_CANDIDATE is not emitted by Pass 2 hardening; "
            "ACCEPT belongs to Stage 7 curation/listening."
        ),
    }
    (output_dir / "ATLAS_PASS2_HARDENING_SUMMARY.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return summary


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("atlas_zip", type=Path)
    parser.add_argument("runtime_topology", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    print(json.dumps(
        extract_hardening(args.atlas_zip, args.runtime_topology, args.output_dir),
        indent=2,
        sort_keys=True,
    ))


if __name__ == "__main__":
    main()
