#!/usr/bin/env python3
"""D1-C source audit: P0 generated Synth A preservation evaluator.

The evaluator is an observational one-sided provider. It must never emit Fail
into the lineage adapter, never read legacy classification authority, never
own publication, and never mutate origin evidence.
"""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


EVAL = read("src/dsp/p0_preservation_evaluator.h")
SEM = read("src/dsp/development_semantics.h")
ADAPTER = read("src/dsp/development_semantic_adapter.h")
ENGINE_H = read("src/dsp/miniacid_engine.h")
ENGINE_CPP = read("src/dsp/miniacid_engine.cpp")
ORIGIN = read("src/state/generated_synth_a_origin.h")
TYPES = read("src/dsp/p0_preservation_types.h")

failures: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def block(text: str, start: str, end: str) -> str:
    a = text.find(start)
    b = text.find(end, a + 1) if a >= 0 else -1
    return text[a:b] if a >= 0 and b > a else ""


# 1. One-sided law: the aggregate is Pass or Unknown, never Fail.
require("allPass ? ClaimEvidenceStatus::Pass : ClaimEvidenceStatus::Unknown" in EVAL,
        "D1-C: lineageSummary must be exactly allPass ? Pass : Unknown")
for match in re.finditer(r"(lineageSummary|lineagePreservation)\s*=\s*([^;]*);", EVAL):
    require("ClaimEvidenceStatus::Fail" not in match.group(2),
            f"D1-C firewall: P0 aggregation assigns Fail to {match.group(1)}: {match.group(0).strip()}")
require("ClaimEvidenceStatus::Fail" not in
        block(EVAL, "inline SemanticFacts adaptP0Preservation(", "}  // namespace GroovePuterDevelopmentSemantic"),
        "D1-C firewall: provider seam must never carry Fail into the D0-C adapter")

# 1b. The engine header names only the light value types (no include cycle).
require('#include "src/dsp/p0_preservation_types.h"' in ENGINE_H and
        "p0_preservation_evaluator.h" not in ENGINE_H,
        "D1-C: engine header must include only p0_preservation_types.h")
require("sizeof(P0PreservationAssessment) <= 8" in TYPES,
        "D1-C: assessment budget assertion missing")

# 2. The generic D0-C carrier is untouched (restriction is provider-specific).
require("NewIdea," in SEM, "D1-C: generic LineageStatus::NewIdea must remain in the D0-C carrier")
require(re.search(r"case ClaimEvidenceStatus::Fail:\s*return LineageStatus::NewIdea;", ADAPTER) is not None,
        "D1-C: D0-C adapter lineageFromClaims must be unchanged")

# 3. No legacy classification authority in the evaluator.
for token in ("IdeaClassification", "GenreResult", "DevelopmentDisposition", "classification",
              "pitchesChanged", "onsetsChanged", "anchorPitchDiff", "anchorOnsetDiff",
              "mutation", "similarity", "fingerprint", "sourceAnchorSnapshot",
              "HarmonicRootPreservation", "ReferenceRole::Source,", "ReferenceRole::ReturnTarget"):
    require(token not in EVAL, f"D1-C firewall: evaluator must not use {token!r}")
require("Sem::LineageStatus::NewIdea" not in EVAL and "LineageStatus::NewIdea" not in EVAL,
        "D1-C firewall: P0 provider must never name NewIdea")

# 4. Owner-derived evidence + authoritative projection owner (no second settings path).
require("bassRhythm.onsets" in EVAL and "bassPitchClasses" in EVAL,
        "D1-C: R2/R3 must be driven by origin bassRhythm and bassPitchClasses")
require("projectPatternToRuntimeEventsWithSourceSteps" in ENGINE_CPP,
        "D1-C: source steps must come from the existing projection owner")
impl = block(ENGINE_CPP, "bool MiniAcid::acquireWorkingMelodySourceImpl_(", "MiniAcid::GoRequestResult MiniAcid::requestGoNextMaterial")
require(impl.count("PatternProjectionSettings settings{}") == 1,
        "D1-C: exactly one shared Pattern->Runtime settings path")
observe = block(ENGINE_CPP, "void MiniAcid::observeP0Preservation_(", "MiniAcid::NextPrepareResult MiniAcid::growWorkingMaterial(")
require("PatternProjectionSettings" not in observe and "projectPattern" not in observe,
        "D1-C: observation must not build a second projection")
require(") const {" in observe.split("{", 1)[0] + ") const {",
        "D1-C: observation must be const")
for token in ("developmentLineage_[", "pendingMaterial_[", "hasSourceAnchorSnapshot_[",
              "sourceAnchorSnapshot_[", "clearGeneratedSynthAOrigin", "publishGeneratedSynthAOrigin",
              "prepareNextMelody"):
    require(token not in observe, f"D1-C: observation must not touch {token!r}")

# 5. Publication path unchanged: NEXT preparation ignores the observation.
develop = block(ENGINE_CPP, "MiniAcid::NextPrepareResult MiniAcid::developWorkingMaterial(", "void MiniAcid::observeP0Preservation_(")
require("return prepareNextMelody(\n      idx, dev.candidate, basis, dev.classification.idea);" in develop,
        "D1-C: developWorkingMaterial must publish exactly as before")
tail = develop[develop.find("return prepareNextMelody("):]
require("semanticOut" not in tail and "observeP0" not in tail,
        "D1-C: publication call must not depend on the observation")
require("GroovePuterDevelopment::developCandidate(sourceBuffer, request, anchorPtr)" in develop,
        "D1-C: candidate generation must be unchanged")
require("DevelopmentSemanticObservation* semanticOut = nullptr" in ENGINE_H,
        "D1-C: semantic output must be optional (default nullptr)")

# 6. Origin stays immutable evidence; claim status is transient.
require("ClaimEvidenceStatus" not in ORIGIN and "P0PreservationAssessment" not in ORIGIN,
        "D1-C: claim status must not be stored in the origin sidecar")
require("const GroovePuterMaterial::GeneratedSynthABarOrigin& origin" in EVAL,
        "D1-C: evaluator must take origin by const reference")

# 7. First-hop scope only.
require("basis.kind == GroovePuterMaterial::MaterialKind::Pattern" in observe and
        "idx == 0" in observe and "holdsMelody()" in observe,
        "D1-C: scope must be Synth A + CURRENT Pattern (Melody unsupported)")
require("currentChangedSinceOrigin" in TYPES and "basis.version != origin->originPatternVersion" in observe,
        "D1-C: version difference is evidence only")
require("originPatternVersion ==" not in EVAL and "originPatternVersion !=" not in EVAL,
        "D1-C: exact PatternVersion equality must not gate the evaluator")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    print(f"0.9.14 D1-C source regressions: FAIL ({len(failures)} issues)")
    sys.exit(1)
print("0.9.14 D1-C source regressions: PASS")
