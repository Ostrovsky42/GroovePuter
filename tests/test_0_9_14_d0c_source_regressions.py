#!/usr/bin/env python3
"""D0-C source firewall for the semantic adapter boundary."""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SEM = (ROOT / "src/dsp/development_semantics.h").read_text()
ADAPTER = (ROOT / "src/dsp/development_semantic_adapter.h").read_text()
LEGACY = (ROOT / "src/dsp/musical_development.h").read_text()

failures: list[str] = []

def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)

# Independent axes must be explicit in the new carrier.
for token in (
    "LineageStatus",
    "ReferenceRole",
    "StateRelation",
    "TrajectoryRole",
    "GenreStatus",
    "OperationConformance",
    "CapabilityStatus",
    "ClaimEvidenceStatus",
    "SemanticFacts",
):
    require(token in SEM, f"D0-C missing semantic axis/carrier: {token}")

# Explicit-reference relation and claim-scoped capability are mandatory.
require("ReferenceRole::ReturnTarget" in SEM,
        "D0-C missing explicit RETURN_TARGET reference role")
require("CapabilityClaim::HarmonicRootPreservation" in SEM,
        "D0-C missing claim-scoped harmonic-root capability")
require("CapabilityClaim::ContourPreservation" in SEM,
        "D0-C missing claim-scoped contour capability")

# Adapter must consume evidence, not legacy verdicts.
for forbidden in (
    "GroovePuterMaterial::IdeaClassification",
    "GroovePuterDevelopment::GenreResult",
    "GroovePuterDevelopment::DevelopmentDisposition",
    "evaluateDisposition(",
    ".classification",
    ".disposition",
):
    require(forbidden not in ADAPTER,
            f"D0-C adapter imported forbidden legacy authority: {forbidden}")

# No scalar/global shortcut may enter the new semantic layer.
for forbidden in (
    "DevelopmentDistance",
    "developmentDistance",
    "SimilarityPercent",
    "similarityPercent",
    "IdeaFingerprint",
    "AdmissibilityResult",
    "admissibility",
):
    require(forbidden not in SEM and forbidden not in ADAPTER,
            f"D0-C forbidden global/scalar shortcut introduced: {forbidden}")

# D0-L5/L6: trajectory and state relation need explicit context/reference.
require("trajectoryContextAvailable" in ADAPTER,
        "D0-C trajectory lacks explicit context availability")
require("RelationAssessmentInput" in ADAPTER and
        "requiredPreservation" in ADAPTER,
        "D0-C state relation lacks explicit reference/preservation input")

# D0-L7: unavailable capability must stay distinct from genre/operation.
require("CapabilityStatus::Unavailable" in ADAPTER,
        "D0-C adapter cannot represent unavailable capability")
require("facts.operation = OperationConformance::Violated" in ADAPTER,
        "D0-C adapter does not separate operation violation")
require("return GenreStatus::Violation" in ADAPTER,
        "D0-C adapter cannot represent explicit genre-requirements failure")
require("MetricAnchorTheOne" not in SEM and
        "MetricAnchorTheOne" not in ADAPTER,
        "D0-D1 overnamed MetricAnchorTheOne claim returned")
require("PrimaryDownbeatOnsetPresence" in SEM and
        "PrimaryDownbeatOnsetPresence" in ADAPTER,
        "D0-D1 truthful downbeat capability claim missing")
require("hasEventOnTheOne" not in ADAPTER,
        "D0-D1 adapter still imports legacy The One helper as semantic authority")
require("genreRequiresTheOne" not in ADAPTER,
        "D0-D1 downbeat observable still drives a genre shortcut")

# The old classifier is deliberately still present; D0-C wraps rather than
# pretending its shortcuts became authoritative.
require("anchorPitchDiff && anchorOnsetDiff" in LEGACY,
        "D0-C unexpectedly rewrote legacy classifier during adapter checkpoint")
require("DevelopmentDisposition evaluateDisposition" in LEGACY,
        "D0-C unexpectedly rewrote publication policy during adapter checkpoint")

if failures:
    for failure in failures:
        print(failure)
    print(f"D0-C source firewall: FAIL ({len(failures)} issues)")
    sys.exit(1)

print("D0-C source firewall: PASS")
