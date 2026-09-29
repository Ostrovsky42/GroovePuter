#!/usr/bin/env python3
"""D1-C1 source audit: attack vs continuation alignment of the P0 evaluator."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


EVAL = read("src/dsp/p0_preservation_evaluator.h")
failures: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def block(text: str, start: str, end: str) -> str:
    a = text.find(start)
    b = text.find(end, a + 1) if a >= 0 else -1
    return text[a:b] if a >= 0 and b > a else ""


# Distinct masks; the physical skeleton is projection evidence, not "onset topology".
for name in ("attackMask(", "continuationMask(", "physicalSkeleton("):
    require(name in EVAL, f"D1-C1: {name} helper missing")
require("attackMask(origin) | continuationMask(origin)" in EVAL,
        "D1-C1: physicalSkeleton must be attackMask | continuationMask")
require("originPitchClassAt" not in EVAL,
        "D1-C1: continuation pitch inheritance must not be part of R3")

r2 = block(EVAL, "// ---- R2: BASS ATTACK topology", "// ---- R3:")
r3 = block(EVAL, "// ---- R3:", "// ---- One-sided aggregation")
require("(mapped & attacks) == attacks" in r2,
        "D1-C1: R2 must require every origin attack to be present at its own step")
require("stepIn(attacks, sourceSteps[i])) continue;" in r2,
        "D1-C1: R2 candidate tick comparison must skip continuation events")
require("candidate.count != source.count" in r2 and "status_unknown = true" in r2,
        "D1-C1: lost correspondence must be UNKNOWN")
require("unclassifiable" in r2,
        "D1-C1: unclassifiable events must not be given an invented meaning")
require("stepIn(attacks, step)) continue;" in r3,
        "D1-C1: R3 must evaluate attack steps only")
require("physicalSkeleton" not in r3, "D1-C1: R3 must not use the physical skeleton")
# R2 must not fail merely because the physical skeleton differs.
require("mapped == skeleton" not in EVAL and "popcount" not in EVAL,
        "D1-C1: R2 must not require the whole physical skeleton to be unchanged")
# One-sided law unchanged.
require("allPass ? ClaimEvidenceStatus::Pass : ClaimEvidenceStatus::Unknown" in EVAL,
        "D1-C1: aggregation must remain Pass/Unknown")
for match in re.finditer(r"(lineageSummary|lineagePreservation)\s*=\s*([^;]*);", EVAL):
    require("ClaimEvidenceStatus::Fail" not in match.group(2),
            f"D1-C1: aggregate may never be Fail: {match.group(0).strip()}")
require("NewIdea" not in EVAL, "D1-C1: P0 provider must never name NewIdea")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    print(f"0.9.14 D1-C1 source regressions: FAIL ({len(failures)} issues)")
    sys.exit(1)
print("0.9.14 D1-C1 source regressions: PASS")
