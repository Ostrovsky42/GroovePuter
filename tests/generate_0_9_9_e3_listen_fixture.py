#!/usr/bin/env python3
import argparse
import csv
import hashlib
import os
import re
import subprocess
import tempfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EXPECTED = {
    "e3r_b_graph_summary.csv": "9c9d3983f456e8ef3ffe19422c08cba0dbaafcb9470ab4f1d33d9b6482b98ed1",
    "e3r_b_graph_summary.json": "bbad8865638cc3dc620680b806cd4a6c00da13fb1f335383b6be0d569356abe5",
    "e3r_b_review_corpus.csv": "6216accb1d399dfe8d909646980b0cfee04fcb63d2dd4a88535cc52a6217fd7d",
    "e3r_b_review_corpus.md": "edf2b8c0bf2bec8944648870be156fa237243a24fe0f8fa7fb6d6dc985f23ecb",
}

GRAPH_SOURCES = (
    "src/generation/generation_context.cpp",
    "src/generation/rhythm/rhythm_catalog.cpp",
    "src/generation/rhythm/relationship_resolver.cpp",
    "src/generation/rhythm/rhythm_realizer.cpp",
    "src/generation/rhythm/rhythm_realizer_evolution.cpp",
    "src/generation/rhythm/rhythm_canonical_diff.cpp",
    "src/generation/rhythm/reference_vocabulary.cpp",
)

ROLES = (
    "Kick", "Backbeat", "ClosedHat", "OpenHat",
    "Percussion", "BassRhythm", "ChordRhythm", "MelodicRhythm",
)
ROLE_INDEX = {name: index for index, name in enumerate(ROLES)}
FAMILY_INDEX = {
    "StraightFour": 0,
    "OffbeatPulse": 1,
    "Breakbeat": 2,
    "HalfTime": 3,
    "Sparse": 4,
    "Rolling": 5,
}
MASK_LABELS = ("S", "Q", "G", "SH", "HE", "TI", "A")
ROLE_RE = re.compile(
    r"([A-Za-z]+)\{"
    r"S\[([0-9,\-]+)\] Q\[([0-9,\-]+)\] G\[([0-9,\-]+)\] "
    r"SH\[([0-9,\-]+)\] HE\[([0-9,\-]+)\] TI\[([0-9,\-]+)\] "
    r"A\[([0-9,\-]+)\]\}"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def compile_graph(output: Path) -> None:
    cxx = os.environ.get("CXX", "g++")
    cmd = [
        cxx, "-std=c++17", "-O2",
        "-Wall", "-Wextra", "-Werror", "-Wvla",
        "-Wno-c++20-extensions", "-Wno-unused-but-set-variable",
        f"-I{ROOT}",
    ]
    cmd.extend(str(ROOT / source) for source in GRAPH_SOURCES)
    cmd.extend((
        str(ROOT / "tools/research/e3r_b_drop_displace_graph.cpp"),
        "-o", str(output),
    ))
    subprocess.run(cmd, check=True)


def reproduce_corpus(out_dir: Path) -> Path:
    binary = out_dir / "e3r_b_graph"
    raw = out_dir / "e3r_b_graph.raw"
    report = out_dir / "report"
    compile_graph(binary)
    with raw.open("w", encoding="utf-8") as handle:
        subprocess.run([str(binary)], check=True, stdout=handle)
    subprocess.run(
        [
            "python3", str(ROOT / "tools/research/e3r_b_report.py"),
            "--input", str(raw),
            "--out-dir", str(report),
            "--frozen-summary", str(ROOT / "tests/data/v0r_e2_variant_graph_summary.csv"),
        ],
        check=True,
    )
    for name, expected in EXPECTED.items():
        actual = sha256(report / name)
        if actual != expected:
            raise AssertionError(
                f"frozen E3R-B artifact mismatch {name}: expected={expected} actual={actual}"
            )
    return report / "e3r_b_review_corpus.csv"


def steps_to_mask(text: str) -> int:
    if text == "-":
        return 0
    result = 0
    for item in text.split(","):
        step = int(item)
        if step < 0 or step > 15:
            raise AssertionError(f"invalid logical step {step}")
        result |= 1 << (15 - step)
    return result


def parse_plan(text: str):
    roles = [[0] * 7 for _ in ROLES]
    if text == "<empty>":
        return roles
    consumed = []
    for match in ROLE_RE.finditer(text):
        role_name = match.group(1)
        if role_name not in ROLE_INDEX:
            raise AssertionError(f"unknown role in frozen plan: {role_name}")
        values = [steps_to_mask(value) for value in match.groups()[1:]]
        roles[ROLE_INDEX[role_name]] = values
        consumed.append(match.group(0))
    normalized = " | ".join(consumed)
    if normalized != text:
        raise AssertionError(f"could not parse frozen plan exactly:\n{text}\nparsed:\n{normalized}")
    return roles


def cpp_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit_plan(lines, plan, indent="    "):
    lines.append(indent + "{")
    for role in plan:
        values = ", ".join(f"0x{value:04x}u" for value in role)
        lines.append(indent + f"  {{{values}}},")
    lines.append(indent + "},")


def generate_header(rows) -> str:
    categories = Counter(row["category"] for row in rows)
    roles = Counter(row["role"] for row in rows)
    if len(rows) != 32:
        raise AssertionError(f"expected 32 review cases, got {len(rows)}")
    if categories != Counter({"DROP": 12, "DISPLACE": 12, "COMBINED": 8}):
        raise AssertionError(f"review category drift: {dict(categories)}")
    expected_roles = Counter({"ClosedHat": 15, "BassRhythm": 15, "Kick": 2})
    if roles != expected_roles:
        raise AssertionError(
            f"frozen mutated-role matrix moved: expected={dict(expected_roles)} "
            f"actual={dict(roles)}"
        )
    unsupported = sorted(
        role for role in roles
        if role not in {"Kick", "Backbeat", "ClosedHat", "OpenHat", "Percussion", "BassRhythm"}
    )
    if unsupported:
        raise AssertionError(
            "E3 LISTEN has no approved physical review boundary for mutated roles: "
            + ", ".join(unsupported)
        )

    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace GroovePuterRhythm {",
        "namespace E3ListenFixtureData {",
        "",
        "struct RolePlanMasks {",
        "  uint16_t structural;",
        "  uint16_t secondary;",
        "  uint16_t ghosts;",
        "  uint16_t shortGate;",
        "  uint16_t heldGate;",
        "  uint16_t tieGate;",
        "  uint16_t accents;",
        "};",
        "",
        "struct PlanMasks {",
        "  RolePlanMasks roles[8];",
        "};",
        "",
        "struct CaseMeta {",
        "  const char* caseId;",
        "  const char* category;",
        "  const char* family;",
        "  uint8_t familyIndex;",
        "  uint8_t level;",
        "  const char* operation;",
        "  const char* role;",
        "  uint8_t roleIndex;",
        "  uint8_t sourceStep;",
        "  uint8_t targetStep;",
        "  const char* sourceClass;",
        "  const char* sourceKind;",
        "  uint8_t distance;",
        "  bool sourceAccented;",
        "  bool sourceCanonicalAnchor;",
        "  const char* canonicalDiff;",
        "  uint8_t densityBefore;",
        "  uint8_t densityAfter;",
        "  bool mutatedRoleExact;",
        "};",
        "",
        "inline constexpr uint8_t kCaseCount = 32;",
        "inline constexpr uint8_t kBassRhythmCaseCount = 15;",
        "",
        "inline constexpr CaseMeta kCases[kCaseCount] = {",
    ]

    parsed = []
    for row in rows:
        level = 1 if row["level"] == "P2" else 2 if row["level"] == "P3" else None
        if level is None:
            raise AssertionError(f"unexpected level {row['level']}")
        target = 0xFF if int(row["target_step"]) < 0 else int(row["target_step"])
        exact = row["role"] != "BassRhythm"
        lines.append(
            "  {"
            + ", ".join(
                (
                    cpp_string(row["case_id"]),
                    cpp_string(row["category"]),
                    cpp_string(row["archetype"]),
                    str(FAMILY_INDEX[row["archetype"]]),
                    str(level),
                    cpp_string(row["operation"]),
                    cpp_string(row["role"]),
                    str(int(row["role_index"])),
                    str(int(row["source_step"])),
                    str(target),
                    cpp_string(row["source_class"]),
                    cpp_string(row["source_kind"]),
                    str(int(row["distance"])),
                    "true" if int(row["source_accented"]) else "false",
                    "true" if int(row["source_canonical_anchor"]) else "false",
                    cpp_string(row["canonical_relative_diff"]),
                    str(int(row["density_before"])),
                    str(int(row["density_after"])),
                    "true" if exact else "false",
                )
            )
            + "},"
        )
        triplet = (
            parse_plan(row["canonical_C"]),
            parse_plan(row["before_V"]),
            parse_plan(row["candidate_W"]),
        )
        # The current production physical path has no arbitrary
        # RhythmPhrasePlan -> ChordRhythm/MelodicRhythm rendering boundary.
        # E3 LISTEN therefore fails closed if the frozen C/V/W case would
        # require one. Bass is handled separately as an explicitly labelled
        # production-context operation audition.
        for deferred_role in ("ChordRhythm", "MelodicRhythm"):
            index = ROLE_INDEX[deferred_role]
            if not (triplet[0][index] == triplet[1][index] == triplet[2][index]):
                raise AssertionError(
                    f"{row['case_id']} changes {deferred_role}; "
                    "no approved E3 LISTEN physical boundary exists"
                )
        parsed.append(triplet)
    lines.extend(("};", ""))

    for variant_index, name in enumerate(("kCanonical", "kBefore", "kAfter")):
        lines.append(f"inline constexpr PlanMasks {name}[kCaseCount] = {{")
        for triplet in parsed:
            emit_plan(lines, triplet[variant_index], "  ")
        lines.extend(("};", ""))

    lines.extend((
        "}  // namespace E3ListenFixtureData",
        "}  // namespace GroovePuterRhythm",
        "",
    ))
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Reproduce frozen E3R-B cap=1 corpus and emit E3 LISTEN fixture header."
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="grooveputer-e3-listen-corpus-") as temp_name:
        corpus = reproduce_corpus(Path(temp_name))
        with corpus.open(newline="", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle))

    header = generate_header(rows)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(header, encoding="utf-8")
    print("E3 LISTEN fixture: frozen cap=1 corpus verified")
    print("E3 LISTEN cases: 32 (DROP=12 DISPLACE=12 COMBINED=8 BassRhythm=15)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
