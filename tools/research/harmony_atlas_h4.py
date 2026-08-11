#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import hashlib
import json
import re
import sys
from collections import Counter, defaultdict
from decimal import Decimal
from fractions import Fraction
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import harmony_atlas_h0 as h0
import harmony_atlas_h2 as h2
import harmony_atlas_h3 as h3

SCHEMA_VERSION = "1.0.0"
STAGE = "H4_CHORD_RHYTHM_EXTRACTION"
EXPECTED_H1_NORMALIZED_SHA256 = "4783be1f77784a978f2239d8527214b3912eaf262e92430bff755679fec0734e"
EXPECTED_H2_FINGERPRINTS_SHA256 = "453287577707e676128f3f83d1215922e22049ab0d880492f95af20ea7e68b0b"
EXPECTED_H3_FUNCTIONAL_SHA256 = "f15d127722691789f6c1d1a003028da755e06a1cba1d2c1014c1232697cc456d"
EXPECTED_RHYTHM_SOURCE_BLOBS = {
    "gen.py": "55ff32d43f8c3754b807530e3a49240f8c5174b4",
    "src/chords2midi/c2mpatterns.py": "0e7c9a39032760650574a72961b162f4295fcee0",
    "src/chords2midi/c2m.py": "04c5cc8decf729ac564dc847518db431b3c73a88",
}
PATTERN_TOKEN_RE = re.compile(r"^([0-9]*\.?[0-9]+)?([NSX])$")


class RhythmExtractionError(RuntimeError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_json(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def namespaced_sha256(namespace: str, payload: Any) -> str:
    return hashlib.sha256((namespace + "\0" + canonical_json(payload)).encode()).hexdigest()


def verify_json_bytes(data: bytes, expected: str, label: str) -> str:
    digest = sha256_bytes(data)
    if digest != expected:
        raise RhythmExtractionError(
            f"{label} digest mismatch: expected {expected}, got {digest}"
        )
    return digest


def load_verified_json(path: Path, expected: str, label: str) -> tuple[dict[str, Any], str]:
    data = path.read_bytes()
    digest = verify_json_bytes(data, expected, label)
    try:
        value = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise RhythmExtractionError(f"{label} is not valid UTF-8 JSON: {exc}") from exc
    if not isinstance(value, dict):
        raise RhythmExtractionError(f"{label} must contain a JSON object")
    return value, digest


def verify_rhythm_source_files(source_root: Path) -> dict[str, str]:
    observed: dict[str, str] = {}
    for relative, expected in EXPECTED_RHYTHM_SOURCE_BLOBS.items():
        path = source_root / relative
        if not path.is_file():
            raise RhythmExtractionError(f"missing pinned rhythm source file: {relative}")
        digest = h0.git_blob_sha1(path.read_bytes())
        observed[relative] = digest
        if digest != expected:
            raise RhythmExtractionError(
                f"pinned rhythm source mismatch for {relative}: expected {expected}, got {digest}"
            )
    return observed


def _safe_eval(node: ast.AST, env: dict[str, Any]) -> Any:
    if isinstance(node, ast.Constant) and isinstance(node.value, (str, int, float)):
        return node.value
    if isinstance(node, ast.List):
        return [_safe_eval(item, env) for item in node.elts]
    if isinstance(node, ast.Tuple):
        return tuple(_safe_eval(item, env) for item in node.elts)
    if isinstance(node, ast.Dict):
        return {
            _safe_eval(key, env): _safe_eval(value, env)
            for key, value in zip(node.keys, node.values)
        }
    if isinstance(node, ast.Name) and node.id in env:
        return env[node.id]
    raise RhythmExtractionError(
        "unsupported non-literal rhythm-source expression: "
        + ast.dump(node, include_attributes=False)
    )


def safe_source_assignments(path: Path) -> dict[str, Any]:
    try:
        tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    except (OSError, SyntaxError) as exc:
        raise RhythmExtractionError(f"cannot parse {path}: {exc}") from exc
    env: dict[str, Any] = {}
    for node in tree.body:
        if not isinstance(node, ast.Assign) or len(node.targets) != 1:
            continue
        target = node.targets[0]
        if not isinstance(target, ast.Name):
            continue
        try:
            env[target.id] = _safe_eval(node.value, env)
        except RhythmExtractionError:
            # Dynamic source code remains unavailable. H4 never executes upstream code.
            continue
    return env


def require_patterns(assignments: dict[str, Any]) -> dict[str, list[str]]:
    value = assignments.get("patterns")
    if not isinstance(value, dict):
        raise RhythmExtractionError("renderer patterns must be a literal dict")
    result: dict[str, list[str]] = {}
    for name, tokens in value.items():
        if not isinstance(name, str) or not isinstance(tokens, list):
            raise RhythmExtractionError(f"invalid pattern entry {name!r}")
        if not all(isinstance(token, str) for token in tokens):
            raise RhythmExtractionError(f"pattern {name!r} must contain strings only")
        result[name] = list(tokens)
    return result


def parse_pattern_token(token: str) -> tuple[str, Fraction]:
    match = PATTERN_TOKEN_RE.fullmatch(token)
    if match is None:
        raise RhythmExtractionError(f"unsupported pattern token {token!r}")
    duration_text, instruction = match.groups()
    duration = Fraction(Decimal(duration_text)) if duration_text else Fraction(1)
    if duration <= 0:
        raise RhythmExtractionError(f"pattern duration must be positive: {token!r}")
    return instruction, duration


def fraction_payload(value: Fraction) -> dict[str, int]:
    return {"numerator": value.numerator, "denominator": value.denominator}


def fraction_from_payload(value: dict[str, int]) -> Fraction:
    return Fraction(value["numerator"], value["denominator"])


def fraction_text(value: Fraction) -> str:
    return str(value.numerator) if value.denominator == 1 else f"{value.numerator}/{value.denominator}"


def source_style_name(raw_style: str) -> str:
    return "default" if raw_style == "" else raw_style


def resolve_pattern(raw_style: str, event_refs: list[str]) -> tuple[str, Fraction]:
    if raw_style != "":
        return raw_style, Fraction(1)
    if "REST" in event_refs:
        return "basic", Fraction(2)
    return "long", Fraction(1)


def _segment_kind(instruction: str, current_ref: str | None) -> str:
    if instruction == "X":
        return "REST_PATTERN"
    if current_ref == "REST":
        return "REST_SOURCE"
    if instruction == "N":
        return "CHORD_ONSET"
    if instruction == "S":
        return "CHORD_RETRIGGER"
    raise RhythmExtractionError(f"unsupported instruction {instruction!r}")


def expand_pattern(
    event_refs: list[str],
    pattern_name: str,
    pattern_tokens: list[str],
    *,
    base_duration: Fraction,
) -> dict[str, Any]:
    if not event_refs:
        raise RhythmExtractionError("cannot materialize an empty progression")
    parsed = [parse_pattern_token(token) for token in pattern_tokens]
    if not parsed:
        raise RhythmExtractionError(f"pattern {pattern_name!r} is empty")

    input_index = 0
    mask_index = 0
    current_ref: str | None = None
    current_source_index: int | None = None
    raw_segments: list[dict[str, Any]] = []
    guard = 0
    while True:
        guard += 1
        if guard > 100000:
            raise RhythmExtractionError(f"pattern {pattern_name!r} did not terminate")
        instruction, multiplier = parsed[mask_index]
        if instruction == "N":
            if input_index >= len(event_refs):
                break
            current_ref = event_refs[input_index]
            current_source_index = input_index
            input_index += 1
        elif instruction == "S":
            if current_ref is None:
                raise RhythmExtractionError(f"pattern {pattern_name!r} uses S before N")
        elif instruction != "X":
            raise RhythmExtractionError(f"unknown pattern instruction {instruction!r}")

        raw_segments.append(
            {
                "instruction": instruction,
                "kind": _segment_kind(instruction, current_ref),
                "duration": base_duration * multiplier,
                "source_event_index": current_source_index if instruction != "X" else None,
                "source_ref": current_ref if instruction != "X" else None,
            }
        )
        mask_index = (mask_index + 1) % len(parsed)

    if input_index != len(event_refs):
        raise RhythmExtractionError(f"pattern {pattern_name!r} did not consume all source events")

    # Pinned c2m.py doubles the final long-pattern step for odd source-event counts.
    if pattern_name == "long" and len(event_refs) % 2 == 1 and raw_segments:
        raw_segments[-1]["duration"] *= 2

    cursor = Fraction(0)
    segments: list[dict[str, Any]] = []
    for index, raw in enumerate(raw_segments):
        kind = raw["kind"]
        duration = raw["duration"]
        note_onset = kind in ("CHORD_ONSET", "CHORD_RETRIGGER")
        segments.append(
            {
                "segment_index": index,
                "instruction": raw["instruction"],
                "kind": kind,
                "start_beats": fraction_payload(cursor),
                "duration_beats": fraction_payload(duration),
                "source_event_index": raw["source_event_index"],
                "source_ref": raw["source_ref"],
                "note_onset": note_onset,
                "source_advance": raw["instruction"] == "N",
                "same_chord_retrigger": kind == "CHORD_RETRIGGER",
                "rest": kind.startswith("REST_"),
                # S is rendered with a new addNote call; it is not a tie/hold continuation.
                "continuation": False,
            }
        )
        cursor += duration
    return {
        "segments": segments,
        "phrase_length_beats": fraction_payload(cursor),
        "phrase_length_fraction": cursor,
    }


def f5_segment(segment: dict[str, Any]) -> dict[str, Any]:
    # Rest provenance is diagnostic, not rhythmic identity.
    kind = "REST" if segment["rest"] else segment["kind"]
    return {"kind": kind, "duration_beats": segment["duration_beats"]}


def f5_payload(segments: list[dict[str, Any]]) -> dict[str, Any]:
    return {"segments": [f5_segment(segment) for segment in segments]}


def f6_payload(f3_sha256: str, f5_sha256: str) -> dict[str, str]:
    return {"F3": f3_sha256, "F5": f5_sha256}


def make_observation(
    definition: dict[str, Any],
    h2_row: dict[str, Any],
    h3_row: dict[str, Any],
    raw_style: str,
    patterns: dict[str, list[str]],
) -> dict[str, Any]:
    event_refs = definition["event_refs"]
    pattern_name, base_duration = resolve_pattern(raw_style, event_refs)
    if pattern_name not in patterns:
        raise RhythmExtractionError(
            f"style {source_style_name(raw_style)!r} requires missing pattern {pattern_name!r}"
        )
    expanded = expand_pattern(
        event_refs, pattern_name, patterns[pattern_name], base_duration=base_duration
    )
    segments = expanded["segments"]
    f5 = namespaced_sha256("F5", f5_payload(segments))
    f3 = h2_row["fingerprints"]["F3"]
    f6 = namespaced_sha256("F6", f6_payload(f3, f5))
    style = source_style_name(raw_style)
    return {
        "observation_id": f"{definition['source_id']}@{style}",
        "source_id": definition["source_id"],
        "source_family": definition["source_family"],
        "source_style": style,
        "source_style_raw": raw_style,
        "pattern_name": pattern_name,
        "source_event_count": len(event_refs),
        "source_rest_event_count": sum(ref == "REST" for ref in event_refs),
        "segment_count": len(segments),
        "note_onset_count": sum(segment["note_onset"] for segment in segments),
        "same_chord_retrigger_count": sum(segment["same_chord_retrigger"] for segment in segments),
        "rest_segment_count": sum(segment["rest"] for segment in segments),
        "pattern_rest_segment_count": sum(segment["kind"] == "REST_PATTERN" for segment in segments),
        "source_rest_segment_count": sum(segment["kind"] == "REST_SOURCE" for segment in segments),
        "continuation_segment_count": 0,
        "phrase_length_beats": expanded["phrase_length_beats"],
        "fingerprints": {
            "F3": f3,
            "F4": h3_row["fingerprints"]["F4"],
            "F5": f5,
            "F6": f6,
        },
        "segments": segments,
    }


def verify_f0_f4_stability(
    normalization: dict[str, Any],
    fingerprints: dict[str, Any],
    functional: dict[str, Any],
    *,
    h1_digest: str,
    h2_digest: str,
) -> dict[str, Any]:
    rebuilt_h2 = h2.build_fingerprints(normalization, h1_input_sha256=h1_digest)
    expected_h2 = {row["source_id"]: row["fingerprints"] for row in fingerprints["definitions"]}
    actual_h2 = {row["source_id"]: row["fingerprints"] for row in rebuilt_h2["definitions"]}
    if expected_h2 != actual_h2:
        raise RhythmExtractionError("H4 detected F0-F3 drift relative to frozen H2")

    rebuilt_h3 = h3.build_functional_analysis(
        normalization, fingerprints, h1_digest=h1_digest, h2_digest=h2_digest
    )
    expected_h3 = {row["source_id"]: row["fingerprints"] for row in functional["definitions"]}
    actual_h3 = {row["source_id"]: row["fingerprints"] for row in rebuilt_h3["definitions"]}
    if expected_h3 != actual_h3:
        raise RhythmExtractionError("H4 detected F0-F4 drift relative to frozen H3")
    return {
        "verified": True,
        "definition_count": len(actual_h3),
        "frozen_levels": ["F0", "F1", "F2", "F3", "F4"],
    }


def rhythm_shape(observation: dict[str, Any]) -> dict[str, Any]:
    segments = observation["segments"]
    return {
        "f5_fingerprint_sha256": observation["fingerprints"]["F5"],
        "segment_count": len(segments),
        "phrase_length_beats": observation["phrase_length_beats"],
        "segment_durations_beats": [segment["duration_beats"] for segment in segments],
        "note_onset_coordinates_beats": [
            segment["start_beats"] for segment in segments if segment["note_onset"]
        ],
        "rest_mask": [segment["rest"] for segment in segments],
        "continuation_mask": [segment["continuation"] for segment in segments],
        "same_chord_retrigger_mask": [segment["same_chord_retrigger"] for segment in segments],
        "source_advance_mask": [segment["source_advance"] for segment in segments],
        "canonical_f5_segments": [f5_segment(segment) for segment in segments],
    }


def compact_observation(observation: dict[str, Any]) -> dict[str, Any]:
    return {key: value for key, value in observation.items() if key != "segments"}


def class_report(observations: list[dict[str, Any]], level: str) -> dict[str, Any]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for observation in observations:
        grouped[observation["fingerprints"][level]].append(observation)
    classes, duplicates = [], []
    for index, digest in enumerate(sorted(grouped), start=1):
        members = sorted(grouped[digest], key=lambda row: row["observation_id"])
        styles = sorted({row["source_style"] for row in members})
        source_ids = sorted({row["source_id"] for row in members})
        group = {
            "class_id": f"{level}:C{index:03d}",
            "fingerprint_sha256": digest,
            "observation_count": len(members),
            "logical_source_count": len(source_ids),
            "source_styles": styles,
            "cross_style": len(styles) > 1,
            "source_ids": source_ids,
            "observation_ids": [row["observation_id"] for row in members],
        }
        classes.append(group)
        if len(members) > 1:
            duplicates.append(group)
    return {
        "status": "COMPUTED_H4",
        "definition_observation_count": len(observations),
        "unique_class_count": len(classes),
        "duplicate_group_count": len(duplicates),
        "duplicate_surplus_count": sum(group["observation_count"] - 1 for group in duplicates),
        "largest_class_size": max((group["observation_count"] for group in classes), default=0),
        "cross_style_duplicate_group_count": sum(group["cross_style"] for group in duplicates),
        "classes": classes,
        "duplicate_groups": duplicates,
    }


def style_report(observations: list[dict[str, Any]]) -> dict[str, Any]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for observation in observations:
        grouped[observation["source_style"]].append(observation)
    result: dict[str, Any] = {}
    for style, rows in sorted(grouped.items()):
        lengths = Counter(
            fraction_text(fraction_from_payload(row["phrase_length_beats"])) for row in rows
        )
        segment_counts = Counter(row["segment_count"] for row in rows)
        result[style] = {
            "observation_count": len(rows),
            "unique_f5_count": len({row["fingerprints"]["F5"] for row in rows}),
            "pattern_names": sorted({row["pattern_name"] for row in rows}),
            "segment_count_distribution": {str(k): v for k, v in sorted(segment_counts.items())},
            "phrase_length_distribution_beats": dict(sorted(lengths.items())),
            "note_onset_count": sum(row["note_onset_count"] for row in rows),
            "same_chord_retrigger_count": sum(row["same_chord_retrigger_count"] for row in rows),
            "rest_segment_count": sum(row["rest_segment_count"] for row in rows),
            "continuation_segment_count": 0,
        }
    return result


def harmonic_rhythm_cross_table(observations: list[dict[str, Any]]) -> dict[str, Any]:
    by_f3: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for observation in observations:
        by_f3[observation["fingerprints"]["F3"]].append(observation)
    rows, distribution = [], Counter()
    for f3, members in sorted(by_f3.items()):
        source_ids = sorted({row["source_id"] for row in members})
        f5_ids = sorted({row["fingerprints"]["F5"] for row in members})
        distribution[len(f5_ids)] += 1
        rows.append(
            {
                "f3_fingerprint_sha256": f3,
                "logical_source_count": len(source_ids),
                "rhythm_observation_count": len(members),
                "unique_f5_count": len(f5_ids),
                "source_ids": source_ids,
                "source_styles": sorted({row["source_style"] for row in members}),
            }
        )
    return {
        "f3_class_count": len(rows),
        "logical_source_count": len({row["source_id"] for row in observations}),
        "rhythm_observation_count": len(observations),
        "unique_f5_count_per_f3_distribution": {str(k): v for k, v in sorted(distribution.items())},
        "rows": rows,
    }


def build_h4(
    source_root: Path,
    normalization: dict[str, Any],
    fingerprints: dict[str, Any],
    functional: dict[str, Any],
    *,
    h1_digest: str,
    h2_digest: str,
    h3_digest: str,
) -> dict[str, Any]:
    if normalization.get("stage") != "H1_CANONICAL_PARSER_NORMALIZATION":
        raise RhythmExtractionError("H4 requires H1 normalization")
    if fingerprints.get("stage") != "H2_STRUCTURAL_FINGERPRINTS_DEDUP":
        raise RhythmExtractionError("H4 requires H2 fingerprints")
    if functional.get("stage") != "H3_FUNCTIONAL_ANALYSIS":
        raise RhythmExtractionError("H4 requires H3 functional evidence")
    if normalization.get("source", {}).get("commit") != h0.PINNED_SOURCE_COMMIT:
        raise RhythmExtractionError("H4 source commit is not the pinned Harmony Atlas revision")

    blob_sha1 = verify_rhythm_source_files(source_root)
    stability = verify_f0_f4_stability(
        normalization, fingerprints, functional, h1_digest=h1_digest, h2_digest=h2_digest
    )
    gen_assignments = h0.literal_assignments(source_root / "gen.py")
    raw_styles = h0.require_string_list(gen_assignments, "styles")
    key_pairs = h0.require_key_pairs(gen_assignments)
    if len(raw_styles) != len(set(raw_styles)):
        raise RhythmExtractionError("generator source styles contain duplicates")
    patterns = require_patterns(
        safe_source_assignments(source_root / "src/chords2midi/c2mpatterns.py")
    )

    h2_by_source = {row["source_id"]: row for row in fingerprints["definitions"]}
    h3_by_source = {row["source_id"]: row for row in functional["definitions"]}
    full_observations: list[dict[str, Any]] = []
    for definition in normalization["definitions"]:
        source_id = definition["source_id"]
        if source_id not in h2_by_source or source_id not in h3_by_source:
            raise RhythmExtractionError(f"missing dependency row for {source_id}")
        for raw_style in raw_styles:
            full_observations.append(
                make_observation(
                    definition, h2_by_source[source_id], h3_by_source[source_id], raw_style, patterns
                )
            )
    full_observations.sort(key=lambda row: row["observation_id"])

    shapes_by_f5: dict[str, dict[str, Any]] = {}
    for observation in full_observations:
        shape = rhythm_shape(observation)
        f5 = observation["fingerprints"]["F5"]
        previous = shapes_by_f5.get(f5)
        if previous is not None and previous != shape:
            raise RhythmExtractionError("same F5 fingerprint produced inconsistent rhythm shapes")
        shapes_by_f5[f5] = shape
    rhythm_shapes = [
        {"shape_id": f"R{index:03d}", **shapes_by_f5[digest]}
        for index, digest in enumerate(sorted(shapes_by_f5), start=1)
    ]
    shape_id_by_f5 = {row["f5_fingerprint_sha256"]: row["shape_id"] for row in rhythm_shapes}

    observations = []
    for row in full_observations:
        compact = compact_observation(row)
        compact["rhythm_shape_id"] = shape_id_by_f5[row["fingerprints"]["F5"]]
        observations.append(compact)

    f5 = class_report(observations, "F5")
    f6 = class_report(observations, "F6")
    styles = style_report(observations)
    cross = harmonic_rhythm_cross_table(observations)
    source_form_distribution = Counter(row["source_event_count"] for row in observations if row["source_style"] == "default")
    used_patterns = sorted({row["pattern_name"] for row in observations})

    renderer_inventory = {
        name: [
            {
                "instruction": parse_pattern_token(token)[0],
                "duration_beats": fraction_payload(parse_pattern_token(token)[1]),
                "source_token": token,
            }
            for token in tokens
        ]
        for name, tokens in sorted(patterns.items())
    }
    logical_count = len(normalization["definitions"])
    style_count = len(raw_styles)
    key_count = len(key_pairs)
    return {
        "schema_version": SCHEMA_VERSION,
        "stage": STAGE,
        "source": normalization["source"],
        "dependencies": {
            "h1_normalized_json_sha256": h1_digest,
            "expected_h1_normalized_json_sha256": EXPECTED_H1_NORMALIZED_SHA256,
            "h2_fingerprints_json_sha256": h2_digest,
            "expected_h2_fingerprints_json_sha256": EXPECTED_H2_FINGERPRINTS_SHA256,
            "h3_functional_json_sha256": h3_digest,
            "expected_h3_functional_json_sha256": EXPECTED_H3_FUNCTIONAL_SHA256,
            "f0_f4_stability": stability,
        },
        "source_rhythm_provenance": {
            "critical_blob_sha1": blob_sha1,
            "generator_source_styles_raw": raw_styles,
            "generator_source_styles": [source_style_name(style) for style in raw_styles],
            "generator_source_style_count": style_count,
            "renderer_pattern_inventory_count": len(patterns),
            "renderer_pattern_inventory": renderer_inventory,
            "renderer_patterns_used_by_progression_pack": used_patterns,
            "renderer_execution": "NOT_PERFORMED",
            "midi_decode": "NOT_PERFORMED",
            "unknown_pattern_token_policy": "REJECT_NO_LEGACY_FALLBACK",
        },
        "rhythm_semantics": {
            "N": "consume next source progression event and emit a new segment",
            "S": "repeat current chord as a new MIDI-note onset/retrigger",
            "X": "rest segment",
            "numeric_prefix": "segment duration multiplier in beats",
            "continuation_policy": "S is retrigger, not tie/hold; continuation=false",
            "rest_identity_policy": "REST_SOURCE and REST_PATTERN share canonical F5 kind REST; origin remains diagnostic",
            "default_style_policy": "no source rests -> long@1 beat base; source rests -> basic@2 beat base",
            "long_odd_policy": "final long-pattern segment doubles for odd source-event counts",
        },
        "fingerprint_status": {
            "F0": "FROZEN_FROM_H2",
            "F1": "FROZEN_FROM_H2",
            "F2": "FROZEN_FROM_H2",
            "F3": "FROZEN_FROM_H2",
            "F4": "FROZEN_FROM_H3",
            "F5": "COMPUTED_H4_CHORD_RHYTHM",
            "F6": "COMPUTED_H4_F3_PLUS_F5",
        },
        "support_contract": {
            "logical_harmonic_support_count": logical_count,
            "rhythm_style_observation_count": len(observations),
            "styles_per_logical_definition": style_count,
            "key_projection_count": key_count,
            "physical_progression_materialization_count": logical_count * style_count * key_count,
            "style_materialization_increases_harmonic_support": False,
            "key_transposition_increases_harmonic_support": False,
            "definitions_removed": 0,
            "runtime_admission": "NOT_PERFORMED",
            "source_incidence_to_runtime_weight": "FORBIDDEN",
            "next_stage": "H5_STAGE15_REPRESENTABILITY",
        },
        "source_form_distribution": {str(k): v for k, v in sorted(source_form_distribution.items())},
        "rhythm_vocabulary_interpretation": {
            "source_pattern_masks_used": len(used_patterns),
            "source_event_count_classes": len(source_form_distribution),
            "realized_f5_identity_count": len(rhythm_shapes),
            "realized_f5_count_is_independent_authored_pattern_count": False,
        },
        "segment_statistics": {
            "total_segments": sum(row["segment_count"] for row in observations),
            "note_onset_segments": sum(row["note_onset_count"] for row in observations),
            "same_chord_retrigger_segments": sum(row["same_chord_retrigger_count"] for row in observations),
            "rest_segments": sum(row["rest_segment_count"] for row in observations),
            "pattern_rest_segments": sum(row["pattern_rest_segment_count"] for row in observations),
            "source_rest_segments": sum(row["source_rest_segment_count"] for row in observations),
            "continuation_segments": 0,
        },
        "styles": styles,
        "F5": f5,
        "F6": f6,
        "harmonic_rhythm_cross_table": cross,
        "rhythm_shapes": rhythm_shapes,
        "observations": observations,
    }


def render_markdown(result: dict[str, Any]) -> str:
    support = result["support_contract"]
    stats = result["segment_statistics"]
    f5, f6 = result["F5"], result["F6"]
    cross = result["harmonic_rhythm_cross_table"]
    vocab = result["rhythm_vocabulary_interpretation"]
    lines = [
        "# Harmony Atlas H4 ChordRhythm Extraction", "",
        "**Status:** generated research evidence / H4 checkpoint  ",
        f"**Source:** `{result['source']['repository']} @ {result['source']['commit']}`  ",
        f"**Evidence class:** `{result['source']['evidence_class']}`  ",
        "**Runtime impact:** none", "", "## Identity boundary", "",
        "```text",
        "F0 SourceIdentity              FROZEN_FROM_H2",
        "F1 TranspositionInvariant      FROZEN_FROM_H2",
        "F2 RootSequence                FROZEN_FROM_H2",
        "F3 RootQualitySequence         FROZEN_FROM_H2",
        "F4 FunctionalClassSequence     FROZEN_FROM_H3",
        "F5 ChordRhythmIdentity         COMPUTED_H4_CHORD_RHYTHM",
        "F6 CombinedIdentity            COMPUTED_H4_F3_PLUS_F5",
        "```", "",
        "H4 recomputes and verifies F0-F4 before creating F5/F6.", "",
        "## Source rhythm semantics", "",
        "Pinned `gen.py`, `c2mpatterns.py` and `c2m.py` are parsed as source evidence. H4 executes no upstream renderer and decodes no generated MIDI.", "",
        "```text", "N  next progression event / new chord onset",
        "S  same chord / new note onset (retrigger)", "X  rest",
        "numeric prefix  duration multiplier in beats", "```", "",
        "`S` is not a tie/hold continuation: pinned `c2m.py` emits `addNote(...)` for every non-rest segment. H4 therefore records retriggers separately and continuation=false.", "",
        "`REST_SOURCE` and `REST_PATTERN` remain separate diagnostics, but both canonicalize to `REST` inside F5 because rest origin is provenance, not rhythm identity.", "",
        "## Support accounting", "",
        f"Logical harmonic definitions: **{support['logical_harmonic_support_count']}**.",
        f"Rhythm-style observations: **{support['rhythm_style_observation_count']}**.",
        f"Generator styles per definition: **{support['styles_per_logical_definition']}**.",
        f"Key projections: **{support['key_projection_count']}**.",
        f"Physical progression MIDI materializations implied by source generation: **{support['physical_progression_materialization_count']}**.", "",
        "Only the 190 logical definitions count as harmonic support. Style and key materialization multiplicity cannot inflate support or runtime weights.", "",
        "## Realized rhythm vocabulary", "",
        f"Source generator pattern masks used: **{vocab['source_pattern_masks_used']}**.",
        f"Source event-count classes: **{vocab['source_event_count_classes']}**.",
        f"Realized F5 identities: **{vocab['realized_f5_identity_count']}**.", "",
        "The realized F5 count is **not** a claim that the source contains that many independently authored rhythm patterns: it is the result of source masks conditioned by progression form length.", "",
        "Source-event-count distribution:", "", "| Events | Definitions |", "|---:|---:|",
    ]
    for count, definitions in result["source_form_distribution"].items():
        lines.append(f"| {count} | {definitions} |")
    lines += [
        "", "## F5 / F6", "",
        "| Level | Observations | Unique classes | Duplicate groups | Surplus | Cross-style groups | Largest class |",
        "|---|---:|---:|---:|---:|---:|---:|",
        f"| F5 | {f5['definition_observation_count']} | {f5['unique_class_count']} | {f5['duplicate_group_count']} | {f5['duplicate_surplus_count']} | {f5['cross_style_duplicate_group_count']} | {f5['largest_class_size']} |",
        f"| F6 | {f6['definition_observation_count']} | {f6['unique_class_count']} | {f6['duplicate_group_count']} | {f6['duplicate_surplus_count']} | {f6['cross_style_duplicate_group_count']} | {f6['largest_class_size']} |", "",
        "F5 fingerprints realized onset/retrigger/rest structure plus exact rational durations, independent of pitch, quality, family and style name. F6 combines exact F3 harmonic identity with F5.", "",
        "## Segment statistics", "",
        f"- total segments: **{stats['total_segments']}**",
        f"- note-onset segments: **{stats['note_onset_segments']}**",
        f"- same-chord retrigger segments: **{stats['same_chord_retrigger_segments']}**",
        f"- rest segments: **{stats['rest_segments']}**",
        f"- pattern-rest segments: **{stats['pattern_rest_segments']}**",
        f"- source-rest segments: **{stats['source_rest_segments']}**",
        f"- continuation segments: **{stats['continuation_segments']}**", "",
        "## Source style inventory", "",
        "| Style | Observations | Unique F5 | Patterns | Onsets | Retriggers | Rests | Continuations |",
        "|---|---:|---:|---|---:|---:|---:|---:|",
    ]
    for style, row in result["styles"].items():
        lines.append(
            f"| `{style}` | {row['observation_count']} | {row['unique_f5_count']} | "
            f"{', '.join(row['pattern_names'])} | {row['note_onset_count']} | "
            f"{row['same_chord_retrigger_count']} | {row['rest_segment_count']} | "
            f"{row['continuation_segment_count']} |"
        )
    lines += [
        "", "## Harmonic identity × rhythm identity", "",
        f"F3 harmonic classes: **{cross['f3_class_count']}**.", "",
        "| Unique F5 per F3 | F3 classes |", "|---:|---:|",
    ]
    for count, classes in cross["unique_f5_count_per_f3_distribution"].items():
        lines.append(f"| {count} | {classes} |")
    cross_style = [group for group in f5["duplicate_groups"] if group["cross_style"]]
    lines += [
        "", "## Cross-style F5 equivalence", "",
        f"F5 classes shared by multiple source styles: **{len(cross_style)}**.",
    ]
    if cross_style:
        for group in cross_style[:12]:
            lines.append(
                f"- `{group['class_id']}` — {group['observation_count']} observations; styles: "
                + ", ".join(f"`{style}`" for style in group["source_styles"])
            )
    else:
        lines.append("- none in pinned source")
    lines += [
        "", "## H4 contract", "",
        "- exact H1/H2/H3 artifact digests are mandatory;",
        "- F0-F4 are recomputed and verified unchanged;",
        "- source rhythm masks are parsed without executing upstream code;",
        "- unknown renderer pattern tokens are rejected rather than accepted through legacy fallback;",
        "- style/key multiplicity never increases harmonic support;",
        "- same-chord `S` means retrigger, not continuation;",
        "- rest provenance is excluded from F5 identity;",
        "- F5 is pitch-independent ChordRhythm identity;",
        "- F6 is exact F3 + F5 combined identity;",
        "- zero source definitions are removed and no runtime candidate is selected;",
        "- source incidence never becomes runtime probability.", "",
        "Next stage: **H5 Stage 15 representability report**. H5 may compare F3/F5/F6 evidence to current runtime contracts but must not change production code.", "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract Harmony Atlas ChordRhythm evidence and compute F5/F6")
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--normalization", type=Path, required=True)
    parser.add_argument("--fingerprints", type=Path, required=True)
    parser.add_argument("--functional", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--markdown-output", type=Path)
    args = parser.parse_args()
    normalization, h1_digest = load_verified_json(args.normalization, EXPECTED_H1_NORMALIZED_SHA256, "H1 normalized JSON")
    fingerprints, h2_digest = load_verified_json(args.fingerprints, EXPECTED_H2_FINGERPRINTS_SHA256, "H2 fingerprints JSON")
    functional, h3_digest = load_verified_json(args.functional, EXPECTED_H3_FUNCTIONAL_SHA256, "H3 functional JSON")
    result = build_h4(
        args.source_root.resolve(), normalization, fingerprints, functional,
        h1_digest=h1_digest, h2_digest=h2_digest, h3_digest=h3_digest,
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
