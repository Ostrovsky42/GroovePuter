#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import harmony_atlas_h0 as h0

SCHEMA_VERSION = "1.0.0"
STAGE = "H1_CANONICAL_PARSER_NORMALIZATION"

NOTATION_CLASS_BY_FAMILY = {
    "Major": "TRADITIONAL_MAJOR",
    "Minor": "TRADITIONAL_MINOR",
    "Modal": "IONIAN_RELATIVE_MODAL",
}

KNOWN_MOOD_TAGS = {
    "Anguished", "Dark", "Dramatic", "Empowered", "Excited", "Fearful",
    "Hopeful", "Joyful", "Lonely", "Mysterious", "Nostalgic", "Peaceful",
    "Playful", "Rebellious", "Relaxed", "Romantic", "Sad", "Spiritual",
    "Surprised", "Tender", "Triumphant",
}

DEGREE_BY_ROMAN = {"I": 0, "II": 1, "III": 2, "IV": 3, "V": 4, "VI": 5, "VII": 6}
ACCIDENTAL_TO_SEMITONES = {"": 0, "b": -1, "#": 1}
SUPPORTED_RAW_SUFFIXES = {
    "", "5", "6", "69", "7", "9", "M", "M-5", "M6", "M7",
    "add9", "dim", "dom7", "m", "m6", "m7", "m9", "madd9", "sus2", "sus4",
}

QUALITY_ENUMS = {
    "triad_class": ["MAJOR", "MINOR", "DIMINISHED", "SUSPENDED_2", "SUSPENDED_4", "POWER_5"],
    "triad_source": [
        "ROMAN_CASE", "EXPLICIT_MAJOR_SUFFIX", "EXPLICIT_MINOR_SUFFIX",
        "EXPLICIT_DIMINISHED_SUFFIX", "EXPLICIT_SUSPENDED_SUFFIX",
        "EXPLICIT_POWER_SUFFIX", "EXPLICIT_DOMINANT_SUFFIX",
    ],
    "extension_class": ["NONE", "SIXTH", "SEVENTH", "NINTH", "SIX_NINE", "ADD_NINTH"],
    "seventh_flavor": ["NONE", "UNSPECIFIED", "MAJOR", "DOMINANT"],
}


class NormalizationError(RuntimeError):
    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code


def source_definition_sha256(definition: str) -> str:
    return hashlib.sha256(definition.encode("utf-8")).hexdigest()


def roman_case(roman: str) -> str:
    if roman.isupper():
        return "UPPER"
    if roman.islower():
        return "LOWER"
    raise NormalizationError("MIXED_ROMAN_CASE", f"Roman numeral must be consistently cased: {roman!r}")


def inherited_triad_class(roman: str) -> str:
    return "MAJOR" if roman_case(roman) == "UPPER" else "MINOR"


def quality_from_suffix(roman: str, suffix: str) -> dict[str, Any]:
    if suffix not in SUPPORTED_RAW_SUFFIXES:
        raise NormalizationError("UNSUPPORTED_SUFFIX", f"unsupported source suffix {suffix!r}")

    triad = inherited_triad_class(roman)
    source = "ROMAN_CASE"
    extension = "NONE"
    seventh = "NONE"
    fifth_alt = 0

    if suffix == "M":
        triad, source = "MAJOR", "EXPLICIT_MAJOR_SUFFIX"
    elif suffix == "m":
        triad, source = "MINOR", "EXPLICIT_MINOR_SUFFIX"
    elif suffix == "dim":
        triad, source = "DIMINISHED", "EXPLICIT_DIMINISHED_SUFFIX"
    elif suffix == "sus2":
        triad, source = "SUSPENDED_2", "EXPLICIT_SUSPENDED_SUFFIX"
    elif suffix == "sus4":
        triad, source = "SUSPENDED_4", "EXPLICIT_SUSPENDED_SUFFIX"
    elif suffix == "5":
        triad, source = "POWER_5", "EXPLICIT_POWER_SUFFIX"
    elif suffix == "6":
        extension = "SIXTH"
    elif suffix == "69":
        extension = "SIX_NINE"
    elif suffix == "7":
        extension, seventh = "SEVENTH", "UNSPECIFIED"
    elif suffix == "9":
        extension, seventh = "NINTH", "UNSPECIFIED"
    elif suffix == "add9":
        extension = "ADD_NINTH"
    elif suffix == "M-5":
        triad, source, fifth_alt = "MAJOR", "EXPLICIT_MAJOR_SUFFIX", -1
    elif suffix == "M6":
        triad, source, extension = "MAJOR", "EXPLICIT_MAJOR_SUFFIX", "SIXTH"
    elif suffix == "M7":
        triad, source, extension, seventh = "MAJOR", "EXPLICIT_MAJOR_SUFFIX", "SEVENTH", "MAJOR"
    elif suffix == "m6":
        triad, source, extension = "MINOR", "EXPLICIT_MINOR_SUFFIX", "SIXTH"
    elif suffix == "m7":
        triad, source, extension, seventh = "MINOR", "EXPLICIT_MINOR_SUFFIX", "SEVENTH", "UNSPECIFIED"
    elif suffix == "m9":
        triad, source, extension, seventh = "MINOR", "EXPLICIT_MINOR_SUFFIX", "NINTH", "UNSPECIFIED"
    elif suffix == "madd9":
        triad, source, extension = "MINOR", "EXPLICIT_MINOR_SUFFIX", "ADD_NINTH"
    elif suffix == "dom7":
        triad, source, extension, seventh = "MAJOR", "EXPLICIT_DOMINANT_SUFFIX", "SEVENTH", "DOMINANT"

    return {
        "triad_class": triad,
        "triad_source": source,
        "extension_class": extension,
        "seventh_flavor": seventh,
        "fifth_alteration_semitones": fifth_alt,
        "raw_suffix": suffix,
    }


def normalize_chord_token(token: str) -> dict[str, Any]:
    parsed = h0.lexical_chord_token(token)
    if parsed is None:
        raise NormalizationError("UNPARSEABLE_ROMAN_TOKEN", f"cannot parse {token!r}")
    accidental, roman, suffix = parsed
    degree_key = roman.upper()
    if degree_key not in DEGREE_BY_ROMAN:
        raise NormalizationError("UNSUPPORTED_DEGREE", f"unsupported Roman degree {roman!r}")
    if accidental not in ACCIDENTAL_TO_SEMITONES:
        raise NormalizationError("UNSUPPORTED_ACCIDENTAL", f"unsupported accidental {accidental!r}")
    return {
        "kind": "CHORD",
        "source_token": token,
        "root": {
            "diatonic_degree": DEGREE_BY_ROMAN[degree_key],
            "alteration_semitones": ACCIDENTAL_TO_SEMITONES[accidental],
            "notation_case": roman_case(roman),
            "source_accidental": accidental,
            "source_roman": roman,
        },
        "quality": quality_from_suffix(roman, suffix),
    }


def render_source_token(event: dict[str, Any]) -> str:
    if event.get("kind") != "CHORD":
        raise NormalizationError("NOT_CHORD_EVENT", "round-trip requires CHORD event")
    return (
        str(event["root"]["source_accidental"])
        + str(event["root"]["source_roman"])
        + str(event["quality"]["raw_suffix"])
    )


def type_descriptors(descriptors: list[str]) -> tuple[dict[str, list[str]], list[str]]:
    typed = {"mood": [], "structural": [], "catalog": []}
    unknown: list[str] = []
    for descriptor in descriptors:
        if descriptor in KNOWN_MOOD_TAGS:
            typed["mood"].append(descriptor)
        elif descriptor in h0.STRUCTURAL_TAGS:
            typed["structural"].append(descriptor)
        elif descriptor in h0.CATALOG_TAGS:
            typed["catalog"].append(descriptor)
        else:
            unknown.append(descriptor)
    return typed, unknown


def canonical_token_key(event: dict[str, Any]) -> str:
    root, quality = event["root"], event["quality"]
    return "|".join([
        str(root["diatonic_degree"]), f"{int(root['alteration_semitones']):+d}",
        str(quality["triad_class"]), str(quality["extension_class"]),
        str(quality["seventh_flavor"]), f"{int(quality['fifth_alteration_semitones']):+d}",
    ])


def normalize_definition(family: str, index: int, definition: str) -> tuple[dict[str, Any] | None, dict[str, Any] | None]:
    source_id = f"{family}:{index:03d}"
    progression, descriptors = h0.split_progression(definition)
    tags, unknown_descriptors = type_descriptors(descriptors)
    reasons: list[dict[str, Any]] = [
        {"code": "UNKNOWN_DESCRIPTOR", "descriptor": descriptor}
        for descriptor in unknown_descriptors
    ]
    events: list[dict[str, Any]] = []

    for position, source_token in enumerate(h0.progression_tokens(progression)):
        if source_token == "X":
            events.append({"kind": "REST", "position": position})
            continue
        try:
            event = normalize_chord_token(source_token)
            rendered = render_source_token(event)
            if rendered != source_token:
                reasons.append({
                    "code": "ROUND_TRIP_MISMATCH", "position": position,
                    "token": source_token, "rendered": rendered,
                })
                continue
            event["position"] = position
            events.append(event)
        except NormalizationError as exc:
            reasons.append({
                "code": exc.code, "position": position,
                "token": source_token, "message": str(exc),
            })

    if reasons:
        return None, {
            "source_id": source_id,
            "source_family": family,
            "source_definition_sha256": source_definition_sha256(definition),
            "reasons": reasons,
        }

    chord_count = sum(event["kind"] == "CHORD" for event in events)
    rest_count = sum(event["kind"] == "REST" for event in events)
    return {
        "source_id": source_id,
        "source_family": family,
        "notation_class": NOTATION_CLASS_BY_FAMILY[family],
        "source_definition_sha256": source_definition_sha256(definition),
        "event_count": len(events),
        "chord_event_count": chord_count,
        "rest_event_count": rest_count,
        "tags": tags,
        "events": events,
    }, None


def build_normalization(source_root: Path, *, verify_pin: bool = True) -> dict[str, Any]:
    source_root = source_root.resolve()
    h0_audit = h0.build_audit(source_root, verify_pin=verify_pin)
    assignments = h0.literal_assignments(source_root / "chords.py")
    progressions = {
        family: h0.require_string_list(assignments, assignment)
        for family, assignment in h0.PROGRESSION_ASSIGNMENTS.items()
    }

    definitions: list[dict[str, Any]] = []
    quarantine: list[dict[str, Any]] = []
    incidence: Counter[str] = Counter()
    token_families: dict[str, set[str]] = defaultdict(set)
    token_canonical: dict[str, dict[str, Any]] = {}
    suffix_incidence: Counter[str] = Counter()

    for family in ("Major", "Minor", "Modal"):
        for index, definition in enumerate(progressions[family], start=1):
            normalized, rejected = normalize_definition(family, index, definition)
            if rejected is not None:
                quarantine.append(rejected)
                continue
            assert normalized is not None
            definitions.append(normalized)
            for event in normalized["events"]:
                if event["kind"] != "CHORD":
                    continue
                source_token = event["source_token"]
                incidence[source_token] += 1
                token_families[source_token].add(family)
                suffix_incidence[event["quality"]["raw_suffix"]] += 1
                canonical = {
                    "root": event["root"],
                    "quality": event["quality"],
                    "canonical_token_key": canonical_token_key(event),
                }
                previous = token_canonical.get(source_token)
                if previous is not None and previous != canonical:
                    raise NormalizationError(
                        "NONDETERMINISTIC_TOKEN_NORMALIZATION",
                        f"raw token {source_token!r} normalized inconsistently",
                    )
                token_canonical[source_token] = canonical

    token_vocabulary: list[dict[str, Any]] = []
    for token_id, source_token in enumerate(sorted(token_canonical), start=1):
        canonical = token_canonical[source_token]
        token_vocabulary.append({
            "token_id": f"T{token_id:03d}",
            "source_token": source_token,
            "source_event_incidence": incidence[source_token],
            "source_families": sorted(token_families[source_token]),
            "canonical_token_key": canonical["canonical_token_key"],
            "root": canonical["root"],
            "quality": canonical["quality"],
        })
    token_id_by_source = {row["source_token"]: row["token_id"] for row in token_vocabulary}

    compact_definitions: list[dict[str, Any]] = []
    chord_event_count = 0
    rest_event_count = 0
    for definition in definitions:
        refs: list[str] = []
        for event in definition["events"]:
            if event["kind"] == "REST":
                refs.append("REST")
                rest_event_count += 1
            else:
                refs.append(token_id_by_source[event["source_token"]])
                chord_event_count += 1
        compact_definitions.append({
            "source_id": definition["source_id"],
            "source_family": definition["source_family"],
            "notation_class": definition["notation_class"],
            "source_definition_sha256": definition["source_definition_sha256"],
            "event_count": definition["event_count"],
            "chord_event_count": definition["chord_event_count"],
            "rest_event_count": definition["rest_event_count"],
            "tags": definition["tags"],
            "event_refs": refs,
        })

    observed_suffixes = sorted(suffix_incidence)
    unsupported = sorted(set(observed_suffixes) - SUPPORTED_RAW_SUFFIXES)
    if unsupported:
        raise NormalizationError("UNSUPPORTED_OBSERVED_SUFFIX_SET", f"unsupported suffixes: {unsupported}")

    return {
        "schema_version": SCHEMA_VERSION,
        "stage": STAGE,
        "source": h0_audit["source"],
        "h0_dependency": {
            "logical_definition_count": h0_audit["progressions"]["logical_definition_count"],
            "raw_chord_token_unique_count": h0_audit["progressions"]["raw_chord_token_unique_count"],
            "raw_suffix_vocabulary": h0_audit["progressions"]["raw_suffix_vocabulary"],
        },
        "normalization_contract": {
            "source_family_preserved": True,
            "event_order_preserved": True,
            "repeated_events_preserved": True,
            "explicit_rests_preserved": True,
            "altered_degree_preserved": True,
            "source_spelling_round_trip_required": True,
            "unknown_notation_policy": "QUARANTINE_DEFINITION",
            "unknown_descriptor_policy": "QUARANTINE_DEFINITION",
            "absolute_midi_projection": "FORBIDDEN",
            "progression_deduplication": "NOT_PERFORMED",
            "functional_classification": "NOT_PERFORMED",
            "runtime_admission": "NOT_PERFORMED",
            "next_stage": "H2_STRUCTURAL_FINGERPRINTS_DEDUP",
        },
        "quality_schema": {
            "enums": QUALITY_ENUMS,
            "supported_raw_suffix_count": len(SUPPORTED_RAW_SUFFIXES),
            "supported_raw_suffixes": sorted(SUPPORTED_RAW_SUFFIXES),
            "generic_seventh_policy": (
                "Raw 7/9 preserve seventh_flavor=UNSPECIFIED; they are not collapsed to explicit dom7 or M7."
            ),
        },
        "summary": {
            "logical_definition_count": sum(len(items) for items in progressions.values()),
            "admitted_definition_count": len(compact_definitions),
            "quarantined_definition_count": len(quarantine),
            "normalized_chord_event_count": chord_event_count,
            "normalized_rest_event_count": rest_event_count,
            "raw_token_vocabulary_count": len(token_vocabulary),
            "observed_raw_suffix_count": len(observed_suffixes),
            "observed_raw_suffixes": observed_suffixes,
            "unobserved_supported_suffixes": sorted(SUPPORTED_RAW_SUFFIXES - set(observed_suffixes)),
            "quarantine_reason_count": sum(len(row["reasons"]) for row in quarantine),
        },
        "token_vocabulary": token_vocabulary,
        "definitions": compact_definitions,
        "quarantine": quarantine,
    }


def render_markdown(result: dict[str, Any]) -> str:
    summary, source = result["summary"], result["source"]
    suffixes = result["quality_schema"]["supported_raw_suffixes"]
    lines = [
        "# Harmony Atlas H1 Canonical Normalization", "",
        "**Status:** generated research evidence / H1 checkpoint  ",
        f"**Source:** `{source['repository']} @ {source['commit']}`  ",
        f"**Evidence class:** `{source['evidence_class']}`  ",
        "**Runtime impact:** none", "", "## Gate result", "",
        "| Item | Count |", "|---|---:|",
        f"| Logical definitions | {summary['logical_definition_count']} |",
        f"| Admitted definitions | {summary['admitted_definition_count']} |",
        f"| Quarantined definitions | {summary['quarantined_definition_count']} |",
        f"| Normalized chord events | {summary['normalized_chord_event_count']} |",
        f"| Normalized rest events | {summary['normalized_rest_event_count']} |",
        f"| Raw token vocabulary | {summary['raw_token_vocabulary_count']} |",
        f"| Observed raw suffixes | {summary['observed_raw_suffix_count']} |", "",
        "H1 normalizes source notation only. It performs no progression deduplication, functional/cadential classification, runtime weighting, absolute MIDI projection, or firmware vocabulary admission.",
        "", "## Canonical root representation", "",
        "Each chord token is separated into:", "", "```text",
        "FunctionalDegree", "  diatonic_degree       0..6",
        "  alteration_semitones  -1 | 0 | +1 in the pinned corpus",
        "  notation_case         UPPER | LOWER",
        "  source_accidental     exact source spelling",
        "  source_roman          exact source Roman spelling", "```", "",
        "`bIII`, `III` and `#III` therefore cannot collapse to the same root identity.",
        "", "## Loss-aware chord quality", "",
        "Quality is decomposed into triad class, extension class, seventh flavor, fifth alteration and exact raw suffix.",
        "", "Critical non-equivalence:", "", "```text",
        "I7    -> MAJOR + SEVENTH + UNSPECIFIED seventh flavor",
        "Idom7 -> MAJOR + SEVENTH + DOMINANT",
        "IM7   -> MAJOR + SEVENTH + MAJOR", "```", "",
        "H1 refuses to guess that generic `7` is equivalent to explicit `dom7` or `M7`.",
        "", f"Supported pinned progression suffixes ({len(suffixes)}): "
        + ", ".join(f"`{value or '<empty>'}`" for value in suffixes),
        "", "## Source family / notation boundary", "", "```text",
        "Major -> TRADITIONAL_MAJOR", "Minor -> TRADITIONAL_MINOR",
        "Modal -> IONIAN_RELATIVE_MODAL", "```", "",
        "Family remains part of every normalized progression; H1 claims no cross-family semantic equivalence.",
        "", "## Typed metadata", "",
        "Known descriptors are typed as mood, structural (`Cadence`) or catalog (`New`). Unknown descriptors quarantine the definition.",
        "", "## Quarantine", "",
        f"Quarantined definitions: **{summary['quarantined_definition_count']}**. Quarantine reasons: **{summary['quarantine_reason_count']}**.", "",
    ]
    if result["quarantine"]:
        for row in result["quarantine"]:
            lines.append(f"- `{row['source_id']}`: " + ", ".join(reason["code"] for reason in row["reasons"]))
    else:
        lines.append("No pinned-source definitions are quarantined.")
    lines += [
        "", "## Reproducibility / handoff", "",
        "- exact H0 source pin and critical blob verification are reused;",
        "- every admitted token round-trips to exact source spelling;",
        "- event order and repeated events are preserved;",
        "- explicit rests use `REST` event refs;",
        "- normalized definitions reference a deterministic token vocabulary;",
        "- H1 performs no progression deduplication;",
        "- H2 may consume this representation for explicit F0-F6 fingerprints;",
        "", "Next stage: **H2 structural fingerprints / dedup**.", "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Normalize pinned Harmony Atlas Roman progression notation")
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--markdown-output", type=Path)
    args = parser.parse_args()
    result = build_normalization(args.source_root, verify_pin=True)
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
