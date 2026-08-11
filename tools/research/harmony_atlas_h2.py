#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable

SCHEMA_VERSION = "1.0.0"
STAGE = "H2_STRUCTURAL_FINGERPRINTS_DEDUP"
EXPECTED_H1_NORMALIZED_SHA256 = "4783be1f77784a978f2239d8527214b3912eaf262e92430bff755679fec0734e"

COMPUTED_LEVELS = ("F0", "F1", "F2", "F3")
ALL_LEVELS = ("F0", "F1", "F2", "F3", "F4", "F5", "F6")

FINGERPRINT_REGISTRY: dict[str, dict[str, Any]] = {
    "F0": {
        "name": "SourceIdentity",
        "status": "COMPUTED",
        "meaning": "Exact normalized source definition including source identity metadata.",
        "includes": ["source_id", "source_family", "notation_class", "source_definition_sha256"],
    },
    "F1": {
        "name": "TranspositionInvariant",
        "status": "COMPUTED",
        "meaning": "Authored Roman progression identity independent of absolute key projection.",
        "includes": ["source_family", "notation_class", "ordered exact source-token/rest sequence"],
        "excludes": ["source_id", "tags", "absolute key"],
    },
    "F2": {
        "name": "RootSequence",
        "status": "COMPUTED",
        "meaning": "Ordered functional roots including chromatic alteration and rests, ignoring chord quality.",
        "includes": ["ordered degree", "alteration_semitones", "rests"],
        "excludes": ["source_family", "Roman case", "source spelling", "chord quality", "tags"],
    },
    "F3": {
        "name": "RootQualitySequence",
        "status": "COMPUTED",
        "meaning": "Ordered functional roots plus loss-aware semantic quality and rests.",
        "includes": [
            "ordered degree", "alteration_semitones", "triad_class", "extension_class",
            "seventh_flavor", "fifth_alteration_semitones", "rests",
        ],
        "excludes": ["source_family", "source spelling", "triad_source", "tags"],
    },
    "F4": {
        "name": "FunctionalClassSequence",
        "status": "DEFERRED",
        "dependency": ["H3_FUNCTIONAL_ANALYSIS"],
        "reason": "Tonic/predominant/dominant/borrowed labels do not exist before H3 and must not be guessed in H2.",
    },
    "F5": {
        "name": "ChordRhythmIdentity",
        "status": "DEFERRED",
        "dependency": ["H4_CHORD_RHYTHM_EXTRACTION"],
        "reason": "H1 contains harmonic event order but not the timing/rest/continuation evidence required by F5.",
    },
    "F6": {
        "name": "CombinedIdentity",
        "status": "DEFERRED",
        "dependency": ["F3_COMPUTED", "F5_COMPUTED"],
        "reason": "Combined harmonic+rhythm identity cannot exist until F5 is available.",
    },
}


class FingerprintError(RuntimeError):
    pass


def canonical_json(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def sha256_json(level: str, payload: Any) -> str:
    material = f"{level}\0{canonical_json(payload)}".encode("utf-8")
    return hashlib.sha256(material).hexdigest()


def verify_h1_json_bytes(data: bytes) -> str:
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_H1_NORMALIZED_SHA256:
        raise FingerprintError(
            "H1 normalized input mismatch: expected "
            f"{EXPECTED_H1_NORMALIZED_SHA256}, got {digest}"
        )
    return digest


def require_h1_shape(normalization: dict[str, Any]) -> None:
    if normalization.get("stage") != "H1_CANONICAL_PARSER_NORMALIZATION":
        raise FingerprintError("H2 requires H1_CANONICAL_PARSER_NORMALIZATION input")
    summary = normalization.get("summary")
    if not isinstance(summary, dict):
        raise FingerprintError("H1 summary is missing")
    if int(summary.get("quarantined_definition_count", -1)) != 0:
        raise FingerprintError("H2 refuses H1 input with quarantined definitions")
    definitions = normalization.get("definitions")
    vocabulary = normalization.get("token_vocabulary")
    if not isinstance(definitions, list) or not isinstance(vocabulary, list):
        raise FingerprintError("H1 definitions/token_vocabulary are missing")
    admitted = int(summary.get("admitted_definition_count", -1))
    if admitted != len(definitions):
        raise FingerprintError("H1 admitted_definition_count does not match definitions")


def token_lookup(normalization: dict[str, Any]) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for row in normalization["token_vocabulary"]:
        token_id = row.get("token_id")
        if not isinstance(token_id, str) or not token_id:
            raise FingerprintError(f"invalid H1 token id: {token_id!r}")
        if token_id in result:
            raise FingerprintError(f"duplicate H1 token id: {token_id}")
        result[token_id] = row
    return result


def materialize_events(definition: dict[str, Any], tokens: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    refs = definition.get("event_refs")
    if not isinstance(refs, list):
        raise FingerprintError(f"{definition.get('source_id')} has invalid event_refs")
    events: list[dict[str, Any]] = []
    for position, ref in enumerate(refs):
        if ref == "REST":
            events.append({"kind": "REST", "position": position})
            continue
        if ref not in tokens:
            raise FingerprintError(f"{definition.get('source_id')} references unknown token {ref!r}")
        token = tokens[ref]
        events.append({
            "kind": "CHORD",
            "position": position,
            "token_id": ref,
            "source_token": token["source_token"],
            "root": token["root"],
            "quality": token["quality"],
        })
    if int(definition.get("event_count", -1)) != len(events):
        raise FingerprintError(f"{definition.get('source_id')} event_count mismatch")
    return events


def f0_payload(definition: dict[str, Any]) -> dict[str, Any]:
    return {
        "source_id": definition["source_id"],
        "source_family": definition["source_family"],
        "notation_class": definition["notation_class"],
        "source_definition_sha256": definition["source_definition_sha256"],
    }


def f1_event(event: dict[str, Any]) -> dict[str, Any]:
    if event["kind"] == "REST":
        return {"kind": "REST"}
    return {"kind": "CHORD", "source_token": event["source_token"]}


def f2_event(event: dict[str, Any]) -> dict[str, Any]:
    if event["kind"] == "REST":
        return {"kind": "REST"}
    root = event["root"]
    return {
        "kind": "CHORD",
        "diatonic_degree": int(root["diatonic_degree"]),
        "alteration_semitones": int(root["alteration_semitones"]),
    }


def f3_event(event: dict[str, Any]) -> dict[str, Any]:
    if event["kind"] == "REST":
        return {"kind": "REST"}
    root, quality = event["root"], event["quality"]
    return {
        "kind": "CHORD",
        "diatonic_degree": int(root["diatonic_degree"]),
        "alteration_semitones": int(root["alteration_semitones"]),
        "triad_class": quality["triad_class"],
        "extension_class": quality["extension_class"],
        "seventh_flavor": quality["seventh_flavor"],
        "fifth_alteration_semitones": int(quality["fifth_alteration_semitones"]),
    }


def fingerprint_payloads(definition: dict[str, Any], events: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "F0": f0_payload(definition),
        "F1": {
            "source_family": definition["source_family"],
            "notation_class": definition["notation_class"],
            "events": [f1_event(event) for event in events],
        },
        "F2": {"events": [f2_event(event) for event in events]},
        "F3": {"events": [f3_event(event) for event in events]},
    }


def tags_key(tags: Any) -> str:
    return canonical_json(tags)


def make_level_report(level: str, rows: list[dict[str, Any]]) -> dict[str, Any]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[row["fingerprints"][level]].append(row)

    classes: list[dict[str, Any]] = []
    duplicate_groups: list[dict[str, Any]] = []
    for class_index, digest in enumerate(sorted(grouped), start=1):
        members = sorted(grouped[digest], key=lambda row: row["source_id"])
        families = sorted({row["source_family"] for row in members})
        member_tags = sorted({tags_key(row["tags"]) for row in members})
        group = {
            "class_id": f"{level}:C{class_index:03d}",
            "fingerprint_sha256": digest,
            "size": len(members),
            "source_ids": [row["source_id"] for row in members],
            "source_families": families,
            "cross_family": len(families) > 1,
            "metadata_variant": len(member_tags) > 1,
        }
        classes.append(group)
        if len(members) > 1:
            duplicate_groups.append(group)

    definitions_in_duplicate_groups = sum(group["size"] for group in duplicate_groups)
    duplicate_surplus = sum(group["size"] - 1 for group in duplicate_groups)
    return {
        "status": "COMPUTED",
        "registry": FINGERPRINT_REGISTRY[level],
        "summary": {
            "definition_count": len(rows),
            "unique_class_count": len(classes),
            "duplicate_group_count": len(duplicate_groups),
            "definitions_in_duplicate_groups": definitions_in_duplicate_groups,
            "duplicate_surplus_count": duplicate_surplus,
            "largest_class_size": max((group["size"] for group in classes), default=0),
            "cross_family_duplicate_group_count": sum(group["cross_family"] for group in duplicate_groups),
            "metadata_variant_duplicate_group_count": sum(group["metadata_variant"] for group in duplicate_groups),
        },
        "classes": classes,
        "duplicate_groups": duplicate_groups,
    }


def event_sequence_key(events: Iterable[dict[str, Any]]) -> tuple[str, ...]:
    return tuple(canonical_json(event) for event in events)


def rotation_steps(left: tuple[str, ...], right: tuple[str, ...]) -> int | None:
    if len(left) != len(right) or len(left) < 2 or left == right:
        return None
    for shift in range(1, len(left)):
        if left[shift:] + left[:shift] == right:
            return shift
    return None


def repetition_factor(shorter: tuple[str, ...], longer: tuple[str, ...]) -> int | None:
    if not shorter or len(longer) <= len(shorter) or len(longer) % len(shorter) != 0:
        return None
    factor = len(longer) // len(shorter)
    if factor < 2:
        return None
    if shorter * factor == longer:
        return factor
    return None


def relation_sources(relation: dict[str, Any]) -> tuple[str, str]:
    if relation["kind"] == "CYCLIC_ROTATION":
        return relation["source_a"], relation["source_b"]
    if relation["kind"] == "REPETITION_EXTENSION":
        return relation["shorter_source"], relation["longer_source"]
    raise FingerprintError(f"unknown near relation kind: {relation['kind']!r}")


def relation_components(relations: list[dict[str, Any]]) -> dict[str, Any]:
    components: list[dict[str, Any]] = []
    count_by_kind: Counter[str] = Counter()

    for kind in sorted({relation["kind"] for relation in relations}):
        kind_relations = [relation for relation in relations if relation["kind"] == kind]
        adjacency: dict[str, set[str]] = defaultdict(set)
        edge_keys: set[tuple[str, str]] = set()
        for relation in kind_relations:
            left, right = relation_sources(relation)
            adjacency[left].add(right)
            adjacency[right].add(left)
            edge_keys.add(tuple(sorted((left, right))))

        unseen = set(adjacency)
        kind_index = 0
        while unseen:
            start = min(unseen)
            stack = [start]
            members: set[str] = set()
            while stack:
                current = stack.pop()
                if current in members:
                    continue
                members.add(current)
                unseen.discard(current)
                stack.extend(sorted(adjacency[current] - members, reverse=True))
            member_list = sorted(members)
            member_set = set(member_list)
            component_edges = [
                edge for edge in edge_keys if edge[0] in member_set and edge[1] in member_set
            ]
            kind_index += 1
            count_by_kind[kind] += 1
            components.append({
                "component_id": f"{kind}:C{kind_index:03d}",
                "kind": kind,
                "source_ids": member_list,
                "source_count": len(member_list),
                "pair_edge_count": len(component_edges),
                "dedup_equivalent": False,
            })

    components.sort(key=lambda row: (row["kind"], row["source_ids"]))
    return {
        "component_count": len(components),
        "component_count_by_kind": dict(sorted(count_by_kind.items())),
        "components": components,
    }


def near_relations(rows: list[dict[str, Any]]) -> dict[str, Any]:
    relations: list[dict[str, Any]] = []
    for left_index in range(len(rows)):
        left = rows[left_index]
        left_sequence = left["f3_sequence"]
        for right_index in range(left_index + 1, len(rows)):
            right = rows[right_index]
            right_sequence = right["f3_sequence"]
            if left["fingerprints"]["F3"] == right["fingerprints"]["F3"]:
                continue

            shift = rotation_steps(left_sequence, right_sequence)
            if shift is not None:
                relations.append({
                    "kind": "CYCLIC_ROTATION",
                    "source_a": left["source_id"],
                    "source_b": right["source_id"],
                    "event_count": len(left_sequence),
                    "rotation_steps_a_to_b": shift,
                    "dedup_equivalent": False,
                })
                continue

            factor = repetition_factor(left_sequence, right_sequence)
            if factor is not None:
                relations.append({
                    "kind": "REPETITION_EXTENSION",
                    "shorter_source": left["source_id"],
                    "longer_source": right["source_id"],
                    "repetition_factor": factor,
                    "dedup_equivalent": False,
                })
                continue
            factor = repetition_factor(right_sequence, left_sequence)
            if factor is not None:
                relations.append({
                    "kind": "REPETITION_EXTENSION",
                    "shorter_source": right["source_id"],
                    "longer_source": left["source_id"],
                    "repetition_factor": factor,
                    "dedup_equivalent": False,
                })

    relations.sort(key=lambda row: canonical_json(row))
    pair_counts = Counter(row["kind"] for row in relations)
    components = relation_components(relations)
    return {
        "policy": "REPORT_ONLY_NEVER_DEDUP",
        "pair_relation_count": len(relations),
        "pair_count_by_kind": dict(sorted(pair_counts.items())),
        **components,
        "relations": relations,
    }


def cross_level_diagnostics(rows: list[dict[str, Any]]) -> dict[str, Any]:
    by_f2: dict[str, list[dict[str, Any]]] = defaultdict(list)
    by_f3: dict[str, list[dict[str, Any]]] = defaultdict(list)
    by_f1: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        by_f1[row["fingerprints"]["F1"]].append(row)
        by_f2[row["fingerprints"]["F2"]].append(row)
        by_f3[row["fingerprints"]["F3"]].append(row)

    quality_sensitive: list[dict[str, Any]] = []
    for digest, members in sorted(by_f2.items()):
        f3_ids = sorted({member["fingerprints"]["F3"] for member in members})
        if len(f3_ids) <= 1:
            continue
        quality_sensitive.append({
            "f2_fingerprint_sha256": digest,
            "source_ids": sorted(member["source_id"] for member in members),
            "member_count": len(members),
            "f3_class_count": len(f3_ids),
            "meaning": "Root-only equivalence would collapse distinct chord-quality sequences.",
        })

    notation_variants: list[dict[str, Any]] = []
    for digest, members in sorted(by_f3.items()):
        f1_ids = sorted({member["fingerprints"]["F1"] for member in members})
        if len(f1_ids) <= 1:
            continue
        notation_variants.append({
            "f3_fingerprint_sha256": digest,
            "source_ids": sorted(member["source_id"] for member in members),
            "member_count": len(members),
            "f1_class_count": len(f1_ids),
            "meaning": "Semantic root+quality equivalence spans distinct source-family/spelling identities.",
        })

    metadata_variants: list[dict[str, Any]] = []
    for digest, members in sorted(by_f1.items()):
        tag_keys = sorted({tags_key(member["tags"]) for member in members})
        if len(members) <= 1 or len(tag_keys) <= 1:
            continue
        metadata_variants.append({
            "f1_fingerprint_sha256": digest,
            "source_ids": sorted(member["source_id"] for member in members),
            "member_count": len(members),
            "tag_variant_count": len(tag_keys),
            "tag_sets": [json.loads(tag_key) for tag_key in tag_keys],
            "meaning": "Same authored progression identity carries differing source metadata; H2 does not choose a winning tag set.",
        })

    return {
        "f2_quality_sensitive_group_count": len(quality_sensitive),
        "f3_source_notation_variant_group_count": len(notation_variants),
        "f1_metadata_variant_group_count": len(metadata_variants),
        "f2_quality_sensitive_groups": quality_sensitive,
        "f3_source_notation_variant_groups": notation_variants,
        "f1_metadata_variant_groups": metadata_variants,
    }


def build_fingerprints(
    normalization: dict[str, Any], *, h1_input_sha256: str | None = None
) -> dict[str, Any]:
    require_h1_shape(normalization)
    tokens = token_lookup(normalization)
    rows: list[dict[str, Any]] = []

    for definition in normalization["definitions"]:
        events = materialize_events(definition, tokens)
        payloads = fingerprint_payloads(definition, events)
        fingerprints = {level: sha256_json(level, payloads[level]) for level in COMPUTED_LEVELS}
        rows.append({
            "source_id": definition["source_id"],
            "source_family": definition["source_family"],
            "notation_class": definition["notation_class"],
            "source_definition_sha256": definition["source_definition_sha256"],
            "tags": definition["tags"],
            "event_count": definition["event_count"],
            "fingerprints": fingerprints,
            "f3_sequence": event_sequence_key(f3_event(event) for event in events),
        })

    rows.sort(key=lambda row: row["source_id"])
    level_reports: dict[str, Any] = {
        level: make_level_report(level, rows) for level in COMPUTED_LEVELS
    }
    for level in ("F4", "F5", "F6"):
        level_reports[level] = {
            "status": "DEFERRED",
            "registry": FINGERPRINT_REGISTRY[level],
            "summary": None,
            "classes": [],
            "duplicate_groups": [],
        }

    compact_rows = [
        {
            "source_id": row["source_id"],
            "source_family": row["source_family"],
            "fingerprints": row["fingerprints"],
        }
        for row in rows
    ]

    return {
        "schema_version": SCHEMA_VERSION,
        "stage": STAGE,
        "source": normalization["source"],
        "h1_dependency": {
            "schema_version": normalization["schema_version"],
            "stage": normalization["stage"],
            "normalized_json_sha256": h1_input_sha256,
            "expected_normalized_json_sha256": EXPECTED_H1_NORMALIZED_SHA256,
            "logical_definition_count": normalization["summary"]["logical_definition_count"],
            "admitted_definition_count": normalization["summary"]["admitted_definition_count"],
            "quarantined_definition_count": normalization["summary"]["quarantined_definition_count"],
            "raw_token_vocabulary_count": normalization["summary"]["raw_token_vocabulary_count"],
        },
        "dedup_contract": {
            "definitions_removed": 0,
            "representative_selection": "NOT_PERFORMED",
            "runtime_admission": "NOT_PERFORMED",
            "source_incidence_to_runtime_weight": "FORBIDDEN",
            "cyclic_rotation_equivalence": "FORBIDDEN",
            "repetition_extension_equivalence": "FORBIDDEN",
            "tags_in_structural_fingerprint": False,
            "next_stage": "H3_FUNCTIONAL_ANALYSIS",
        },
        "fingerprint_registry": {level: FINGERPRINT_REGISTRY[level] for level in ALL_LEVELS},
        "levels": level_reports,
        "cross_level_diagnostics": cross_level_diagnostics(rows),
        "near_relations": near_relations(rows),
        "definitions": compact_rows,
    }


def render_markdown(result: dict[str, Any]) -> str:
    source = result["source"]
    lines = [
        "# Harmony Atlas H2 Structural Fingerprints / Dedup", "",
        "**Status:** generated research evidence / H2 checkpoint  ",
        f"**Source:** `{source['repository']} @ {source['commit']}`  ",
        f"**Evidence class:** `{source['evidence_class']}`  ",
        "**Runtime impact:** none", "",
        "## Dependency boundary", "",
        "H2 consumes the verified H1 normalized representation. It does not re-interpret raw Roman notation and it does not remove source definitions.", "",
        f"Frozen H1 normalized JSON SHA-256: `{result['h1_dependency']['expected_normalized_json_sha256']}`.", "",
        "The F0-F6 namespace is fully registered, but only levels whose prerequisite evidence exists are computed in H2:", "",
        "| Level | Name | Status | Unique classes | Duplicate groups | Surplus duplicates |",
        "|---|---|---|---:|---:|---:|",
    ]
    for level in ALL_LEVELS:
        report = result["levels"][level]
        name = result["fingerprint_registry"][level]["name"]
        if report["status"] == "COMPUTED":
            summary = report["summary"]
            lines.append(
                f"| {level} | {name} | COMPUTED | {summary['unique_class_count']} | "
                f"{summary['duplicate_group_count']} | {summary['duplicate_surplus_count']} |"
            )
        else:
            dependency = ", ".join(result["fingerprint_registry"][level].get("dependency", []))
            lines.append(f"| {level} | {name} | DEFERRED ({dependency}) | — | — | — |")

    lines += [
        "", "## Operational fingerprint meanings", "",
        "```text",
        "F0 SourceIdentity",
        "   source id + family + notation class + exact source-definition SHA-256",
        "",
        "F1 TranspositionInvariant",
        "   family + notation class + exact ordered Roman/rest sequence; source id and tags excluded",
        "",
        "F2 RootSequence",
        "   ordered degree + alteration + rests; quality/family/spelling excluded",
        "",
        "F3 RootQualitySequence",
        "   ordered degree + alteration + semantic quality + rests; source spelling/family excluded",
        "```", "",
        "F3 intentionally uses H1 semantic quality fields rather than raw suffix spelling. Thus notation variants may collapse only when the preserved H1 semantics are equal; ambiguous `7/9` remain distinct from explicit `dom7`/`M7` through `seventh_flavor`.", "",
        "## Deferred levels", "",
    ]
    for level in ("F4", "F5", "F6"):
        registry = result["fingerprint_registry"][level]
        lines.append(f"- **{level} {registry['name']}** — `{registry['status']}`: {registry['reason']}")

    diagnostics = result["cross_level_diagnostics"]
    near = result["near_relations"]
    lines += [
        "", "## Over-collapse diagnostics", "",
        f"- F2 root-only groups that split into multiple F3 quality classes: **{diagnostics['f2_quality_sensitive_group_count']}**.",
        f"- F3 semantic groups spanning multiple F1 source-notation/family identities: **{diagnostics['f3_source_notation_variant_group_count']}**.",
        f"- F1 duplicate groups carrying multiple tag sets: **{diagnostics['f1_metadata_variant_group_count']}**.",
        "", "These are diagnostics, not automatic merge rules.", "",
        "## Forbidden naive equivalence checks", "",
        f"Near-relation **pairs** detected but explicitly not deduplicated: **{near['pair_relation_count']}**.",
        f"Connected near-relation components: **{near['component_count']}**.",
    ]
    for kind, count in near["pair_count_by_kind"].items():
        component_count = near["component_count_by_kind"].get(kind, 0)
        lines.append(f"- `{kind}`: **{count} pair edges**, **{component_count} connected components**")
    if not near["pair_count_by_kind"]:
        lines.append("- none in pinned source")

    if near["components"]:
        lines += ["", "Components (report-only):", ""]
        for component in near["components"][:10]:
            lines.append(
                f"- `{component['component_id']}` — {component['source_count']} definitions / "
                f"{component['pair_edge_count']} pair edges: "
                + ", ".join(f"`{source_id}`" for source_id in component["source_ids"])
            )

    examples = near["relations"][:10]
    if examples:
        lines += ["", "Pair examples (report-only):", ""]
        for row in examples:
            if row["kind"] == "CYCLIC_ROTATION":
                lines.append(
                    f"- `{row['source_a']}` ↔ `{row['source_b']}` — CYCLIC_ROTATION "
                    f"({row['rotation_steps_a_to_b']} steps), **not dedup-equivalent**."
                )
            else:
                lines.append(
                    f"- `{row['shorter_source']}` → `{row['longer_source']}` — REPETITION_EXTENSION "
                    f"(×{row['repetition_factor']}), **not dedup-equivalent**."
                )

    lines += [
        "", "## Dedup policy", "",
        "- every duplicate count names its fingerprint level;",
        "- H2 removes **zero** definitions;",
        "- no representative/canonical winner is selected;",
        "- source tags remain attached to source definitions and do not participate in F1-F3 structural keys;",
        "- cyclic rotations are near-relations, never equality;",
        "- repeated longer forms are near-relations, never equality;",
        "- pair-edge counts are never presented as counts of independent near-duplicate families;",
        "- source incidence never becomes runtime probability;",
        "- F4/F5/F6 remain unavailable until their prerequisite research stages exist;",
        "- production vocabulary admission remains a later human-reviewed R2 step.",
        "", "Next stage: **H3 functional analysis**. H3 may enable F4, but must not retroactively rewrite F0-F3 identities.", "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Build Harmony Atlas F0-F6 fingerprint registry and H2 equivalence reports")
    parser.add_argument("--normalization", type=Path, required=True, help="Frozen H1 normalized JSON")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--markdown-output", type=Path)
    args = parser.parse_args()

    normalization_bytes = args.normalization.read_bytes()
    h1_digest = verify_h1_json_bytes(normalization_bytes)
    normalization = json.loads(normalization_bytes.decode("utf-8"))
    result = build_fingerprints(normalization, h1_input_sha256=h1_digest)
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
