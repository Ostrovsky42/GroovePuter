#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
from collections import Counter, defaultdict
from fractions import Fraction
from pathlib import Path
from typing import Any

SCHEMA_VERSION = "1.0.0"
STAGE = "H5_STAGE15_REPRESENTABILITY"
EXPECTED_H1_SHA256 = "4783be1f77784a978f2239d8527214b3912eaf262e92430bff755679fec0734e"
EXPECTED_H4_SHA256 = "68c51f4ca8827167f2b6ff12543ed226164c9c8aa691816caac9102058849e81"
TARGET_REPOSITORY = "Ostrovsky42/GroovePuter"
TARGET_STAGE15_COMMIT = "fc42763e7798866e61895bf1b8d62339ec59e0a7"
TARGET_EVIDENCE_CLASS = "TARGET_CONTRACT_EVIDENCE"
TARGET_BLOB_SHA1 = {
    "src/generation/roles/chord_progression.h": "0a4e014fbe1b671a0cb50e22bee7c473720758a6",
    "src/generation/roles/chord_progression.cpp": "f83d0c7859a4e9fee8f1e878a2de0ad5401a873a",
    "src/generation/roles/chord_rhythm.h": "481c70a5c16ca4bda29b5eee946789f3f6ecbf2d",
    "src/generation/roles/chord_rhythm.cpp": "9a50e369e8c68d3f09116e4b0c793e9c898af3ef",
    "src/generation/rhythm/rhythm_types.h": "5a3d415d8ae2f4bdc35c9d3391cea3ef40bce613",
    "src/generation/tonal/tonal_materializer.cpp": "2ab2b5e2d05090d3f6e855d2d59abe4f23a4f1b6",
    "src/generation/migration/strong_rhythm_migration.cpp": "5641a2e02dfde652434271d12ed391254db5bcb5",
}

EXACT_QUALITY_MAP = {
    ("DIMINISHED", "NONE", "NONE", 0): "Diminished",
    ("SUSPENDED_4", "NONE", "NONE", 0): "Sus4",
    ("MAJOR", "SEVENTH", "DOMINANT", 0): "Dominant7",
    ("MAJOR", "SEVENTH", "MAJOR", 0): "Major7",
    ("MINOR", "SEVENTH", "UNSPECIFIED", 0): "Minor7",
    ("MINOR", "NINTH", "UNSPECIFIED", 0): "Minor9",
    ("MAJOR", "NINTH", "MAJOR", 0): "Major9",
}
CONTEXT_TRIADS = {
    ("MAJOR", "NONE", "NONE", 0),
    ("MINOR", "NONE", "NONE", 0),
}
TARGET_GRAMMARS = {
    "PopCycle": [
        [(0, 0, "Triad"), (4, 0, "Triad"), (5, 0, "Minor7"), (3, 0, "Major7")],
        [(0, 0, "Triad"), (5, 0, "Minor7"), (3, 0, "Major7"), (4, 0, "Dominant7")],
    ],
    "TwoFiveOne": [
        [(1, 0, "Minor7"), (4, 0, "Dominant7"), (0, 0, "Major7")],
        [(1, 0, "Minor9"), (4, 0, "Dominant7"), (0, 0, "Major9")],
    ],
    "ParallelShift": [
        [(0, 0, "Minor9"), (0, 1, "Minor9"), (0, 0, "Minor9"), (0, -1, "Minor9")],
        [(0, 0, "Minor7"), (0, -2, "Minor7"), (0, 0, "Minor7"), (0, 2, "Minor7")],
    ],
    "MinorFall": [
        [(0, 0, "Minor7"), (5, 0, "Major7"), (2, 0, "Major7"), (6, 0, "Major7")],
        [(0, 0, "Minor7"), (5, 0, "Triad"), (2, 0, "Triad"), (6, 0, "Triad")],
    ],
    "BorrowedLift": [
        [(0, 0, "Minor7"), (3, 0, "Major7"), (4, 0, "Dominant7"), (3, 1, "Major7")],
        [(0, 0, "Triad"), (4, 0, "Dominant7"), (3, 1, "Major7"), (0, 0, "Triad")],
    ],
}
TARGET_FIXED_RHYTHMS = {
    "HeldPad": ({0}, set(range(1, 12))),
    "WholeBarHold": ({0}, set(range(1, 16))),
    "HalfBarChange": ({0, 8}, set(range(1, 8)) | set(range(9, 16))),
    "OffbeatStab": ({2, 6, 10, 14}, set()),
    "BackbeatStab": ({4, 12}, set()),
    "AnticipatedChange": ({7, 15}, set()),
    "DubChordSpace": ({6, 14}, set()),
    "DubChordSpaceEmpty": (set(), set()),
    "SyncopatedComp": ({1, 4, 7, 10, 13}, set()),
}

class RepresentabilityError(RuntimeError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def git_blob_sha1(data: bytes) -> str:
    return hashlib.sha1(f"blob {len(data)}\0".encode("ascii") + data).hexdigest()


def load_verified_json(path: Path, expected: str, label: str) -> tuple[dict[str, Any], str]:
    data = path.read_bytes()
    digest = sha256_bytes(data)
    if digest != expected:
        raise RepresentabilityError(f"{label} digest mismatch: expected {expected}, got {digest}")
    value = json.loads(data.decode("utf-8"))
    if not isinstance(value, dict):
        raise RepresentabilityError(f"{label} must contain an object")
    return value, digest


def strip_comments(text: str) -> str:
    return re.sub(r"//.*", "", re.sub(r"/\*.*?\*/", "", text, flags=re.S))


def enum_members(text: str, name: str) -> list[str]:
    match = re.search(rf"enum\s+class\s+{name}\s*:\s*\w+\s*\{{(.*?)\}};", strip_comments(text), re.S)
    if not match:
        raise RepresentabilityError(f"cannot find enum {name}")
    return [part.split("=", 1)[0].strip() for part in match.group(1).split(",") if part.strip()]


def verify_target(target_root: Path) -> dict[str, Any]:
    blobs: dict[str, str] = {}
    for relative, expected in TARGET_BLOB_SHA1.items():
        path = target_root / relative
        if not path.is_file():
            raise RepresentabilityError(f"missing Stage15 target file: {relative}")
        digest = git_blob_sha1(path.read_bytes())
        blobs[relative] = digest
        if digest != expected:
            raise RepresentabilityError(f"target blob mismatch for {relative}: expected {expected}, got {digest}")

    cp_h = (target_root / "src/generation/roles/chord_progression.h").read_text()
    cp_cpp = strip_comments((target_root / "src/generation/roles/chord_progression.cpp").read_text())
    cr_h = (target_root / "src/generation/roles/chord_rhythm.h").read_text()
    rt_h = (target_root / "src/generation/rhythm/rhythm_types.h").read_text()
    tonal = strip_comments((target_root / "src/generation/tonal/tonal_materializer.cpp").read_text())
    bridge = strip_comments((target_root / "src/generation/migration/strong_rhythm_migration.cpp").read_text())

    qualities = enum_members(cp_h, "ChordQuality")
    rhythms = enum_members(cr_h, "ChordRhythmId")
    if qualities != ["Triad", "Minor7", "Major7", "Dominant7", "Sus4", "Minor9", "Major9", "Diminished", "Count"]:
        raise RepresentabilityError(f"unexpected ChordQuality target: {qualities}")
    if rhythms != ["Auto", "HeldPad", "WholeBarHold", "HalfBarChange", "OffbeatStab", "BackbeatStab", "AnticipatedChange", "SparseChordReply", "DubChordSpace", "SyncopatedComp", "Count"]:
        raise RepresentabilityError(f"unexpected ChordRhythmId target: {rhythms}")

    checks = {
        "max_events": "constexpr uint8_t kMaxHarmonicEvents = 8;" in cp_h,
        "offset": "constexpr int8_t kMaxRootOffsetSemitones = 2;" in cp_h,
        "steps": "constexpr uint8_t kStepsPerBar = 16;" in rt_h,
        "phrase": "constexpr uint8_t kMaxPhraseBars = 4;" in rt_h,
        "live_one_bar": "progressionRequest.phraseBars = 1;" in bridge,
        "event_count": "progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);" in bridge,
        "offset_consumed": "static_cast<int>(event.rootOffsetSemitones)" in tonal,
        "offset_catalog_gate": "rootOffsetSemitones == 0 || allowsChromaticRootOffset(id)" in cp_cpp,
    }
    if not all(checks.values()):
        raise RepresentabilityError("Stage15 target contract drift: " + ", ".join(k for k, v in checks.items() if not v))
    quality_reads = len(re.findall(r"\bevent\.quality\b", tonal))
    quality_tokens = len(re.findall(r"\bChordQuality\b", tonal))
    if quality_reads != 1 or quality_tokens != 1:
        raise RepresentabilityError(f"unexpected ChordQuality consumption: event.quality={quality_reads}, ChordQuality={quality_tokens}")

    return {
        "repository": TARGET_REPOSITORY,
        "commit": TARGET_STAGE15_COMMIT,
        "evidence_class": TARGET_EVIDENCE_CLASS,
        "blob_sha1": blobs,
        "chord_quality_members": qualities[:-1],
        "chord_rhythm_members": rhythms[1:-1],
        "max_harmonic_events": 8,
        "max_root_offset_semitones": 2,
        "steps_per_bar": 16,
        "max_phrase_bars_generic": 4,
        "live_progression_phrase_bars": 1,
        "quality_consumed_for_pitch": False,
        "root_offset_consumed_for_pitch": True,
        "same_chord_retrigger_field": False,
    }


def qsig(token: dict[str, Any]) -> tuple[str, str, str, int]:
    q = token["quality"]
    return q["triad_class"], q["extension_class"], q["seventh_flavor"], int(q["fifth_alteration_semitones"])


def qclass(signature: tuple[str, str, str, int]) -> tuple[str, str | None]:
    if signature in EXACT_QUALITY_MAP:
        return "EXACT_ENUM_LABEL", EXACT_QUALITY_MAP[signature]
    if signature in CONTEXT_TRIADS:
        return "CONTEXT_DEPENDENT_TRIAD", "Triad"
    return "UNREPRESENTABLE_QUALITY", None


def cyclic_match(sequence: list[Any], grammar: list[Any]) -> bool:
    return bool(grammar) and all(item == grammar[index % len(grammar)] for index, item in enumerate(sequence))


def harmonic_report(h1: dict[str, Any], target: dict[str, Any]) -> dict[str, Any]:
    if h1.get("stage") != "H1_CANONICAL_PARSER_NORMALIZATION" or h1.get("summary", {}).get("quarantined_definition_count") != 0:
        raise RepresentabilityError("invalid H1 dependency")
    tokens = {row["token_id"]: row for row in h1["token_vocabulary"]}
    rows = []
    qevents = Counter()
    qdefs: dict[str, set[str]] = defaultdict(set)
    unsupported_defs: dict[str, set[str]] = defaultdict(set)
    unsupported_events = Counter()
    altered, over8, raw_field, exact_field, catalog_exact, root_only = (set() for _ in range(6))

    for definition in h1["definitions"]:
        sid = definition["source_id"]
        events = [tokens[ref] for ref in definition["event_refs"] if ref != "REST"]
        classes, mapped, roots = [], [], []
        for token in events:
            sig = qsig(token)
            cls, label = qclass(sig)
            degree = int(token["root"]["diatonic_degree"])
            offset = int(token["root"]["alteration_semitones"])
            classes.append(cls)
            mapped.append((degree, offset, label))
            roots.append((degree, offset))
            qevents[cls] += 1
            qdefs[cls].add(sid)
            if cls == "UNREPRESENTABLE_QUALITY":
                key = "|".join(map(str, sig))
                unsupported_defs[key].add(sid)
                unsupported_events[key] += 1
        if any(offset != 0 for _, offset in roots):
            altered.add(sid)
        if len(events) > target["max_harmonic_events"]:
            over8.add(sid)
        raw_ok = len(events) <= target["max_harmonic_events"] and all(abs(o) <= target["max_root_offset_semitones"] for _, o in roots) and all(c != "UNREPRESENTABLE_QUALITY" for c in classes)
        if raw_ok:
            raw_field.add(sid)
        if raw_ok and all(c == "EXACT_ENUM_LABEL" for c in classes):
            exact_field.add(sid)

        full_matches, root_matches = [], []
        if len(events) <= target["max_harmonic_events"]:
            for name, variants in TARGET_GRAMMARS.items():
                for vi, grammar in enumerate(variants):
                    if cyclic_match(roots, [(d, o) for d, o, _ in grammar]):
                        root_matches.append(f"{name}:{vi}")
                    if all(c == "EXACT_ENUM_LABEL" for c in classes) and cyclic_match(mapped, grammar):
                        full_matches.append(f"{name}:{vi}")
        if full_matches:
            catalog_exact.add(sid)
        if root_matches:
            root_only.add(sid)
        rows.append({
            "source_id": sid,
            "source_family": definition["source_family"],
            "harmonic_event_count": len(events),
            "quality_classes": dict(sorted(Counter(classes).items())),
            "altered_degree": sid in altered,
            "raw_field_shape_encodable": raw_ok,
            "exact_quality_field_encodable": sid in exact_field,
            "current_catalog_exact_matches": full_matches,
            "current_catalog_root_only_matches": root_matches,
            "audible_f3_exact": bool(full_matches) and target["quality_consumed_for_pitch"],
        })

    unsupported = [
        {"quality_signature": key, "logical_definition_support": len(sids), "event_count": unsupported_events[key], "source_ids": sorted(sids)}
        for key, sids in unsupported_defs.items()
    ]
    unsupported.sort(key=lambda row: (-row["logical_definition_support"], -row["event_count"], row["quality_signature"]))
    return {
        "definition_count": len(rows),
        "quality_event_class_distribution": dict(sorted(qevents.items())),
        "quality_definition_support": {k: len(v) for k, v in sorted(qdefs.items())},
        "unsupported_quality_signatures": unsupported,
        "definitions_with_altered_degree": len(altered),
        "altered_degree_source_ids": sorted(altered),
        "definitions_over_max_harmonic_events": len(over8),
        "over_max_harmonic_event_source_ids": sorted(over8),
        "raw_field_shape_encodable_definition_count": len(raw_field),
        "exact_quality_field_encodable_definition_count": len(exact_field),
        "current_catalog_exact_definition_count": len(catalog_exact),
        "current_catalog_exact_source_ids": sorted(catalog_exact),
        "current_catalog_root_only_match_definition_count": len(root_only),
        "current_catalog_root_only_match_source_ids": sorted(root_only),
        "audible_f3_exact_definition_count": 0 if not target["quality_consumed_for_pitch"] else len(catalog_exact),
        "rows": rows,
    }


def frac(value: dict[str, int]) -> Fraction:
    return Fraction(int(value["numerator"]), int(value["denominator"]))


def fixed_rhythm_matches(shape: dict[str, Any]) -> list[str]:
    cursor, onsets, continuations = 0, set(), set()
    for segment in shape["canonical_f5_segments"]:
        steps = frac(segment["duration_beats"]) * 4
        if steps.denominator != 1 or steps <= 0:
            return []
        count = int(steps)
        if segment["kind"] == "CHORD_RETRIGGER":
            return []
        if cursor + count > 16:
            return []
        if segment["kind"] == "CHORD_ONSET":
            onsets.add(cursor)
            continuations.update(range(cursor + 1, cursor + count))
        elif segment["kind"] != "REST":
            return []
        cursor += count
    if cursor != 16:
        return []
    return sorted(name for name, (wanted_o, wanted_c) in TARGET_FIXED_RHYTHMS.items() if onsets == wanted_o and continuations == wanted_c)


def rhythm_report(h4: dict[str, Any], target: dict[str, Any]) -> dict[str, Any]:
    if h4.get("stage") != "H4_CHORD_RHYTHM_EXTRACTION":
        raise RepresentabilityError("invalid H4 dependency")
    shapes = {row["shape_id"]: row for row in h4["rhythm_shapes"]}
    gaps_obs, gaps_support = Counter(), defaultdict(set)
    rows, exact_obs, grid_ok = [], [], 0
    for observation in h4["observations"]:
        shape = shapes[observation["rhythm_shape_id"]]
        sid = observation["source_id"]
        phrase = frac(observation["phrase_length_beats"])
        gaps = []
        if phrase != 4:
            gaps.append("LIVE_ONE_BAR_DURATION")
        if phrase > 16:
            gaps.append("GENERIC_PHRASE_GT4_BARS")
        if observation["same_chord_retrigger_count"]:
            gaps.append("SAME_CHORD_RETRIGGER_SEMANTICS")
        if observation["note_onset_count"] > target["max_harmonic_events"]:
            gaps.append("HARMONIC_EVENTS_GT8")
        quarter = all((frac(seg["duration_beats"]) * 4).denominator == 1 for seg in shape["canonical_f5_segments"]) and all((frac(coord) * 4).denominator == 1 for coord in shape["note_onset_coordinates_beats"])
        if quarter:
            grid_ok += 1
        else:
            gaps.append("FINER_THAN_16TH_GRID")
        catalog = fixed_rhythm_matches(shape) if not gaps else []
        if phrase == 4 and quarter and not observation["same_chord_retrigger_count"] and not catalog:
            gaps.append("CURRENT_CHORD_RHYTHM_CATALOG_MISS")
        exact = not gaps and bool(catalog)
        if exact:
            exact_obs.append(observation["observation_id"])
        for gap in gaps:
            gaps_obs[gap] += 1
            gaps_support[gap].add(sid)
        rows.append({
            "observation_id": observation["observation_id"],
            "source_id": sid,
            "source_style": observation["source_style"],
            "F5": observation["fingerprints"]["F5"],
            "F6": observation["fingerprints"]["F6"],
            "phrase_length_beats": observation["phrase_length_beats"],
            "note_onset_count": observation["note_onset_count"],
            "same_chord_retrigger_count": observation["same_chord_retrigger_count"],
            "quarter_beat_grid_compatible": quarter,
            "current_fixed_catalog_matches": catalog,
            "current_chord_rhythm_exact": exact,
            "gaps": gaps,
        })
    return {
        "observation_count": len(rows),
        "unique_f5_count": int(h4["F5"]["unique_class_count"]),
        "exact_observation_count": len(exact_obs),
        "exact_unique_f5_count": len({row["F5"] for row in rows if row["current_chord_rhythm_exact"]}),
        "quarter_beat_grid_compatible_observations": grid_ok,
        "quarter_beat_grid_gap_observations": len(rows) - grid_ok,
        "gap_observation_counts": dict(sorted(gaps_obs.items())),
        "gap_logical_definition_support": {gap: len(sids) for gap, sids in sorted(gaps_support.items())},
        "gap_source_ids": {gap: sorted(sids) for gap, sids in sorted(gaps_support.items())},
        "rows": rows,
    }


def capability_ranking(harmonic: dict[str, Any], rhythm: dict[str, Any]) -> list[dict[str, Any]]:
    caps: list[dict[str, Any]] = []
    def add(name: str, support: int, dimension: str, observations: int | None = None, note: str = "") -> None:
        row: dict[str, Any] = {"capability": name, "logical_definition_support": support, "dimension": dimension, "support_is_non_additive": True, "note": note}
        if observations is not None:
            row["rhythm_observation_count"] = observations
        caps.append(row)
    gs, go = rhythm["gap_logical_definition_support"], rhythm["gap_observation_counts"]
    add("QUALITY_RENDERING_CONSUMPTION", harmonic["definition_count"], "Harmony/F3", note="Current TonalMaterializer validates ChordQuality but does not use it for pitch.")
    add("MULTI_BAR_CHORD_RHYTHM_IDENTITY", gs.get("LIVE_ONE_BAR_DURATION", 0), "Rhythm/F5", go.get("LIVE_ONE_BAR_DURATION", 0))
    add("SAME_CHORD_RETRIGGER_WITHOUT_HARMONIC_ADVANCE", gs.get("SAME_CHORD_RETRIGGER_SEMANTICS", 0), "Rhythm/F5", go.get("SAME_CHORD_RETRIGGER_SEMANTICS", 0))
    add("TRIAD_POLARITY_OR_EXPLICIT_CONTEXT", harmonic["quality_definition_support"].get("CONTEXT_DEPENDENT_TRIAD", 0), "Harmony/F3")
    add("GENERIC_ALTERED_DEGREE_REACHABILITY", harmonic["definitions_with_altered_degree"], "Harmony/F3")
    add("ADDITIONAL_CHORD_QUALITY_VOCABULARY", harmonic["quality_definition_support"].get("UNREPRESENTABLE_QUALITY", 0), "Harmony/F3")
    add("CHORD_RHYTHM_BEYOND_GENERIC_4_BAR_CONTAINER", gs.get("GENERIC_PHRASE_GT4_BARS", 0), "Rhythm/F5", go.get("GENERIC_PHRASE_GT4_BARS", 0))
    add("MORE_THAN_8_HARMONIC_ONSETS", gs.get("HARMONIC_EVENTS_GT8", 0), "Combined F3/F5", go.get("HARMONIC_EVENTS_GT8", 0))
    add("SOURCE_HARMONIC_FORM_GT8", harmonic["definitions_over_max_harmonic_events"], "Harmony/F3")
    caps.sort(key=lambda row: (-row["logical_definition_support"], row["capability"]))
    for rank, row in enumerate(caps, 1):
        row["rank"] = rank
    return caps


def build_report(h1: dict[str, Any], h4: dict[str, Any], target_root: Path, h1_digest: str, h4_digest: str) -> dict[str, Any]:
    if h1_digest != EXPECTED_H1_SHA256 or h4_digest != EXPECTED_H4_SHA256:
        raise RepresentabilityError("dependency digest contract mismatch")
    target = verify_target(target_root)
    harmonic = harmonic_report(h1, target)
    rhythm = rhythm_report(h4, target)
    return {
        "schema_version": SCHEMA_VERSION,
        "stage": STAGE,
        "dependencies": {"h1_normalized_json_sha256": h1_digest, "h4_chord_rhythm_json_sha256": h4_digest},
        "target_contract": target,
        "methodology": {
            "runtime_mutation": "NOT_PERFORMED",
            "production_files_changed": 0,
            "runtime_admission": "NOT_PERFORMED",
            "source_incidence_to_runtime_weight": "FORBIDDEN",
            "support_unit": "LogicalProgressionDefinition",
            "style_observations_are_support": False,
            "key_materializations_are_support": False,
            "quality_boundary": "ChordQuality enum presence is not audible quality rendering; current TonalMaterializer ignores quality when projecting pitch.",
            "capability_support_counts_are_additive": False,
            "next_stage": "H6_CURATED_RUNTIME_CANDIDATES",
        },
        "harmonic": harmonic,
        "rhythm": rhythm,
        "combined": {
            "h4_unique_f6_count": int(h4["F6"]["unique_class_count"]),
            "h4_observation_count": int(h4["F6"]["definition_observation_count"]),
            "current_exact_f6_unique_count": 0,
            "current_exact_f6_observation_count": 0,
            "exact_rule": "AUDIBLE_F3_EXACT AND CURRENT_CHORD_RHYTHM_F5_EXACT",
        },
        "ranked_deferred_capabilities": capability_ranking(harmonic, rhythm),
    }


def render_markdown(result: dict[str, Any]) -> str:
    h, r, c, t = result["harmonic"], result["rhythm"], result["combined"], result["target_contract"]
    lines = [
        "# Harmony Atlas H5 — Stage 15 Representability", "",
        "**Status:** generated R1 research evidence / H5 checkpoint  ",
        f"**Target:** `{t['repository']} @ {t['commit']}`  ",
        f"**Target evidence:** `{t['evidence_class']}`  ",
        "**Runtime impact:** none", "",
        "## Target boundary", "",
        "H5 compares frozen Harmony Atlas evidence against an exact Stage 15 RC source checkpoint. Target code is not copied into the research ancestry and H5 changes no production file.", "",
        f"- max harmonic events: **{t['max_harmonic_events']}**",
        f"- root offset field: **±{t['max_root_offset_semitones']} semitones**",
        f"- ChordRhythm grid: **{t['steps_per_bar']} steps/bar**",
        f"- generic phrase ceiling: **{t['max_phrase_bars_generic']} bars**",
        f"- live Stage15 progression request: **{t['live_progression_phrase_bars']} bar**",
        f"- `ChordQuality` consumed for pitch: **{str(t['quality_consumed_for_pitch']).lower()}**",
        f"- `rootOffsetSemitones` consumed for pitch: **{str(t['root_offset_consumed_for_pitch']).lower()}**", "",
        "## Harmonic representability (F3)", "", "| Level | Definitions |", "|---|---:|",
        f"| H1 logical definitions | {h['definition_count']} |",
        f"| Raw field-shape encodable | {h['raw_field_shape_encodable_definition_count']} |",
        f"| Exact quality-label field encodable | {h['exact_quality_field_encodable_definition_count']} |",
        f"| Exact current progression-catalog F3 | {h['current_catalog_exact_definition_count']} |",
        f"| Root path exact, ignoring quality | {h['current_catalog_root_only_match_definition_count']} |",
        f"| **Audible exact F3** | **{h['audible_f3_exact_definition_count']}** |", "",
        "| Quality class | Events | Logical definitions |", "|---|---:|---:|",
    ]
    for key in ("EXACT_ENUM_LABEL", "CONTEXT_DEPENDENT_TRIAD", "UNREPRESENTABLE_QUALITY"):
        lines.append(f"| `{key}` | {h['quality_event_class_distribution'].get(key, 0)} | {h['quality_definition_support'].get(key, 0)} |")
    lines += ["", f"Altered-degree definitions: **{h['definitions_with_altered_degree']}**.", f"Source harmonic forms >8 events: **{h['definitions_over_max_harmonic_events']}**.", "", "Root-path overlaps (quality intentionally ignored):", "", "```text", *h["current_catalog_root_only_match_source_ids"], "```", "", "### Unsupported quality signatures", "", "| H1 semantic quality | Logical support | Events |", "|---|---:|---:|"]
    for row in h["unsupported_quality_signatures"]:
        lines.append(f"| `{row['quality_signature']}` | {row['logical_definition_support']} | {row['event_count']} |")
    lines += ["", "## ChordRhythm representability (F5)", "", "| Item | Count |", "|---|---:|", f"| H4 rhythm observations | {r['observation_count']} |", f"| H4 unique F5 identities | {r['unique_f5_count']} |", f"| Exact current observations | {r['exact_observation_count']} |", f"| Exact current unique F5 | {r['exact_unique_f5_count']} |", f"| 16th-grid compatible observations | {r['quarter_beat_grid_compatible_observations']} |", f"| Finer-grid gaps | {r['quarter_beat_grid_gap_observations']} |", "", "| Gap | Logical definitions | Style observations |", "|---|---:|---:|"]
    for gap, support in r["gap_logical_definition_support"].items():
        lines.append(f"| `{gap}` | {support} | {r['gap_observation_counts'][gap]} |")
    lines += ["", "## Combined representability (F6)", "", f"H4: **{c['h4_unique_f6_count']}** unique F6 / **{c['h4_observation_count']}** observations.", f"Current exact F6: **{c['current_exact_f6_unique_count']} unique / {c['current_exact_f6_observation_count']} observations**.", "", "Exact F6 requires both audibly exact F3 and exact F5; root-only similarity, enum presence and rescaling are not substitutes.", "", "## Ranked deferred capabilities", "", "Counts overlap and are **not additive**; ranking uses logical definitions, never style/key multiplicity.", "", "| Rank | Capability | Dimension | Logical support | Rhythm observations |", "|---:|---|---|---:|---:|"]
    for row in result["ranked_deferred_capabilities"]:
        lines.append(f"| {row['rank']} | `{row['capability']}` | {row['dimension']} | {row['logical_definition_support']} | {row.get('rhythm_observation_count', '')} |")
    lines += ["", "## H5 contract", "", "- exact Stage15 target blobs are verified;", "- H1/H4 bytes are pinned by SHA-256;", "- enum support is separate from audible quality rendering;", "- altered-degree field capacity is separate from catalog reachability;", "- root-only overlap is diagnostic, never F3 equality;", "- F5 exactness preserves duration and retrigger semantics;", "- support ranking uses logical source definitions and overlapping counts;", "- no production changes, runtime weights or runtime candidates are created.", "", "Next stage: **H6 — curated runtime candidates**. Production integration remains separate.", ""]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Harmony Atlas H5 Stage15 representability report")
    parser.add_argument("--normalization", type=Path, required=True)
    parser.add_argument("--rhythm", type=Path, required=True)
    parser.add_argument("--stage15-root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--markdown-output", type=Path)
    args = parser.parse_args()
    h1, d1 = load_verified_json(args.normalization, EXPECTED_H1_SHA256, "H1 normalized JSON")
    h4, d4 = load_verified_json(args.rhythm, EXPECTED_H4_SHA256, "H4 ChordRhythm JSON")
    result = build_report(h1, h4, args.stage15_root.resolve(), d1, d4)
    json_text = json.dumps(result, indent=2, sort_keys=True) + "\n"
    markdown = render_markdown(result)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json_text, encoding="utf-8")
    else:
        print(json_text, end="")
    if args.markdown_output:
        args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_output.write_text(markdown, encoding="utf-8")

if __name__ == "__main__":
    main()
