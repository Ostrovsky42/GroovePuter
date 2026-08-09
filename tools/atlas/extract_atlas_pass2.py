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
EXPECTED_RUNTIME_ARCHETYPE_COUNT = 20

ROLE_NAMES = {
    0: "Kick",
    1: "Backbeat",
    2: "ClosedHat",
    3: "OpenHat",
    4: "Percussion",
    5: "BassRhythm",
    6: "ChordRhythm",
    7: "MelodicRhythm",
}
DRUM_ROLES = (0, 1, 2, 3, 4)
ORNAMENT_ROLES = (2, 3, 4)
DRUM_ROLE_WEIGHTS = {0: 3.0, 1: 3.0, 2: 1.5, 3: 1.0, 4: 1.0}
ORNAMENT_ROLE_WEIGHTS = {2: 1.5, 3: 1.0, 4: 1.0}

SENSITIVE_OUTPUT_TOKENS = (
    "pattern_id",
    "structural_group_id",
    "structural_hash",
    "expressive_hash",
    "source_locator",
    "exact_mask",
    "kick_mask",
    "backbeat_mask",
)

def rows(zf: zipfile.ZipFile, root: str, rel: str) -> list[dict[str, str]]:
    return list(csv.DictReader(io.StringIO(zf.read(root + rel).decode("utf-8-sig"))))

def q(values: list[float], quantile: float) -> float:
    if not values:
        return 0.0
    xs = sorted(values)
    if len(xs) == 1:
        return xs[0]
    pos = (len(xs) - 1) * quantile
    lo = math.floor(pos)
    hi = math.ceil(pos)
    if lo == hi:
        return xs[lo]
    return xs[lo] * (hi - pos) + xs[hi] * (pos - lo)

def summarize_distribution(name: str, values: list[float]) -> dict[str, object]:
    xs = sorted(values)
    return {
        "metric": name,
        "count": len(xs),
        "min": round(xs[0], 6) if xs else 0.0,
        "p05": round(q(xs, 0.05), 6),
        "p25": round(q(xs, 0.25), 6),
        "p50": round(q(xs, 0.50), 6),
        "p75": round(q(xs, 0.75), 6),
        "p90": round(q(xs, 0.90), 6),
        "p95": round(q(xs, 0.95), 6),
        "max": round(xs[-1], 6) if xs else 0.0,
    }

def map_track(track: dict[str, str]) -> int | None:
    tid = track["track_id"].upper()
    role = track["track_role"].lower()
    if tid == "KICK":
        return 0
    if tid in {"SNARE", "CLAP", "HAND_CLAP"}:
        return 1
    if tid in {"CLOSED_HAT", "HAT1", "MID_HAT", "HAT_FOOT"}:
        return 2
    if tid in {"OPEN_HAT", "HAT2", "RIDE"}:
        return 3
    if role == "bass":
        return 5
    if role == "harmony":
        return 6
    if role == "melody":
        return 7
    if role in {"annotation", "sample"} or "UNKNOWN" in track["track_role"].upper():
        return None
    if role in {"drums", "drum_or_percussion"}:
        return 4
    return None

def mask_to_steps(mask: int) -> set[int]:
    return {step for step in range(16) if mask & (1 << (15 - step))}

def load_runtime(path: Path) -> dict[int, dict[str, object]]:
    runtime: dict[int, dict[str, object]] = {}
    saw_format = False
    saw_count = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        if not raw:
            continue
        parts = raw.split("\t")
        kind = parts[0]
        if kind == "FORMAT":
            if parts[1] != "GROOVEPUTER_RUNTIME_RHYTHM_V1":
                raise ValueError("unexpected runtime dump format")
            saw_format = True
        elif kind == "A":
            aid = int(parts[1])
            runtime[aid] = {
                "id": aid,
                "name": parts[2],
                "family": int(parts[3]),
                "active_roles": int(parts[4]),
                "density": tuple(map(int, parts[5:9])),
                "lanes": {},
            }
        elif kind == "L":
            aid = int(parts[1])
            if aid not in runtime:
                raise ValueError("lane before archetype")
            role = int(parts[2])
            immutable, canonical, preferred, optional, forbidden, minimum, maximum, ornament_max = map(int, parts[3:11])
            runtime[aid]["lanes"][role] = {
                "required": mask_to_steps(immutable | canonical),
                "support": mask_to_steps(immutable | canonical | preferred | optional),
                "forbidden": mask_to_steps(forbidden),
                "min": minimum,
                "max": maximum,
                "ornament_max": ornament_max,
            }
        elif kind == "COUNT":
            if int(parts[1]) != EXPECTED_RUNTIME_ARCHETYPE_COUNT:
                raise ValueError("unexpected runtime archetype count")
            saw_count = True
        else:
            raise ValueError(f"unsupported runtime row: {kind}")
    if not saw_format or not saw_count or len(runtime) != EXPECTED_RUNTIME_ARCHETYPE_COUNT:
        raise ValueError("runtime dump incomplete")
    return runtime

def jaccard_distance(a: set[int], b: set[int]) -> float:
    union = a | b
    return 0.0 if not union else 1.0 - (len(a & b) / len(union))

def observation_distance(a: dict[int, set[int]], b: dict[int, set[int]],
                         roles=DRUM_ROLES, weights=DRUM_ROLE_WEIGHTS) -> float:
    total = 0.0
    weight_sum = 0.0
    for role in roles:
        weight = weights[role]
        total += weight * jaccard_distance(a[role], b[role])
        weight_sum += weight
    return total / weight_sum

def runtime_envelope_distance(a: dict[str, object], b: dict[str, object]) -> float:
    total = 0.0
    weight_sum = 0.0
    for role in DRUM_ROLES:
        weight = DRUM_ROLE_WEIGHTS[role]
        lane_a = a["lanes"].get(role)
        lane_b = b["lanes"].get(role)
        support_a = set() if lane_a is None else lane_a["support"]
        support_b = set() if lane_b is None else lane_b["support"]
        total += weight * jaccard_distance(support_a, support_b)
        weight_sum += weight
    return total / weight_sum

def role_compatible(observation: set[int], lane: dict[str, object]) -> bool:
    return (
        lane["required"].issubset(observation)
        and observation.issubset(lane["support"])
        and lane["min"] <= len(observation) <= lane["max"]
    )

def skeleton_runtime_compatibility(kick: set[int], backbeat: set[int],
                                   runtime: dict[int, dict[str, object]]) -> list[str]:
    compatible = []
    for archetype in runtime.values():
        kick_lane = archetype["lanes"].get(0)
        backbeat_lane = archetype["lanes"].get(1)
        if kick_lane and backbeat_lane and role_compatible(kick, kick_lane) and role_compatible(backbeat, backbeat_lane):
            compatible.append(str(archetype["name"]))
    return sorted(compatible)

def grammar_fit(observation: dict[int, set[int]], archetype: dict[str, object]) -> dict[str, float]:
    required_misses = 0
    required_total = 0
    outside_support = 0
    observed_total = 0
    density_deviation = 0
    soft_jaccard_total = 0.0
    weight_sum = 0.0
    for role in DRUM_ROLES:
        lane = archetype["lanes"].get(role)
        observed = observation[role]
        weight = DRUM_ROLE_WEIGHTS[role]
        if lane is None:
            outside_support += len(observed)
            observed_total += len(observed)
            continue
        required_misses += len(lane["required"] - observed)
        required_total += len(lane["required"])
        outside_support += len(observed - lane["support"])
        observed_total += len(observed)
        if len(observed) < lane["min"]:
            density_deviation += lane["min"] - len(observed)
        elif len(observed) > lane["max"]:
            density_deviation += len(observed) - lane["max"]
        soft_jaccard_total += weight * jaccard_distance(observed, lane["support"])
        weight_sum += weight
    return {
        "required_misses": float(required_misses),
        "outside_support_hits": float(outside_support),
        "required_miss_rate": required_misses / required_total if required_total else 0.0,
        "outside_support_rate": outside_support / observed_total if observed_total else 0.0,
        "density_deviation_steps": float(density_deviation),
        "support_jaccard": soft_jaccard_total / weight_sum if weight_sum else 0.0,
    }

def nearest_runtime_fit(observation: dict[int, set[int]],
                        runtime: dict[int, dict[str, object]]) -> tuple[str, dict[str, float]]:
    ranked = []
    for archetype in runtime.values():
        fit = grammar_fit(observation, archetype)
        # Diagnostic only. Preserve hard grammar precedence instead of
        # inventing a weighted scalar distance between observation and grammar.
        rank = (
            fit["required_misses"],
            fit["outside_support_hits"],
            fit["density_deviation_steps"],
            fit["support_jaccard"],
            str(archetype["name"]),
        )
        ranked.append((rank, str(archetype["name"]), fit))
    ranked.sort(key=lambda item: item[0])
    return ranked[0][1], ranked[0][2]

def pattern_masks(pid: str, bars: int, steps_per_bar: int, track_by_pid, events_by_key) -> list[dict[int, set[int]]]:
    if steps_per_bar not in (8, 16):
        raise ValueError(f"unsupported steps_per_bar: {steps_per_bar}")
    result = [{role: set() for role in range(8)} for _ in range(bars)]
    for track in track_by_pid[pid]:
        role = map_track(track)
        if role is None:
            continue
        for event in events_by_key[(pid, track["track_id"])]:
            bar = int(event.get("bar_index") or "1") - 1
            source_step = int(event["step_index"]) - 1
            step = source_step if steps_per_bar == 16 else source_step * 2
            if 0 <= bar < bars and 0 <= step < 16:
                result[bar][role].add(step)
    return result

def relation_features(source: set[int], target: set[int]) -> dict[str, float] | None:
    if not source or not target:
        return None
    coincide = len(source & target) / len(target)
    target_in_gaps = len(target - source) / len(target)
    respond = sum(any((target_step - delta) in source for delta in (1, 2, 3)) for target_step in target) / len(target)
    anticipate = sum(any((target_step + delta) in source for delta in (1, 2)) for target_step in target) / len(target)
    source_windows = 0
    for source_step in source:
        if any(1 <= target_step - source_step <= 4 for target_step in target):
            source_windows += 1
    return {
        "coincide_fraction": coincide,
        "target_in_gaps_fraction": target_in_gaps,
        "respond_1_3_fraction": respond,
        "anticipate_1_2_fraction": anticipate,
        "source_response_window_fraction": source_windows / len(source),
    }

def transition_features(a: dict[int, set[int]], b: dict[int, set[int]]) -> dict[str, object]:
    adds = drops = 0
    for role in DRUM_ROLES:
        adds += len(b[role] - a[role])
        drops += len(a[role] - b[role])
    if adds == 0 and drops == 0:
        kind = "EXACT_REPEAT"
    elif adds and not drops:
        kind = "ADD_ONLY"
    elif drops and not adds:
        kind = "DROP_ONLY"
    else:
        kind = "MIXED"
    return {"transition_class": kind, "adds": adds, "drops": drops}

def contour_class(notes: list[int]) -> str:
    if len(notes) < 2:
        return "INSUFFICIENT"
    compact = [notes[0]]
    for note in notes[1:]:
        if note != compact[-1]:
            compact.append(note)
    if len(compact) == 1:
        return "STATIC"
    signs = []
    for a, b in zip(compact, compact[1:]):
        signs.append(1 if b > a else -1)
    if all(sign > 0 for sign in signs):
        return "RISE"
    if all(sign < 0 for sign in signs):
        return "FALL"
    changes = sum(1 for a, b in zip(signs, signs[1:]) if a != b)
    if changes == 1 and signs[0] > 0:
        return "ARCH"
    if changes == 1 and signs[0] < 0:
        return "VALLEY"
    return "PENDULUM_OR_MIXED"

def note_sequence(pid: str, role_name: str, track_by_pid, events_by_key) -> list[int]:
    items: list[tuple[int, int, int]] = []
    for track in track_by_pid[pid]:
        if track["track_role"].lower() != role_name:
            continue
        for event in events_by_key[(pid, track["track_id"])]:
            raw = (event.get("midi_note") or "").strip()
            if raw:
                items.append((int(event.get("bar_index") or "1"), int(event["step_index"]), int(raw)))
    return [note for _, _, note in sorted(items)]

def write_csv(path: Path, rows_: list[dict[str, object]], fieldnames: list[str]) -> None:
    lowered = {name.lower() for name in fieldnames}
    for token in SENSITIVE_OUTPUT_TOKENS:
        if token in lowered:
            raise ValueError(f"restricted output column: {token}")
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows_)

def extract(atlas_zip: Path, runtime_tsv: Path, output_dir: Path) -> dict[str, object]:
    digest = hashlib.sha256(atlas_zip.read_bytes()).hexdigest()
    if digest != EXPECTED_ATLAS_SHA256:
        raise ValueError(f"unexpected Atlas archive SHA-256: {digest}")

    with zipfile.ZipFile(atlas_zip) as zf:
        roots = {name.split("/", 1)[0] for name in zf.namelist() if "/" in name}
        if len(roots) != 1:
            raise ValueError("Atlas ZIP must have exactly one root")
        root = next(iter(roots)) + "/"
        validation = json.loads(zf.read(root + "reports/validation_summary.json"))
        if validation.get("schema_version") != EXPECTED_SCHEMA_VERSION or validation.get("failures") != 0:
            raise ValueError("Atlas validation gate failed")

        patterns = rows(zf, root, "core/patterns.csv")
        tracks = rows(zf, root, "core/pattern_tracks.csv")
        events = rows(zf, root, "core/pattern_events.csv")
        relations = rows(zf, root, "core/pattern_relations.csv")
        direction_links = rows(zf, root, "core/pattern_direction_links.csv")
        recipes = rows(zf, root, "core/recipes.csv")
        recipe_patterns = rows(zf, root, "core/recipe_patterns.csv")

    if len(patterns) != EXPECTED_PATTERN_COUNT or len(events) != EXPECTED_EVENT_COUNT:
        raise ValueError("Atlas corpus size changed")

    runtime = load_runtime(runtime_tsv)
    pattern_map = {pattern["pattern_id"]: pattern for pattern in patterns}
    track_by_pid = defaultdict(list)
    events_by_key = defaultdict(list)
    directions_by_pid = defaultdict(list)
    for track in tracks:
        track_by_pid[track["pattern_id"]].append(track)
    for event in events:
        events_by_key[(event["pattern_id"], event["track_id"])].append(event)
    for link in direction_links:
        directions_by_pid[link["pattern_id"]].append(link["direction_id"])

    eligible = [
        pattern for pattern in patterns
        if pattern["bars"] == "1"
        and pattern["steps_per_bar"] == "16"
        and pattern["pattern_kind"] != "COMPOSITE_SEQUENCE"
        and pattern["pattern_kind"] != "INTERNAL_PROTOTYPE"
        and pattern["publication_status"] != "SUPERSEDED"
    ]
    eligible_map = {pattern["pattern_id"]: pattern for pattern in eligible}
    masks = {
        pattern["pattern_id"]: pattern_masks(pattern["pattern_id"], 1, 16, track_by_pid, events_by_key)[0]
        for pattern in eligible
    }

    structural_groups = defaultdict(list)
    for pattern in eligible:
        structural_groups[pattern["structural_group_id"]].append(pattern["pattern_id"])
    representatives = []
    for group_id, pids in structural_groups.items():
        declared = eligible_map[pids[0]].get("structure_representative_pattern_id", "")
        representatives.append(declared if declared in pids else sorted(pids)[0])
    representatives = sorted(representatives)

    atlas_pair_distances = []
    for i, left in enumerate(representatives):
        for right in representatives[i + 1:]:
            atlas_pair_distances.append(observation_distance(masks[left], masks[right]))

    variation_distances = []
    for relation in relations:
        if relation["relation_type"] != "VARIATION_OF":
            continue
        left = relation["source_pattern_id"]
        right = relation["target_pattern_id"]
        if left in masks and right in masks:
            variation_distances.append(observation_distance(masks[left], masks[right]))

    runtime_ids = sorted(runtime)
    runtime_pair_distances = []
    for i, left in enumerate(runtime_ids):
        for right in runtime_ids[i + 1:]:
            runtime_pair_distances.append(runtime_envelope_distance(runtime[left], runtime[right]))

    nearest_runtime_rows = []
    for pid in representatives:
        runtime_name, fit = nearest_runtime_fit(masks[pid], runtime)
        nearest_runtime_rows.append((pid, runtime_name, fit))

    skeleton_groups = defaultdict(list)
    for pid in representatives:
        skeleton_groups[(tuple(sorted(masks[pid][0])), tuple(sorted(masks[pid][1])))].append(pid)
    recurring = [(key, pids) for key, pids in skeleton_groups.items() if len(pids) >= 3]
    recurring.sort(key=lambda item: (-len(item[1]), item[0]))

    topology_rows = []
    for index, (key, pids) in enumerate(recurring, 1):
        kick = set(key[0])
        backbeat = set(key[1])
        compatible = skeleton_runtime_compatibility(kick, backbeat, runtime)
        source_ids = {eligible_map[pid]["source_id"] for pid in pids}
        directions = {direction for pid in pids for direction in directions_by_pid[pid]}
        research_count = sum(1 for pid in pids if eligible_map[pid]["pattern_kind"] == "SOURCE_OBSERVATION")
        editorial_count = sum(1 for pid in pids if eligible_map[pid]["pattern_kind"] == "EDITORIAL_RECIPE_PATTERN")
        evidence_mix = (
            "RESEARCH+PROJECT"
            if research_count and editorial_count
            else ("RESEARCH_ONLY" if research_count else "PROJECT_ONLY")
        )
        ornament_distances = [
            observation_distance(masks[left], masks[right], ORNAMENT_ROLES, ORNAMENT_ROLE_WEIGHTS)
            for pos, left in enumerate(pids)
            for right in pids[pos + 1:]
        ]
        if compatible:
            decision = "NEAR_EXISTING"
        elif len(source_ids) >= 2:
            decision = "REVIEW"
        else:
            decision = "HOLD"
        topology_rows.append({
            "candidate_id": f"SKEL_{index:02d}",
            "structural_group_count": len(pids),
            "source_count": len(source_ids),
            "direction_count": len(directions),
            "directions": "|".join(sorted(directions)),
            "evidence_mix": evidence_mix,
            "research_observation_count": research_count,
            "project_editorial_count": editorial_count,
            "runtime_compatible_archetype_count": len(compatible),
            "runtime_compatible_archetypes": "|".join(compatible),
            "ornament_distance_median": round(statistics.median(ornament_distances), 6) if ornament_distances else 0.0,
            "ornament_distance_max": round(max(ornament_distances), 6) if ornament_distances else 0.0,
            "decision": decision,
        })

    research_representatives = [
        pid for pid in representatives
        if eligible_map[pid]["pattern_kind"] == "SOURCE_OBSERVATION"
    ]
    research_pair_distances = []
    for i, left in enumerate(research_representatives):
        for right in research_representatives[i + 1:]:
            research_pair_distances.append(observation_distance(masks[left], masks[right]))

    distance_rows = [
        summarize_distribution("atlas_all_structural_group_pair_drum_jaccard", atlas_pair_distances),
        summarize_distribution("atlas_research_structural_group_pair_drum_jaccard", research_pair_distances),
        summarize_distribution("atlas_variation_of_drum_jaccard", variation_distances),
        summarize_distribution("runtime_grammar_envelope_pair_jaccard_diagnostic", runtime_pair_distances),
        summarize_distribution(
            "atlas_to_nearest_runtime_required_miss_rate_diagnostic",
            [row[2]["required_miss_rate"] for row in nearest_runtime_rows],
        ),
        summarize_distribution(
            "atlas_to_nearest_runtime_outside_support_rate_diagnostic",
            [row[2]["outside_support_rate"] for row in nearest_runtime_rows],
        ),
        summarize_distribution(
            "atlas_to_nearest_runtime_density_deviation_steps_diagnostic",
            [row[2]["density_deviation_steps"] for row in nearest_runtime_rows],
        ),
        summarize_distribution(
            "atlas_to_nearest_runtime_support_jaccard_diagnostic",
            [row[2]["support_jaccard"] for row in nearest_runtime_rows],
        ),
    ]

    relationship_rows = []
    relationship_specs = [
        ("KickToBackbeat", 0, 1),
        ("KickToClosedHat", 0, 2),
        ("KickToOpenHat", 0, 3),
        ("KickToPercussion", 0, 4),
    ]
    source_observations = [
        pattern for pattern in eligible if pattern["pattern_kind"] == "SOURCE_OBSERVATION"
    ]
    for domain, source_role, target_role in relationship_specs:
        by_direction_group = {}
        for pattern in source_observations:
            pid = pattern["pattern_id"]
            features = relation_features(masks[pid][source_role], masks[pid][target_role])
            if features is None:
                continue
            for direction in directions_by_pid[pid]:
                key = (direction, pattern["structural_group_id"])
                by_direction_group.setdefault(key, features)
        by_direction = defaultdict(list)
        for (direction, _group_id), features in by_direction_group.items():
            by_direction[direction].append(features)
        for direction, feature_rows in sorted(by_direction.items()):
            if len(feature_rows) < 3:
                continue
            relationship_rows.append({
                "domain": domain,
                "evidence_class": "RESEARCH_AGGREGATE",
                "direction": direction,
                "structural_group_count": len(feature_rows),
                **{
                    key: round(statistics.median([row[key] for row in feature_rows]), 6)
                    for key in (
                        "coincide_fraction",
                        "target_in_gaps_fraction",
                        "respond_1_3_fraction",
                        "anticipate_1_2_fraction",
                        "source_response_window_fraction",
                    )
                },
            })

    bass_by_direction_group = {}
    for pattern in eligible:
        pid = pattern["pattern_id"]
        if not masks[pid][5]:
            continue
        features = relation_features(masks[pid][0], masks[pid][5])
        if features is None:
            continue
        for direction in directions_by_pid[pid]:
            key = (direction, pattern["structural_group_id"])
            bass_by_direction_group.setdefault(key, features)
    bass_by_direction = defaultdict(list)
    for (direction, _group_id), features in bass_by_direction_group.items():
        bass_by_direction[direction].append(features)
    for direction, feature_rows in sorted(bass_by_direction.items()):
        if len(feature_rows) < 3:
            continue
        relationship_rows.append({
            "domain": "KickToBassRhythm",
            "evidence_class": "PROJECT_OWNED_EXACT",
            "direction": direction,
            "structural_group_count": len(feature_rows),
            **{
                key: round(statistics.median([row[key] for row in feature_rows]), 6)
                for key in (
                    "coincide_fraction",
                    "target_in_gaps_fraction",
                    "respond_1_3_fraction",
                    "anticipate_1_2_fraction",
                    "source_response_window_fraction",
                )
            },
        })

    measured_phrase_patterns = [
        pattern for pattern in patterns
        if pattern["pattern_kind"] == "SOURCE_OBSERVATION"
        and pattern["bars"] in {"2", "4"}
        and pattern["steps_per_bar"] in {"8", "16"}
    ]
    measured_transition_rows = []
    for pattern in measured_phrase_patterns:
        bar_masks = pattern_masks(pattern["pattern_id"], int(pattern["bars"]), int(pattern["steps_per_bar"]), track_by_pid, events_by_key)
        for left, right in zip(bar_masks, bar_masks[1:]):
            measured_transition_rows.append(transition_features(left, right))
    measured_class_counts = Counter(row["transition_class"] for row in measured_transition_rows)

    derived_four_bar_patterns = [
        pattern for pattern in patterns
        if pattern["pattern_kind"] == "COMPOSITE_SEQUENCE"
        and pattern["bars"] == "4"
        and pattern["steps_per_bar"] == "16"
    ]
    derived_sequences = Counter()
    for pattern in derived_four_bar_patterns:
        bar_masks = pattern_masks(pattern["pattern_id"], 4, 16, track_by_pid, events_by_key)
        classes = tuple(
            transition_features(left, right)["transition_class"]
            for left, right in zip(bar_masks, bar_masks[1:])
        )
        derived_sequences[" > ".join(classes)] += 1

    phrase_rows = []
    for transition_class in ("EXACT_REPEAT", "ADD_ONLY", "DROP_ONLY", "MIXED"):
        matching = [row for row in measured_transition_rows if row["transition_class"] == transition_class]
        phrase_rows.append({
            "evidence_class": "MEASURED",
            "scope": "two_bar_source_observation_transition",
            "transition_signature": transition_class,
            "count": len(matching),
            "median_adds": round(statistics.median([row["adds"] for row in matching]), 3) if matching else 0,
            "median_drops": round(statistics.median([row["drops"] for row in matching]), 3) if matching else 0,
        })
    for signature, count in sorted(derived_sequences.items()):
        phrase_rows.append({
            "evidence_class": "EDITORIAL_CURATED",
            "scope": "four_bar_derived_composite_sequence",
            "transition_signature": signature,
            "count": count,
            "median_adds": "",
            "median_drops": "",
        })

    pitch_rows = []
    for role_name, domain in (("bass", "BassPitch"), ("melody", "MotifContour")):
        by_direction = defaultdict(list)
        for pattern in eligible:
            seq = note_sequence(pattern["pattern_id"], role_name, track_by_pid, events_by_key)
            if len(seq) < 2:
                continue
            compact = [seq[0]]
            for note in seq[1:]:
                if note != compact[-1]:
                    compact.append(note)
            adjacent = [abs(b - a) for a, b in zip(compact, compact[1:])]
            item = {
                "contour": contour_class(seq),
                "note_count": len(seq),
                "unique_pitch_count": len(set(seq)),
                "stepwise_fraction": (sum(delta <= 2 for delta in adjacent) / len(adjacent)) if adjacent else 0.0,
                "octave_fraction": (sum(delta > 0 and delta % 12 == 0 for delta in adjacent) / len(adjacent)) if adjacent else 0.0,
            }
            for direction in directions_by_pid[pattern["pattern_id"]]:
                by_direction[direction].append(item)
        for direction, items in sorted(by_direction.items()):
            if len(items) < 3:
                continue
            contours = Counter(item["contour"] for item in items)
            pitch_rows.append({
                "domain": domain,
                "evidence_class": "PROJECT_OWNED_EXACT",
                "direction": direction,
                "pattern_count": len(items),
                "dominant_contour": contours.most_common(1)[0][0],
                "dominant_contour_count": contours.most_common(1)[0][1],
                "median_note_count": round(statistics.median(item["note_count"] for item in items), 3),
                "median_unique_pitch_count": round(statistics.median(item["unique_pitch_count"] for item in items), 3),
                "median_stepwise_fraction": round(statistics.median(item["stepwise_fraction"] for item in items), 6),
                "median_octave_fraction": round(statistics.median(item["octave_fraction"] for item in items), 6),
                "decision": "HOLD",
            })

    evidence_rows = [
        {
            "domain": "RhythmTopology",
            "evidence_class": "RESEARCH_AGGREGATE",
            "eligible_pattern_count": sum(1 for pattern in eligible if pattern["pattern_kind"] == "SOURCE_OBSERVATION"),
            "distinct_structural_group_count": len({
                pattern["structural_group_id"] for pattern in eligible
                if pattern["pattern_kind"] == "SOURCE_OBSERVATION"
            }),
            "note": "Strongest Pass 2 evidence domain.",
        },
        {
            "domain": "BassRhythm",
            "evidence_class": "PROJECT_OWNED_EXACT",
            "eligible_pattern_count": sum(1 for pattern in eligible if masks[pattern["pattern_id"]][5]),
            "distinct_structural_group_count": len({
                pattern["structural_group_id"] for pattern in eligible
                if masks[pattern["pattern_id"]][5]
            }),
            "note": "One-bar BassRhythm is editorial/project-owned, not research aggregate.",
        },
        {
            "domain": "BassPitch",
            "evidence_class": "PROJECT_OWNED_EXACT",
            "eligible_pattern_count": sum(
                1 for pattern in eligible
                if len(note_sequence(pattern["pattern_id"], "bass", track_by_pid, events_by_key)) >= 2
            ),
            "distinct_structural_group_count": "",
            "note": "Useful for hypothesis generation only; Stage 8 admission remains HOLD.",
        },
        {
            "domain": "MotifContour",
            "evidence_class": "PROJECT_OWNED_EXACT",
            "eligible_pattern_count": sum(
                1 for pattern in eligible
                if len(note_sequence(pattern["pattern_id"], "melody", track_by_pid, events_by_key)) >= 2
            ),
            "distinct_structural_group_count": "",
            "note": "No research-measured one-bar motif corpus in v2.6.",
        },
        {
            "domain": "PhraseDevelopment",
            "evidence_class": "MEASURED",
            "eligible_pattern_count": len(measured_phrase_patterns),
            "distinct_structural_group_count": len({pattern["structural_group_id"] for pattern in measured_phrase_patterns}),
            "note": "Measured evidence is 2-bar source observations; 4-bar sequences are derived/editorial.",
        },
    ]

    published_recipe_ids = {
        recipe["recipe_id"]
        for recipe in recipes
        if recipe["publication_status"].startswith("PUBLISHED")
    }
    slot_groups = defaultdict(list)
    for link in recipe_patterns:
        if link["recipe_id"] not in published_recipe_ids:
            continue
        slot_groups[link["slot_id"]].append(pattern_map[link["pattern_id"]]["structural_group_id"])
    effective_variation_rows = []
    for slot_id in ("P1", "P2", "P3"):
        groups_for_slot = slot_groups.get(slot_id, [])
        distinct_count = len(set(groups_for_slot))
        effective_variation_rows.append({
            "scope": "published_recipe_structural_groups",
            "slot": slot_id,
            "realization_count": len(groups_for_slot),
            "effective_variation_count": distinct_count,
            "effective_variation_ratio": round(
                distinct_count / len(groups_for_slot), 6
            ) if groups_for_slot else 0.0,
        })

    output_dir.mkdir(parents=True, exist_ok=True)
    write_csv(
        output_dir / "ATLAS_PASS2_TOPOLOGY_CANDIDATES.csv",
        topology_rows,
        [
            "candidate_id", "structural_group_count", "source_count", "direction_count",
            "directions", "evidence_mix", "research_observation_count", "project_editorial_count",
            "runtime_compatible_archetype_count", "runtime_compatible_archetypes",
            "ornament_distance_median", "ornament_distance_max", "decision",
        ],
    )
    write_csv(
        output_dir / "ATLAS_PASS2_DISTANCE_DISTRIBUTIONS.csv",
        distance_rows,
        ["metric", "count", "min", "p05", "p25", "p50", "p75", "p90", "p95", "max"],
    )
    write_csv(
        output_dir / "ATLAS_PASS2_RELATIONSHIPS.csv",
        relationship_rows,
        [
            "domain", "evidence_class", "direction", "structural_group_count",
            "coincide_fraction", "target_in_gaps_fraction", "respond_1_3_fraction",
            "anticipate_1_2_fraction", "source_response_window_fraction",
        ],
    )
    write_csv(
        output_dir / "ATLAS_PASS2_PHRASE_TRANSITIONS.csv",
        phrase_rows,
        [
            "evidence_class", "scope", "transition_signature", "count",
            "median_adds", "median_drops",
        ],
    )
    write_csv(
        output_dir / "ATLAS_PASS2_PITCH_CONTOURS.csv",
        pitch_rows,
        [
            "domain", "evidence_class", "direction", "pattern_count",
            "dominant_contour", "dominant_contour_count", "median_note_count",
            "median_unique_pitch_count", "median_stepwise_fraction",
            "median_octave_fraction", "decision",
        ],
    )
    write_csv(
        output_dir / "ATLAS_PASS2_EFFECTIVE_VARIATION_BASELINE.csv",
        effective_variation_rows,
        [
            "scope", "slot", "realization_count", "effective_variation_count",
            "effective_variation_ratio",
        ],
    )
    write_csv(
        output_dir / "ATLAS_PASS2_EVIDENCE_COVERAGE.csv",
        evidence_rows,
        [
            "domain", "evidence_class", "eligible_pattern_count",
            "distinct_structural_group_count", "note",
        ],
    )

    summary = {
        "atlas_sha256": digest,
        "schema_version": EXPECTED_SCHEMA_VERSION,
        "validation_failures": validation.get("failures"),
        "patterns": len(patterns),
        "events": len(events),
        "one_bar_eligible_patterns": len(eligible),
        "one_bar_structural_groups": len(structural_groups),
        "recurring_skeleton_candidates": len(topology_rows),
        "topology_decisions": dict(Counter(row["decision"] for row in topology_rows)),
        "measured_phrase_patterns": len(measured_phrase_patterns),
        "measured_phrase_transition_counts": dict(measured_class_counts),
        "derived_four_bar_patterns": len(derived_four_bar_patterns),
        "published_recipe_effective_variation": {
            row["slot"]: {
                "realization_count": row["realization_count"],
                "effective_variation_count": row["effective_variation_count"],
                "effective_variation_ratio": row["effective_variation_ratio"],
            }
            for row in effective_variation_rows
        },
        "bass_rhythm_one_bar_patterns": sum(1 for pattern in eligible if masks[pattern["pattern_id"]][5]),
        "bass_pitch_contour_eligible_patterns": sum(
            1 for pattern in eligible
            if len(note_sequence(pattern["pattern_id"], "bass", track_by_pid, events_by_key)) >= 2
        ),
        "melodic_rhythm_one_bar_patterns": sum(1 for pattern in eligible if masks[pattern["pattern_id"]][7]),
        "motif_contour_eligible_patterns": sum(
            1 for pattern in eligible
            if len(note_sequence(pattern["pattern_id"], "melody", track_by_pid, events_by_key)) >= 2
        ),
        "stage7_admission": "CLOSED",
        "stage7_reason": (
            "Pass 2 identifies recurring skeleton candidates but does not elevate them "
            "to runtime archetypes. Observation-to-grammar comparison is diagnostic only; "
            "human curation plus listening and cross-layer review remain required."
        ),
        "rights_guard": (
            "Committed outputs contain aggregate counts/distributions only and omit pattern IDs, "
            "structural group IDs, hashes, source locators, event lists and literal masks."
        ),
    }
    (output_dir / "ATLAS_PASS2_SUMMARY.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return summary

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("atlas_zip", type=Path)
    parser.add_argument("runtime_tsv", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    summary = extract(args.atlas_zip, args.runtime_tsv, args.output_dir)
    print(json.dumps(summary, indent=2, sort_keys=True))

if __name__ == "__main__":
    main()
