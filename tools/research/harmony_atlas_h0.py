#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import hashlib
import json
import re
from pathlib import Path
from typing import Any

SCHEMA_VERSION = "1.0.0"
SOURCE_REPOSITORY = "ldrolez/free-midi-chords"
PINNED_SOURCE_COMMIT = "baf0896694de6b09ac00250722f2414202e668ed"
EVIDENCE_CLASS = "EDITORIAL_CATALOG_EVIDENCE"
EXPECTED_BLOB_SHA1 = {
    "README.md": "41a5ad11e13efb788dc9c1c1b525fce029825ca7",
    "chords.py": "33c73d789dfbce449654e1dfa9561923b0b3db12",
    "gen.py": "55ff32d43f8c3754b807530e3a49240f8c5174b4",
}
PROGRESSION_ASSIGNMENTS = {
    "Major": "prog_maj",
    "Minor": "prog_min",
    "Modal": "prog_modal",
}
CHORD_TYPE_ASSIGNMENTS = {
    "major_third": "chord_types_maj",
    "minor_third": "chord_types_min",
}
STRUCTURAL_TAGS = {"Cadence"}
CATALOG_TAGS = {"New"}
ROMAN_TOKEN_RE = re.compile(
    r"^(?P<accidental>[b#]?)(?P<roman>VII|III|VI|IV|II|V|I|vii|iii|vi|iv|ii|v|i)"
    r"(?P<suffix>[A-Za-z0-9+\-]*)$"
)


class AuditError(RuntimeError):
    pass


def git_blob_sha1(data: bytes) -> str:
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


def verify_pinned_files(source_root: Path) -> dict[str, str]:
    observed: dict[str, str] = {}
    for relative, expected in EXPECTED_BLOB_SHA1.items():
        path = source_root / relative
        if not path.is_file():
            raise AuditError(f"missing pinned source file: {relative}")
        digest = git_blob_sha1(path.read_bytes())
        observed[relative] = digest
        if digest != expected:
            raise AuditError(
                f"pinned source mismatch for {relative}: expected {expected}, got {digest}"
            )
    return observed


def literal_assignments(path: Path) -> dict[str, Any]:
    try:
        tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    except (OSError, SyntaxError) as exc:
        raise AuditError(f"cannot parse {path}: {exc}") from exc

    result: dict[str, Any] = {}
    for node in tree.body:
        name: str | None = None
        value_node: ast.AST | None = None
        if isinstance(node, ast.Assign) and len(node.targets) == 1:
            target = node.targets[0]
            if isinstance(target, ast.Name):
                name = target.id
                value_node = node.value
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            name = node.target.id
            value_node = node.value
        if name is None or value_node is None:
            continue
        try:
            result[name] = ast.literal_eval(value_node)
        except (ValueError, TypeError):
            # H0 intentionally does not execute source code. Dynamic assignments
            # remain unavailable rather than gaining hidden behavior.
            continue
    return result


def require_string_list(assignments: dict[str, Any], name: str) -> list[str]:
    value = assignments.get(name)
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise AuditError(f"{name} must be a literal list[str]")
    return value


def require_key_pairs(assignments: dict[str, Any]) -> list[tuple[str, str]]:
    value = assignments.get("keys")
    if not isinstance(value, list):
        raise AuditError("keys must be a literal list")
    pairs: list[tuple[str, str]] = []
    for item in value:
        if (
            not isinstance(item, tuple)
            or len(item) != 2
            or not all(isinstance(part, str) for part in item)
        ):
            raise AuditError(f"invalid key pair: {item!r}")
        pairs.append((item[0], item[1]))
    return pairs


def split_progression(definition: str) -> tuple[str, list[str]]:
    if " =" not in definition:
        return definition, []
    progression, descriptor = definition.split(" =", 1)
    return progression, [token for token in descriptor.split() if token]


def progression_tokens(progression: str) -> list[str]:
    # Mirror the source generator's exact double-space rest convention without
    # performing any harmonic normalization.
    expanded = re.sub(r"  ", " X ", progression)
    return [token for token in expanded.split(" ") if token]


def lexical_chord_token(token: str) -> tuple[str, str, str] | None:
    match = ROMAN_TOKEN_RE.fullmatch(token)
    if match is None:
        return None
    accidental = match.group("accidental")
    roman = match.group("roman")
    suffix = match.group("suffix")
    return accidental, roman, suffix


def build_audit(source_root: Path, *, verify_pin: bool = True) -> dict[str, Any]:
    source_root = source_root.resolve()
    chords_path = source_root / "chords.py"
    gen_path = source_root / "gen.py"
    if not chords_path.is_file() or not gen_path.is_file():
        raise AuditError("source root must contain chords.py and gen.py")

    blob_sha1 = verify_pinned_files(source_root) if verify_pin else {}
    chord_assignments = literal_assignments(chords_path)
    gen_assignments = literal_assignments(gen_path)

    progressions = {
        family: require_string_list(chord_assignments, assignment)
        for family, assignment in PROGRESSION_ASSIGNMENTS.items()
    }
    declared_chord_types = {
        family: require_string_list(chord_assignments, assignment)
        for family, assignment in CHORD_TYPE_ASSIGNMENTS.items()
    }
    key_pairs = require_key_pairs(gen_assignments)
    styles = require_string_list(gen_assignments, "styles")

    descriptor_tokens: set[str] = set()
    mood_tags: set[str] = set()
    structural_tags: set[str] = set()
    catalog_tags: set[str] = set()
    raw_chord_tokens: set[str] = set()
    raw_suffixes: set[str] = set()
    altered_roots: set[str] = set()
    unclassified: set[str] = set()
    definitions_with_new = 0
    definitions_with_cadence = 0
    explicit_rest_markers = 0

    for definitions in progressions.values():
        for definition in definitions:
            progression, descriptors = split_progression(definition)
            descriptor_set = set(descriptors)
            descriptor_tokens.update(descriptors)
            structural_tags.update(descriptor_set & STRUCTURAL_TAGS)
            catalog_tags.update(descriptor_set & CATALOG_TAGS)
            mood_tags.update(descriptor_set - STRUCTURAL_TAGS - CATALOG_TAGS)
            if "New" in descriptor_set:
                definitions_with_new += 1
            if "Cadence" in descriptor_set:
                definitions_with_cadence += 1

            explicit_rest_markers += progression.count("  ")
            for token in progression_tokens(progression):
                if token == "X":
                    continue
                raw_chord_tokens.add(token)
                parsed = lexical_chord_token(token)
                if parsed is None:
                    unclassified.add(token)
                    continue
                accidental, roman, suffix = parsed
                raw_suffixes.add(suffix)
                if accidental:
                    # H0 inventories altered scale-degree classes only. Roman
                    # case/quality remains preserved in raw_chord_tokens and is
                    # deliberately not interpreted until H1.
                    altered_roots.add(f"{accidental}{roman.upper()}")

    family_counts = {family: len(items) for family, items in progressions.items()}
    logical_count = sum(family_counts.values())
    style_names = ["default" if style == "" else style for style in styles]
    materializations_per_progression = len(key_pairs) * len(styles)
    projected_by_family = {
        family: count * materializations_per_progression
        for family, count in family_counts.items()
    }

    chord_type_union = sorted(
        set(declared_chord_types["major_third"])
        | set(declared_chord_types["minor_third"])
    )

    return {
        "schema_version": SCHEMA_VERSION,
        "source": {
            "repository": SOURCE_REPOSITORY,
            "commit": PINNED_SOURCE_COMMIT,
            "evidence_class": EVIDENCE_CLASS,
            "verified_blob_sha1": blob_sha1,
        },
        "generation": {
            "key_pair_count": len(key_pairs),
            "key_pairs": [[major, minor] for major, minor in key_pairs],
            "style_count": len(styles),
            "styles": style_names,
            "materializations_per_logical_progression": materializations_per_progression,
        },
        "progressions": {
            "logical_definition_count": logical_count,
            "by_family": family_counts,
            "projected_materialization_count": sum(projected_by_family.values()),
            "projected_materializations_by_family": projected_by_family,
            "descriptor_vocabulary": sorted(descriptor_tokens),
            "mood_tag_vocabulary": sorted(mood_tags),
            "structural_tag_vocabulary": sorted(structural_tags),
            "catalog_tag_vocabulary": sorted(catalog_tags),
            "definitions_tagged_new": definitions_with_new,
            "definitions_tagged_cadence": definitions_with_cadence,
            "raw_chord_token_unique_count": len(raw_chord_tokens),
            "raw_suffix_vocabulary": sorted(raw_suffixes),
            "altered_degree_classes": sorted(altered_roots),
            "explicit_double_space_rest_markers": explicit_rest_markers,
            "lexically_unclassified_tokens": sorted(unclassified),
        },
        "declared_chord_types": {
            "major_third_count": len(declared_chord_types["major_third"]),
            "major_third": sorted(declared_chord_types["major_third"]),
            "minor_third_count": len(declared_chord_types["minor_third"]),
            "minor_third": sorted(declared_chord_types["minor_third"]),
            "union_count": len(chord_type_union),
            "union": chord_type_union,
        },
        "methodology": {
            "canonical_unit": "LogicalProgressionDefinition",
            "generated_file_is_independent_observation": False,
            "catalog_incidence_is_musical_popularity": False,
            "runtime_weight_from_file_count": "FORBIDDEN",
            "next_stage": "H1_CANONICAL_PARSER_NORMALIZATION",
        },
    }


def render_markdown(audit: dict[str, Any]) -> str:
    source = audit["source"]
    generation = audit["generation"]
    progressions = audit["progressions"]
    chord_types = audit["declared_chord_types"]
    family = progressions["by_family"]
    projected = progressions["projected_materializations_by_family"]

    def code_list(values: list[str]) -> str:
        return ", ".join(f"`{value or '<empty>'}`" for value in values)

    lines = [
        "# Harmony Atlas H0 Source Audit",
        "",
        "**Status:** generated research evidence / H0 complete  ",
        f"**Source:** `{source['repository']} @ {source['commit']}`  ",
        f"**Evidence class:** `{source['evidence_class']}`  ",
        "**Runtime impact:** none",
        "",
        "## Result",
        "",
        "The pinned source is a generated editorial catalog. The canonical harmonic unit is a logical progression definition, not a generated MIDI file.",
        "",
        "| Item | Count |",
        "|---|---:|",
        f"| Logical progressions | {progressions['logical_definition_count']} |",
        f"| Major | {family['Major']} |",
        f"| Minor | {family['Minor']} |",
        f"| Modal | {family['Modal']} |",
        f"| Key pairs | {generation['key_pair_count']} |",
        f"| Rhythm/style materializations | {generation['style_count']} |",
        f"| Materializations per logical progression | {generation['materializations_per_logical_progression']} |",
        f"| Projected progression MIDI materializations | {progressions['projected_materialization_count']} |",
        f"| Major projected materializations | {projected['Major']} |",
        f"| Minor projected materializations | {projected['Minor']} |",
        f"| Modal projected materializations | {projected['Modal']} |",
        "",
        "The 190 logical definitions therefore project to 11,400 progression MIDI files before counting the repository's separate triad/extended-chord material. Generated file count is not musical popularity.",
        "",
        "## Source dimensions",
        "",
        f"Styles: {code_list(generation['styles'])}",
        "",
        f"Descriptor vocabulary ({len(progressions['descriptor_vocabulary'])}): {code_list(progressions['descriptor_vocabulary'])}",
        "",
        f"Mood tags ({len(progressions['mood_tag_vocabulary'])}): {code_list(progressions['mood_tag_vocabulary'])}",
        "",
        f"Structural tags: {code_list(progressions['structural_tag_vocabulary'])}",
        "",
        f"Catalog tags: {code_list(progressions['catalog_tag_vocabulary'])}",
        "",
        f"Definitions tagged `New`: **{progressions['definitions_tagged_new']}**. Definitions tagged `Cadence`: **{progressions['definitions_tagged_cadence']}**.",
        "",
        "## Lexical inventory",
        "",
        f"Unique raw progression chord tokens: **{progressions['raw_chord_token_unique_count']}**.",
        "",
        f"Raw progression suffix vocabulary: {code_list(progressions['raw_suffix_vocabulary'])}",
        "",
        f"Altered degree classes: {code_list(progressions['altered_degree_classes'])}",
        "",
        f"Explicit double-space rest markers: **{progressions['explicit_double_space_rest_markers']}**.",
        "",
        f"Lexically unclassified progression tokens: {code_list(progressions['lexically_unclassified_tokens']) if progressions['lexically_unclassified_tokens'] else '**none**'}.",
        "",
        "This is lexical inventory only. H0 does not claim semantic equivalence between suffix spellings and does not normalize Roman-numeral function.",
        "",
        "## Declared source chord-type catalog",
        "",
        f"Major-third suffixes: **{chord_types['major_third_count']}**. Minor-third suffixes: **{chord_types['minor_third_count']}**. Union: **{chord_types['union_count']}**.",
        "",
        f"Major-third: {code_list(chord_types['major_third'])}",
        "",
        f"Minor-third: {code_list(chord_types['minor_third'])}",
        "",
        "## H0 gate",
        "",
        "- source revision and critical source blobs are pinned;",
        "- all logical progression families are enumerable;",
        "- generated key/style multiplicity is separated from logical support;",
        "- descriptor/tag vocabulary is inventoried and typed;",
        "- source-declared chord-type vocabulary is inventoried;",
        "- altered-degree classes are preserved as lexical evidence while raw spellings remain available in raw tokens;",
        "- lexical anomalies are explicit rather than silently normalized;",
        "- no production generation, Tonal Projector, Scene, Genre or UI code is changed.",
        "",
        "Next stage: **H1 canonical parser / loss-aware normalization**. H1 must preserve altered degrees and chord quality, but it is not part of this checkpoint.",
        "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Audit pinned Harmony Atlas source shape")
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--markdown-output", type=Path)
    args = parser.parse_args()

    audit = build_audit(args.source_root, verify_pin=True)
    json_text = json.dumps(audit, indent=2, sort_keys=True) + "\n"
    markdown_text = render_markdown(audit)

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
