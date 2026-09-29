#!/usr/bin/env python3
"""M0-A source audit: semantic facts stay read-only observations; no semantic
vocabulary reaches the front panel; measurement tooling never enters firmware."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore")


failures: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


SRC = ROOT / "src"
SEMANTIC_TOKENS = ("SemanticFacts", "DevelopmentSemanticObservation", "adaptP0Preservation",
                   "evaluateP0Preservation", "P0PreservationAssessment", "semanticOut")
ALLOWED = {
    "src/dsp/development_semantics.h",
    "src/dsp/development_semantic_adapter.h",
    "src/dsp/p0_preservation_types.h",
    "src/dsp/p0_preservation_evaluator.h",
    "src/dsp/miniacid_engine.h",
    "src/dsp/miniacid_engine.cpp",
}
users = []
for path in SRC.rglob("*"):
    if path.suffix in {".h", ".cpp"} and path.is_file():
        text = read(path)
        if any(token in text for token in SEMANTIC_TOKENS):
            users.append(path.relative_to(ROOT).as_posix())
require(set(users) <= ALLOWED, f"M0-A: semantic facts referenced outside the observation seam: {sorted(set(users) - ALLOWED)}")

# Semantic facts must not reach policy owners.
DEV = read(SRC / "dsp/musical_development.h")
for token in SEMANTIC_TOKENS:
    require(token not in DEV, f"M0-A: DevelopmentDisposition/classification code must not use {token}")
ENGINE_CPP = read(SRC / "dsp/miniacid_engine.cpp")
prepare = ENGINE_CPP[ENGINE_CPP.find("MiniAcid::NextPrepareResult MiniAcid::prepareNextMelody("):]
prepare = prepare[:prepare.find("\nbool MiniAcid::cancelNextMaterial(")] if "\nbool MiniAcid::cancelNextMaterial(" in prepare else prepare[:6000]
for token in SEMANTIC_TOKENS:
    require(token not in prepare, f"M0-A: prepareNextMelody permission must not read {token}")
for path in list((SRC / "ui").rglob("*")) + [SRC / "dsp/genre_manager.h"]:
    if path.is_file() and path.suffix in {".h", ".cpp"}:
        text = read(path)
        for token in SEMANTIC_TOKENS:
            require(token not in text, f"M0-A: UI/Genre code must not use {token} ({path.name})")

# The only call site of the observation: result is not fed into publication.
develop = ENGINE_CPP[ENGINE_CPP.find("MiniAcid::NextPrepareResult MiniAcid::developWorkingMaterial("):]
develop = develop[:develop.find("void MiniAcid::observeP0Preservation_(")]
tail = develop[develop.find("return prepareNextMelody("):]
require("semanticOut" not in tail, "M0-A: publication must not depend on the observation")

# Internal vocabulary is debug/test only: it must not appear as user-facing UI text.
INTERNAL = re.compile(r'"[^"\n]*\b(CONTINUES|UNKNOWN|EXACT|VARIATION|PROVENANCE|PRESERVATION|LINEAGE|ORIGIN WITNESS|PREDECESSOR|CAPABILITY)\b[^"\n]*"')
# Recorded BASELINE (M0-A audit finding, not an endorsement): the legacy NEXT toast
# already prints the old IdeaClassification word. New internal vocabulary must not
# be added to the front panel; this one string is tracked for later redirection.
BASELINE = {("material_development_ux.h", '"NEXT READY: VARIATION"')}
for path in (SRC / "ui").rglob("*"):
    if path.is_file() and path.suffix in {".h", ".cpp"}:
        for match in INTERNAL.finditer(read(path)):
            if (path.name, match.group(0)) in BASELINE:
                continue
            require(False, f"M0-A: NEW internal semantic vocabulary in UI string {match.group(0)!r} ({path.name})")

# Tooling stays host-side.
for path in list(SRC.rglob("*")) + list(ROOT.glob("*.ino")):
    if path.is_file() and path.suffix in {".h", ".cpp", ".ino"}:
        require("tools/m0" not in read(path), f"M0-A: firmware source must not include tools/m0 ({path.name})")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    print(f"0.9.14 M0-A source regressions: FAIL ({len(failures)} issues)")
    sys.exit(1)
print("0.9.14 M0-A source regressions: PASS")
