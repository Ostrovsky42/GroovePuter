#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import harmony_atlas_h2 as h2

SCHEMA_VERSION = "1.0.0"
STAGE = "H3_FUNCTIONAL_ANALYSIS"
EXPECTED_H1_NORMALIZED_SHA256 = "4783be1f77784a978f2239d8527214b3912eaf262e92430bff755679fec0734e"
EXPECTED_H2_FINGERPRINTS_SHA256 = "453287577707e676128f3f83d1215922e22049ab0d880492f95af20ea7e68b0b"

FUNCTIONAL_ROLES = (
    "TONIC",
    "PREDOMINANT",
    "DOMINANT",
    "UNALTERED_OTHER",
    "MODAL_COLOR",
    "CHROMATIC_COLOR",
    "UNKNOWN",
)
COLOR_CLASSES = (
    "UNALTERED_CONTEXT",
    "BORROWED_CANDIDATE",
    "CHROMATIC_OTHER",
    "MODAL_CONTEXT",
    "UNKNOWN",
)
CONFIDENCE_LEVELS = ("HIGH", "MEDIUM", "LOW")
CADENCE_CLASSES = (
    "AUTHENTIC_CADENCE_CANDIDATE",
    "PLAGAL_CADENCE_CANDIDATE",
    "HALF_CADENCE_CANDIDATE",
    "DECEPTIVE_CADENCE_CANDIDATE",
    "MODAL_AMBIGUOUS",
    "NO_CADENCE",
    "UNKNOWN",
)
CLOSURE_CLASSES = (
    "OPEN_LOOP",
    "CLOSED_TONIC_LOOP",
    "DOMINANT_LOOP",
    "MODAL_AMBIGUOUS_LOOP",
    "TURNAROUND",
    "CADENTIAL",
    "UNKNOWN",
)

MAJOR_BORROWED_ROOT_CANDIDATES = {(2, -1), (5, -1), (6, -1)}


class FunctionalAnalysisError(RuntimeError):
    pass


def canonical_json(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify_json_bytes(data: bytes, expected_sha256: str, label: str) -> str:
    digest = sha256_bytes(data)
    if digest != expected_sha256:
        raise FunctionalAnalysisError(
            f"{label} digest mismatch: expected {expected_sha256}, got {digest}"
        )
    return digest


def load_verified_json(path: Path, expected_sha256: str, label: str) -> tuple[dict[str, Any], str]:
    data = path.read_bytes()
    digest = verify_json_bytes(data, expected_sha256, label)
    try:
        value = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise FunctionalAnalysisError(f"{label} is not valid UTF-8 JSON: {exc}") from exc
    if not isinstance(value, dict):
        raise FunctionalAnalysisError(f"{label} must contain a JSON object")
    return value, digest


def require_dependencies(normalization: dict[str, Any], fingerprints: dict[str, Any]) -> None:
    if normalization.get("stage") != "H1_CANONICAL_PARSER_NORMALIZATION":
        raise FunctionalAnalysisError("H3 requires H1 normalization input")
    if fingerprints.get("stage") != "H2_STRUCTURAL_FINGERPRINTS_DEDUP":
        raise FunctionalAnalysisError("H3 requires H2 fingerprint input")
    h2_source = fingerprints.get("source", {})
    h1_source = normalization.get("source", {})
    if h2_source.get("commit") != h1_source.get("commit"):
        raise FunctionalAnalysisError("H1/H2 source commit mismatch")
    h2_defs = fingerprints.get("definitions")
    h1_defs = normalization.get("definitions")
    if not isinstance(h2_defs, list) or not isinstance(h1_defs, list):
        raise FunctionalAnalysisError("H1/H2 definitions are missing")
    if len(h2_defs) != len(h1_defs):
        raise FunctionalAnalysisError("H1/H2 definition count mismatch")
    if int(normalization.get("summary", {}).get("quarantined_definition_count", -1)) != 0:
        raise FunctionalAnalysisError("H3 refuses H1 input with quarantined definitions")


def recompute_and_verify_f0_f3(
    normalization: dict[str, Any],
    fingerprints: dict[str, Any],
    *,
    h1_digest: str,
) -> dict[str, Any]:
    rebuilt = h2.build_fingerprints(normalization, h1_input_sha256=h1_digest)
    expected_rows = {
        row["source_id"]: row["fingerprints"] for row in fingerprints["definitions"]
    }
    actual_rows = {
        row["source_id"]: row["fingerprints"] for row in rebuilt["definitions"]
    }
    if expected_rows != actual_rows:
        mismatches = sorted(set(expected_rows) | set(actual_rows))
        bad = [
            source_id
            for source_id in mismatches
            if expected_rows.get(source_id) != actual_rows.get(source_id)
        ]
        raise FunctionalAnalysisError(
            "H3 detected F0-F3 drift relative to frozen H2: " + ", ".join(bad[:10])
        )
    for level in ("F0", "F1", "F2", "F3"):
        expected = fingerprints["levels"][level]["summary"]
        actual = rebuilt["levels"][level]["summary"]
        for field in (
            "definition_count",
            "unique_class_count",
            "duplicate_group_count",
            "duplicate_surplus_count",
        ):
            if expected[field] != actual[field]:
                raise FunctionalAnalysisError(
                    f"H3 detected {level} summary drift in {field}"
                )
    return {
        "verified": True,
        "definition_count": len(actual_rows),
        "levels": ["F0", "F1", "F2", "F3"],
    }


def quality_signature(event: dict[str, Any]) -> tuple[str, str, str, int]:
    quality = event["quality"]
    return (
        str(quality["triad_class"]),
        str(quality["extension_class"]),
        str(quality["seventh_flavor"]),
        int(quality["fifth_alteration_semitones"]),
    )


def borrowed_candidate(
    family: str, degree: int, alteration: int, triad_class: str
) -> tuple[bool, str]:
    if family == "Major":
        if (degree, alteration) in MAJOR_BORROWED_ROOT_CANDIDATES:
            return True, "MAJOR_PARALLEL_MODE_ROOT_CANDIDATE"
        if alteration == 0 and degree == 3 and triad_class == "MINOR":
            return True, "MAJOR_MINOR_IV_CANDIDATE"
        if alteration == 0 and degree == 0 and triad_class == "MINOR":
            return True, "MAJOR_MINOR_I_CANDIDATE"
        return False, "NO_CONSERVATIVE_MAJOR_BORROWED_RULE"
    if family == "Minor":
        if alteration == 0 and degree == 3 and triad_class == "MAJOR":
            return True, "MINOR_MAJOR_IV_CANDIDATE"
        if alteration == 0 and degree == 0 and triad_class == "MAJOR":
            return True, "MINOR_MAJOR_I_CANDIDATE"
        return False, "NO_CONSERVATIVE_MINOR_BORROWED_RULE"
    return False, "BORROWED_NOT_ASSERTED_IN_MODAL_CONTEXT"


def classify_event(family: str, event: dict[str, Any]) -> dict[str, Any]:
    if event["kind"] == "REST":
        return {"kind": "REST"}

    root = event["root"]
    quality = event["quality"]
    degree = int(root["diatonic_degree"])
    alteration = int(root["alteration_semitones"])
    triad = str(quality["triad_class"])
    is_borrowed, borrowed_reason = borrowed_candidate(
        family, degree, alteration, triad
    )

    if family == "Modal":
        if degree == 0 and alteration == 0:
            role, confidence, reason = "TONIC", "HIGH", "MODAL_TONIC_ROOT"
        else:
            role, confidence, reason = (
                "MODAL_COLOR",
                "LOW",
                "MODAL_CONTEXT_COMMON_PRACTICE_FUNCTION_NOT_FORCED",
            )
        color = "MODAL_CONTEXT"
    elif family in ("Major", "Minor"):
        if alteration != 0:
            role, confidence, reason = (
                "CHROMATIC_COLOR",
                "MEDIUM",
                "ALTERED_ROOT_FUNCTION_NOT_FORCED",
            )
        elif degree == 0:
            role, confidence, reason = "TONIC", "HIGH", "UNALTERED_SCALE_DEGREE_I"
        elif degree == 1:
            role, confidence, reason = (
                "PREDOMINANT",
                "MEDIUM",
                "UNALTERED_SCALE_DEGREE_II",
            )
        elif degree == 3:
            role, confidence, reason = (
                "PREDOMINANT",
                "HIGH",
                "UNALTERED_SCALE_DEGREE_IV",
            )
        elif degree == 4:
            if family == "Minor" and triad == "MINOR":
                confidence = "MEDIUM"
                reason = "MINOR_SCALE_DEGREE_V_WEAK_DOMINANT"
            else:
                confidence = "HIGH"
                reason = "UNALTERED_SCALE_DEGREE_V"
            role = "DOMINANT"
        else:
            role, confidence, reason = (
                "UNALTERED_OTHER",
                "MEDIUM",
                "UNALTERED_NON_TPD_DEGREE",
            )

        if is_borrowed:
            color = "BORROWED_CANDIDATE"
        elif alteration != 0:
            color = "CHROMATIC_OTHER"
        else:
            color = "UNALTERED_CONTEXT"
    else:
        role, confidence, reason = "UNKNOWN", "LOW", "UNKNOWN_SOURCE_FAMILY"
        color = "UNKNOWN"

    return {
        "kind": "CHORD",
        "functional_role": role,
        "confidence": confidence,
        "reason_code": reason,
        "color_class": color,
        "borrowed_candidate": is_borrowed,
        "borrowed_reason_code": borrowed_reason,
        "chromatic_root": alteration != 0,
        "root": {
            "diatonic_degree": degree,
            "alteration_semitones": alteration,
        },
        "quality_signature": list(quality_signature(event)),
    }


def f4_event(event: dict[str, Any]) -> dict[str, Any]:
    if event["kind"] == "REST":
        return {"kind": "REST"}
    return {
        "kind": "CHORD",
        "functional_role": event["functional_role"],
        "color_class": event["color_class"],
    }


def f4_fingerprint(functional_events: list[dict[str, Any]]) -> str:
    payload = {"events": [f4_event(event) for event in functional_events]}
    return h2.sha256_json("F4", payload)


def chord_events(events: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [event for event in events if event["kind"] == "CHORD"]


def dominant_resolution_count(events: list[dict[str, Any]]) -> int:
    chords = chord_events(events)
    return sum(
        left["functional_role"] == "DOMINANT"
        and right["functional_role"] == "TONIC"
        for left, right in zip(chords, chords[1:])
    )


def functional_motion_histogram(events: list[dict[str, Any]]) -> dict[str, int]:
    chords = chord_events(events)
    counts = Counter(
        f"{left['functional_role']}->{right['functional_role']}"
        for left, right in zip(chords, chords[1:])
    )
    return dict(sorted(counts.items()))


def quality_entropy_bits(events: list[dict[str, Any]]) -> float:
    chords = chord_events(events)
    if not chords:
        return 0.0
    counts = Counter(tuple(event["quality_signature"]) for event in chords)
    total = len(chords)
    value = -sum(
        (count / total) * math.log2(count / total)
        for count in counts.values()
        if count
    )
    if abs(value) < 1e-12:
        value = 0.0
    return round(value, 6)


def unique_degree_count(events: list[dict[str, Any]]) -> int:
    return len(
        {
            (
                event["root"]["diatonic_degree"],
                event["root"]["alteration_semitones"],
            )
            for event in chord_events(events)
        }
    )


def repetition_period_from_f3_sequence(sequence: tuple[str, ...]) -> int | None:
    length = len(sequence)
    for period in range(1, length):
        if length % period != 0:
            continue
        candidate = sequence[:period]
        if candidate * (length // period) == sequence:
            return period
    return None


def tonic_return_distance(events: list[dict[str, Any]]) -> int | None:
    chords = chord_events(events)
    tonic_positions = [
        index for index, event in enumerate(chords) if event["functional_role"] == "TONIC"
    ]
    if len(tonic_positions) < 2 or tonic_positions[-1] != len(chords) - 1:
        return None
    return tonic_positions[-1] - tonic_positions[-2]


def classify_cadence(
    family: str, events: list[dict[str, Any]]
) -> dict[str, str]:
    chords = chord_events(events)
    if len(chords) < 2:
        return {
            "cadence_class": "UNKNOWN",
            "confidence": "LOW",
            "reason_code": "FEWER_THAN_TWO_CHORD_EVENTS",
        }
    if family == "Modal":
        return {
            "cadence_class": "MODAL_AMBIGUOUS",
            "confidence": "LOW",
            "reason_code": "MODAL_CONTEXT_COMMON_PRACTICE_CADENCE_NOT_FORCED",
        }

    previous, final = chords[-2], chords[-1]
    prev_role = previous["functional_role"]
    final_role = final["functional_role"]
    prev_root = previous["root"]
    final_root = final["root"]

    if prev_role == "DOMINANT" and final_role == "TONIC":
        return {
            "cadence_class": "AUTHENTIC_CADENCE_CANDIDATE",
            "confidence": "HIGH",
            "reason_code": "ADJACENT_DOMINANT_TO_TONIC",
        }
    if (
        prev_role == "PREDOMINANT"
        and prev_root["diatonic_degree"] == 3
        and prev_root["alteration_semitones"] == 0
        and final_role == "TONIC"
    ):
        return {
            "cadence_class": "PLAGAL_CADENCE_CANDIDATE",
            "confidence": "MEDIUM",
            "reason_code": "ADJACENT_IV_FAMILY_TO_TONIC",
        }
    if final_role == "DOMINANT":
        return {
            "cadence_class": "HALF_CADENCE_CANDIDATE",
            "confidence": "MEDIUM",
            "reason_code": "ENDS_ON_DOMINANT_ROLE",
        }
    if (
        prev_role == "DOMINANT"
        and final_root["diatonic_degree"] == 5
        and final_root["alteration_semitones"] == 0
    ):
        return {
            "cadence_class": "DECEPTIVE_CADENCE_CANDIDATE",
            "confidence": "MEDIUM",
            "reason_code": "DOMINANT_TO_UNALTERED_VI",
        }
    return {
        "cadence_class": "NO_CADENCE",
        "confidence": "MEDIUM",
        "reason_code": "NO_SUPPORTED_TERMINAL_CADENCE_RULE",
    }


def classify_closure(
    family: str,
    events: list[dict[str, Any]],
    cadence: dict[str, str],
) -> dict[str, str]:
    chords = chord_events(events)
    if not chords:
        return {
            "closure_class": "UNKNOWN",
            "reason_code": "NO_CHORD_EVENTS",
        }
    if family == "Modal":
        return {
            "closure_class": "MODAL_AMBIGUOUS_LOOP",
            "reason_code": "MODAL_CONTEXT_TONAL_CLOSURE_NOT_FORCED",
        }

    first, final = chords[0], chords[-1]
    cadence_class = cadence["cadence_class"]
    if cadence_class in (
        "AUTHENTIC_CADENCE_CANDIDATE",
        "PLAGAL_CADENCE_CANDIDATE",
        "DECEPTIVE_CADENCE_CANDIDATE",
    ):
        return {
            "closure_class": "CADENTIAL",
            "reason_code": f"TERMINAL_{cadence_class}",
        }
    if final["functional_role"] == "DOMINANT":
        return {
            "closure_class": "DOMINANT_LOOP",
            "reason_code": "FINAL_ROLE_DOMINANT",
        }
    if (
        first["functional_role"] == "TONIC"
        and final["functional_role"] == "TONIC"
    ):
        return {
            "closure_class": "CLOSED_TONIC_LOOP",
            "reason_code": "STARTS_AND_ENDS_ON_TONIC_ROLE",
        }
    if (
        first["functional_role"] == "TONIC"
        and any(event["functional_role"] == "DOMINANT" for event in chords[1:-1])
        and final["functional_role"] in ("PREDOMINANT", "UNALTERED_OTHER")
    ):
        return {
            "closure_class": "TURNAROUND",
            "reason_code": "TONIC_START_INTERNAL_DOMINANT_NONCLOSING_END",
        }
    return {
        "closure_class": "OPEN_LOOP",
        "reason_code": "NO_SUPPORTED_CLOSURE_RULE",
    }


def cadence_tag_comparison(tags: dict[str, list[str]], cadence_class: str) -> str:
    tagged = "Cadence" in tags.get("structural", [])
    derived = cadence_class not in ("NO_CADENCE", "UNKNOWN", "MODAL_AMBIGUOUS")
    if tagged and cadence_class == "MODAL_AMBIGUOUS":
        return "TAGGED_MODAL_UNRESOLVED"
    if tagged and derived:
        return "TAGGED_AND_DERIVED"
    if tagged:
        return "TAGGED_ONLY"
    if derived:
        return "DERIVED_ONLY"
    return "NEITHER"


def analyze_definition(
    definition: dict[str, Any],
    materialized_events: list[dict[str, Any]],
    h2_row: dict[str, Any],
) -> dict[str, Any]:
    family = definition["source_family"]
    functional_events = [
        classify_event(family, event) for event in materialized_events
    ]
    cadence = classify_cadence(family, functional_events)
    closure = classify_closure(family, functional_events, cadence)
    f4 = f4_fingerprint(functional_events)

    f3_sequence = h2.event_sequence_key(
        h2.f3_event(event) for event in materialized_events
    )
    role_counts = Counter(
        event["functional_role"] for event in chord_events(functional_events)
    )
    confidence_counts = Counter(
        event["confidence"] for event in chord_events(functional_events)
    )

    return {
        "source_id": definition["source_id"],
        "source_family": family,
        "tags": definition["tags"],
        "fingerprints": {
            **h2_row["fingerprints"],
            "F4": f4,
        },
        "functional_events": functional_events,
        "features": {
            "closure_class": closure["closure_class"],
            "closure_reason_code": closure["reason_code"],
            **cadence,
            "cadence_tag_comparison": cadence_tag_comparison(
                definition["tags"], cadence["cadence_class"]
            ),
            "chromatic_root_count": sum(
                event.get("chromatic_root", False)
                for event in functional_events
            ),
            "borrowed_candidate_count": sum(
                event.get("borrowed_candidate", False)
                for event in functional_events
            ),
            "dominant_resolution_count": dominant_resolution_count(functional_events),
            "tonic_return_distance": tonic_return_distance(functional_events),
            "unique_degree_count": unique_degree_count(functional_events),
            "quality_entropy_bits": quality_entropy_bits(functional_events),
            "repetition_period_f3_events": repetition_period_from_f3_sequence(
                f3_sequence
            ),
            "functional_role_counts": dict(sorted(role_counts.items())),
            "functional_motion_histogram": functional_motion_histogram(
                functional_events
            ),
            "confidence_counts": dict(sorted(confidence_counts.items())),
            "low_confidence_event_count": confidence_counts.get("LOW", 0),
        },
    }


def make_f4_report(rows: list[dict[str, Any]]) -> dict[str, Any]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[row["fingerprints"]["F4"]].append(row)

    classes: list[dict[str, Any]] = []
    duplicate_groups: list[dict[str, Any]] = []
    for index, digest in enumerate(sorted(grouped), start=1):
        members = sorted(grouped[digest], key=lambda item: item["source_id"])
        families = sorted({item["source_family"] for item in members})
        group = {
            "class_id": f"F4:C{index:03d}",
            "fingerprint_sha256": digest,
            "size": len(members),
            "source_ids": [item["source_id"] for item in members],
            "source_families": families,
            "cross_family": len(families) > 1,
        }
        classes.append(group)
        if len(members) > 1:
            duplicate_groups.append(group)

    return {
        "status": "COMPUTED",
        "name": "FunctionalClassSequence",
        "definition_count": len(rows),
        "unique_class_count": len(classes),
        "duplicate_group_count": len(duplicate_groups),
        "definitions_in_duplicate_groups": sum(
            group["size"] for group in duplicate_groups
        ),
        "duplicate_surplus_count": sum(
            group["size"] - 1 for group in duplicate_groups
        ),
        "largest_class_size": max(
            (group["size"] for group in classes), default=0
        ),
        "cross_family_duplicate_group_count": sum(
            group["cross_family"] for group in duplicate_groups
        ),
        "classes": classes,
        "duplicate_groups": duplicate_groups,
    }


def distribution(rows: list[dict[str, Any]], field: str) -> dict[str, int]:
    counts = Counter(row["features"][field] for row in rows)
    return dict(sorted(counts.items(), key=lambda item: str(item[0])))


ROMAN_BY_DEGREE = {
    0: "I",
    1: "II",
    2: "III",
    3: "IV",
    4: "V",
    5: "VI",
    6: "VII",
}


def altered_degree_label(degree: int, alteration: int) -> str:
    accidental = {-1: "b", 0: "", 1: "#"}.get(alteration)
    if accidental is None or degree not in ROMAN_BY_DEGREE:
        return f"degree={degree},alteration={alteration:+d}"
    return f"{accidental}{ROMAN_BY_DEGREE[degree]}"


def f4_abstraction_report(rows: list[dict[str, Any]]) -> dict[str, Any]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[row["fingerprints"]["F4"]].append(row)

    overcollapsed: list[dict[str, Any]] = []
    for digest, members in sorted(grouped.items()):
        f3_ids = sorted({member["fingerprints"]["F3"] for member in members})
        if len(f3_ids) <= 1:
            continue
        overcollapsed.append(
            {
                "f4_fingerprint_sha256": digest,
                "member_count": len(members),
                "distinct_f3_class_count": len(f3_ids),
                "source_ids": sorted(member["source_id"] for member in members),
                "meaning": (
                    "F4 intentionally generalizes multiple F3 harmonic identities. "
                    "This group is functional equivalence only and cannot authorize dedup."
                ),
            }
        )

    return {
        "f4_groups_spanning_multiple_f3_classes": len(overcollapsed),
        "definitions_in_f4_overcollapse_groups": sum(
            row["member_count"] for row in overcollapsed
        ),
        "largest_distinct_f3_count_inside_one_f4": max(
            (row["distinct_f3_class_count"] for row in overcollapsed),
            default=0,
        ),
        "groups": overcollapsed,
        "dedup_authority": "FORBIDDEN",
    }


def build_reports(rows: list[dict[str, Any]]) -> dict[str, Any]:
    role_counts: Counter[str] = Counter()
    motion_counts: Counter[str] = Counter()
    cadence_tag_counts: Counter[str] = Counter()
    family_counts: dict[str, Counter[str]] = defaultdict(Counter)

    chromatic_definitions: list[str] = []
    borrowed_definitions: list[str] = []
    uncertain_definitions: list[str] = []
    chromatic_by_family: Counter[str] = Counter()
    altered_degree_classes: Counter[str] = Counter()
    borrowed_by_family: Counter[str] = Counter()
    quality_unique_distribution: Counter[int] = Counter()
    entropy_values: list[float] = []

    for row in rows:
        features = row["features"]
        role_counts.update(features["functional_role_counts"])
        motion_counts.update(features["functional_motion_histogram"])
        cadence_tag_counts[features["cadence_tag_comparison"]] += 1
        family_counts[row["source_family"]][features["closure_class"]] += 1

        if features["chromatic_root_count"] > 0:
            chromatic_definitions.append(row["source_id"])
        if features["borrowed_candidate_count"] > 0:
            borrowed_definitions.append(row["source_id"])

        for event in row["functional_events"]:
            if event["kind"] != "CHORD":
                continue
            if event["chromatic_root"]:
                chromatic_by_family[row["source_family"]] += 1
                altered_degree_classes[
                    altered_degree_label(
                        int(event["root"]["diatonic_degree"]),
                        int(event["root"]["alteration_semitones"]),
                    )
                ] += 1
            if event["borrowed_candidate"]:
                borrowed_by_family[row["source_family"]] += 1

        if (
            features["low_confidence_event_count"] > 0
            or features["cadence_class"] in ("UNKNOWN", "MODAL_AMBIGUOUS")
        ):
            uncertain_definitions.append(row["source_id"])

        quality_signatures = {
            tuple(event["quality_signature"])
            for event in row["functional_events"]
            if event["kind"] == "CHORD"
        }
        quality_unique_distribution[len(quality_signatures)] += 1
        entropy_values.append(float(features["quality_entropy_bits"]))

    entropy_mean = (
        round(sum(entropy_values) / len(entropy_values), 6)
        if entropy_values
        else 0.0
    )

    return {
        "closure_class_distribution": distribution(rows, "closure_class"),
        "closure_class_by_family": {
            family: dict(sorted(counts.items()))
            for family, counts in sorted(family_counts.items())
        },
        "cadence_class_distribution": distribution(rows, "cadence_class"),
        "cadence_tag_comparison_distribution": dict(
            sorted(cadence_tag_counts.items())
        ),
        "chromatic_report": {
            "definition_count": len(chromatic_definitions),
            "source_ids": sorted(chromatic_definitions),
            "total_chromatic_root_events": sum(
                row["features"]["chromatic_root_count"] for row in rows
            ),
            "event_count_by_family": dict(sorted(chromatic_by_family.items())),
            "altered_degree_class_counts": dict(
                sorted(altered_degree_classes.items())
            ),
        },
        "borrowed_candidate_report": {
            "definition_count": len(borrowed_definitions),
            "source_ids": sorted(borrowed_definitions),
            "total_borrowed_candidate_events": sum(
                row["features"]["borrowed_candidate_count"] for row in rows
            ),
            "event_count_by_family": dict(sorted(borrowed_by_family.items())),
            "claim_level": "CANDIDATE_ONLY_NOT_CONFIRMED_BORROWING",
        },
        "quality_diversity_report": {
            "unique_quality_count_distribution": {
                str(key): value
                for key, value in sorted(quality_unique_distribution.items())
            },
            "quality_entropy_bits_mean": entropy_mean,
            "quality_entropy_bits_min": min(entropy_values) if entropy_values else 0.0,
            "quality_entropy_bits_max": max(entropy_values) if entropy_values else 0.0,
        },
        "functional_motion_report": {
            "role_event_counts": dict(sorted(role_counts.items())),
            "motion_histogram": dict(sorted(motion_counts.items())),
            "dominant_resolution_total": sum(
                row["features"]["dominant_resolution_count"] for row in rows
            ),
        },
        "uncertain_unknown_report": {
            "definition_count": len(uncertain_definitions),
            "source_ids": sorted(uncertain_definitions),
            "policy": (
                "LOW-confidence modal/common-practice ambiguity and UNKNOWN cadence "
                "remain explicit; H3 does not force a stronger label."
            ),
        },
    }


def build_functional_analysis(
    normalization: dict[str, Any],
    fingerprints: dict[str, Any],
    *,
    h1_digest: str,
    h2_digest: str,
) -> dict[str, Any]:
    require_dependencies(normalization, fingerprints)
    stability = recompute_and_verify_f0_f3(
        normalization, fingerprints, h1_digest=h1_digest
    )

    tokens = h2.token_lookup(normalization)
    h2_by_source = {
        row["source_id"]: row for row in fingerprints["definitions"]
    }
    rows: list[dict[str, Any]] = []
    for definition in normalization["definitions"]:
        source_id = definition["source_id"]
        if source_id not in h2_by_source:
            raise FunctionalAnalysisError(
                f"H2 missing definition {source_id}"
            )
        events = h2.materialize_events(definition, tokens)
        rows.append(
            analyze_definition(
                definition,
                events,
                h2_by_source[source_id],
            )
        )
    rows.sort(key=lambda row: row["source_id"])

    f4 = make_f4_report(rows)
    reports = build_reports(rows)

    return {
        "schema_version": SCHEMA_VERSION,
        "stage": STAGE,
        "source": normalization["source"],
        "dependencies": {
            "h1_normalized_json_sha256": h1_digest,
            "expected_h1_normalized_json_sha256": EXPECTED_H1_NORMALIZED_SHA256,
            "h2_fingerprints_json_sha256": h2_digest,
            "expected_h2_fingerprints_json_sha256": EXPECTED_H2_FINGERPRINTS_SHA256,
            "f0_f3_stability": stability,
        },
        "analysis_contract": {
            "derived_labels_rewrite_source_identity": False,
            "f0_f3_rewrite": "FORBIDDEN",
            "modal_common_practice_function_forced": False,
            "borrowed_label_strength": "CANDIDATE_ONLY",
            "f4_identity_level": "HIGH_LEVEL_FUNCTIONAL_EQUIVALENCE",
            "f4_destructive_dedup_authority": "FORBIDDEN",
            "absolute_midi_projection": "FORBIDDEN",
            "runtime_admission": "NOT_PERFORMED",
            "source_incidence_to_runtime_weight": "FORBIDDEN",
            "harmonic_event_density_timing_claim": "DEFERRED_TO_H4",
            "next_stage": "H4_CHORD_RHYTHM_EXTRACTION",
        },
        "functional_taxonomy": {
            "roles": list(FUNCTIONAL_ROLES),
            "color_classes": list(COLOR_CLASSES),
            "confidence_levels": list(CONFIDENCE_LEVELS),
            "cadence_classes": list(CADENCE_CLASSES),
            "closure_classes": list(CLOSURE_CLASSES),
            "major_minor_role_policy": (
                "Unaltered I=TONIC, II/IV=PREDOMINANT, V=DOMINANT; "
                "other unaltered roots=UNALTERED_OTHER; altered roots=CHROMATIC_COLOR."
            ),
            "modal_role_policy": (
                "Unaltered tonic=TONIC; every other modal event=MODAL_COLOR. "
                "Common-practice predominant/dominant function is not forced."
            ),
            "borrowed_candidate_policy": {
                "Major": [
                    "bIII/bVI/bVII root candidates",
                    "minor iv candidate",
                    "minor i candidate",
                ],
                "Minor": [
                    "major IV candidate",
                    "major I candidate",
                ],
                "Modal": ["not asserted"],
            },
        },
        "f4": f4,
        "f4_abstraction_report": f4_abstraction_report(rows),
        "reports": reports,
        "definitions": rows,
    }


def render_markdown(result: dict[str, Any]) -> str:
    reports = result["reports"]
    f4 = result["f4"]

    def table_rows(mapping: dict[str, int]) -> list[str]:
        return [f"| `{key}` | {value} |" for key, value in mapping.items()]

    lines = [
        "# Harmony Atlas H3 Functional Analysis",
        "",
        "**Status:** generated research evidence / H3 checkpoint  ",
        f"**Source:** `{result['source']['repository']} @ {result['source']['commit']}`  ",
        f"**Evidence class:** `{result['source']['evidence_class']}`  ",
        "**Runtime impact:** none",
        "",
        "## Dependency / identity boundary",
        "",
        f"H1 JSON SHA-256: `{result['dependencies']['expected_h1_normalized_json_sha256']}`.",
        "",
        f"H2 JSON SHA-256: `{result['dependencies']['expected_h2_fingerprints_json_sha256']}`.",
        "",
        "H3 recomputes and verifies every F0-F3 fingerprint before deriving functional evidence. H3 cannot rewrite normalized source identity.",
        "",
        "## F4 FunctionalClassSequence",
        "",
        "| Item | Count |",
        "|---|---:|",
        f"| Definitions | {f4['definition_count']} |",
        f"| Unique F4 classes | {f4['unique_class_count']} |",
        f"| Duplicate F4 groups | {f4['duplicate_group_count']} |",
        f"| Surplus F4 duplicates | {f4['duplicate_surplus_count']} |",
        f"| Largest F4 class | {f4['largest_class_size']} |",
        f"| Cross-family F4 duplicate groups | {f4['cross_family_duplicate_group_count']} |",
        "",
        "F4 fingerprints only the ordered **functional role + color class** sequence. Reason codes and confidence remain diagnostics and do not silently redefine identity.",
        "",
        f"F4 groups spanning multiple distinct F3 identities: **{result['f4_abstraction_report']['f4_groups_spanning_multiple_f3_classes']}**; largest such F4 class contains **{result['f4_abstraction_report']['largest_distinct_f3_count_inside_one_f4']}** distinct F3 identities.",
        "",
        "**F4 is an analytic abstraction, not a destructive-dedup authority.**",
        "",
        "## Functional classification boundary",
        "",
        "```text",
        "Major / Minor",
        "  I      -> TONIC",
        "  II, IV -> PREDOMINANT",
        "  V      -> DOMINANT",
        "  other unaltered roots -> UNALTERED_OTHER",
        "  altered roots         -> CHROMATIC_COLOR",
        "",
        "Modal",
        "  tonic -> TONIC",
        "  every other event -> MODAL_COLOR",
        "```",
        "",
        "Modal harmony is deliberately not forced into common-practice tonic/predominant/dominant roles.",
        "",
        "Borrowing is reported only as `BORROWED_CANDIDATE`; H3 does not claim historical/compositional borrowing as fact.",
        "",
        "## Closure-class distribution",
        "",
        "| Class | Definitions |",
        "|---|---:|",
        *table_rows(reports["closure_class_distribution"]),
        "",
        "## Cadence-class distribution",
        "",
        "| Class | Definitions |",
        "|---|---:|",
        *table_rows(reports["cadence_class_distribution"]),
        "",
        "Source `Cadence` metadata and derived cadence candidates are kept separate:",
        "",
        "| Comparison | Definitions |",
        "|---|---:|",
        *table_rows(reports["cadence_tag_comparison_distribution"]),
        "",
        "## Chromatic / borrowed candidate report",
        "",
        f"Definitions with altered roots: **{reports['chromatic_report']['definition_count']}**; altered-root events: **{reports['chromatic_report']['total_chromatic_root_events']}**.",
        "",
        "Altered degree classes: " + ", ".join(
            f"`{degree}`={count}"
            for degree, count in reports["chromatic_report"]["altered_degree_class_counts"].items()
        ) + ".",
        "",
        "Altered-root events by source family: " + ", ".join(
            f"`{family}`={count}"
            for family, count in reports["chromatic_report"]["event_count_by_family"].items()
        ) + ".",
        "",
        f"Definitions with conservative borrowed candidates: **{reports['borrowed_candidate_report']['definition_count']}**; candidate events: **{reports['borrowed_candidate_report']['total_borrowed_candidate_events']}**.",
        "",
        "Borrowed counts are candidate evidence only, not a measured statement about compositional origin.",
        "",
        "## Quality-diversity report",
        "",
        f"Mean semantic quality entropy: **{reports['quality_diversity_report']['quality_entropy_bits_mean']} bits**.",
        "",
        f"Range: **{reports['quality_diversity_report']['quality_entropy_bits_min']} .. {reports['quality_diversity_report']['quality_entropy_bits_max']} bits**.",
        "",
        "Unique-quality-count distribution:",
        "",
        "| Unique semantic qualities | Definitions |",
        "|---|---:|",
    ]
    for count, definitions in reports["quality_diversity_report"]["unique_quality_count_distribution"].items():
        lines.append(f"| {count} | {definitions} |")

    lines += [
        "",
        "## Functional-motion report",
        "",
        f"Adjacent DOMINANT→TONIC resolutions: **{reports['functional_motion_report']['dominant_resolution_total']}**.",
        "",
        "Functional role event counts:",
        "",
        "| Role | Events |",
        "|---|---:|",
        *table_rows(reports["functional_motion_report"]["role_event_counts"]),
        "",
        "Top functional motions:",
        "",
    ]
    motions = sorted(
        reports["functional_motion_report"]["motion_histogram"].items(),
        key=lambda item: (-item[1], item[0]),
    )
    for motion, count in motions[:20]:
        lines.append(f"- `{motion}` — {count}")

    uncertainty = reports["uncertain_unknown_report"]
    lines += [
        "",
        "## Uncertain / unknown report",
        "",
        f"Definitions carrying LOW-confidence common-practice ambiguity or UNKNOWN cadence: **{uncertainty['definition_count']}**.",
        "",
        "This is intentional. Modal/common-practice ambiguity stays visible rather than being forced into a stronger function class.",
        "",
        "## H3 contract",
        "",
        "- F0-F3 are verified unchanged before any functional derivation;",
        "- F4 is now COMPUTED from functional role + color class;",
        "- F4 is high-level analytic equivalence and has no destructive-dedup authority;",
        "- F5 remains deferred to H4 ChordRhythm extraction;",
        "- F6 remains deferred until F5 exists;",
        "- cadence and closure classes are deterministic heuristics with reason/confidence;",
        "- source `Cadence` tags do not overwrite derived cadence classes;",
        "- borrowed labels are candidates only;",
        "- no absolute MIDI projection is performed;",
        "- no runtime candidate is selected;",
        "- no source-incidence ratio becomes runtime weight;",
        "- timing-based harmonic density remains deferred to H4.",
        "",
        "Next stage: **H4 ChordRhythm extraction**. H4 may make F5 computable; H3 must not be retroactively rewritten to fit rhythm results.",
        "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Derive Harmony Atlas H3 functional/cadence/color evidence and compute F4"
    )
    parser.add_argument("--normalization", type=Path, required=True)
    parser.add_argument("--fingerprints", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--markdown-output", type=Path)
    args = parser.parse_args()

    normalization, h1_digest = load_verified_json(
        args.normalization,
        EXPECTED_H1_NORMALIZED_SHA256,
        "H1 normalized JSON",
    )
    fingerprints, h2_digest = load_verified_json(
        args.fingerprints,
        EXPECTED_H2_FINGERPRINTS_SHA256,
        "H2 fingerprints JSON",
    )
    result = build_functional_analysis(
        normalization,
        fingerprints,
        h1_digest=h1_digest,
        h2_digest=h2_digest,
    )
    json_text = json.dumps(result, indent=2, sort_keys=True) + "\n"
    markdown_text = render_markdown(result)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json_text, encoding="utf-8")
    else:
        print(json_text, end="")
    if args.markdown_output:
        args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_output.write_text(markdown_text, encoding="utf-8")


if __name__ == "__main__":
    main()
