#!/usr/bin/env python3
"""D0-B source-level RED contracts for the frozen 0.9.13 classifier.

These checks intentionally fail while semantic authority is still collapsed
inside musical_development.h. They do not require any future production type
or enum spelling.
"""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEV = (ROOT / "src/dsp/musical_development.h").read_text()
LINEAGE = (ROOT / "src/state/material_lineage.h").read_text()

failures: list[str] = []

def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)

def forbid(needle: str, message: str) -> None:
    require(needle not in DEV, message)

# Existing useful ancestry carrier must survive D0-B.
require(
    "sourceAnchorBasis" in LINEAGE and "predecessorBasis" in LINEAGE,
    "D0-B lineage carrier lost SOURCE/PREDECESSOR ancestry",
)

# D0-L1 / D0-L2 / NEW_IDEA firewall.
forbid(
    "anchorPitchDiff && anchorOnsetDiff",
    "D0-B RED: NEW_IDEA is still inferred from pitch+onset difference shortcut",
)
forbid(
    "evidence.harmony.pitchesChanged && evidence.rhythm.onsetsChanged",
    "D0-B RED: pairwise changed-dimensions shortcut still classifies NEW_IDEA",
)

# D0-L4 / B8: unavailable tonal authority must not be encoded as genre failure.
extend_marker = 'failureReason = "EXTEND DEFERRED: NO TONAL ROOT AUTHORITY"'
if extend_marker in DEV:
    pos = DEV.index(extend_marker)
    prefix = DEV[max(0, pos - 180):pos]
    require(
        "classification.genre = GenreResult::Fail" not in prefix,
        "D0-B RED: unavailable tonal-root capability is still stored as GenreResult::Fail",
    )

# D0-L4 / B9: operation conformance must not be encoded as genre status.
contour_marker = "melodic contour broken by register wrap"
if contour_marker in DEV:
    pos = DEV.index(contour_marker)
    prefix = DEV[max(0, pos - 500):pos]
    require(
        "classification.genre = GenreResult::Fail" not in prefix,
        "D0-B RED: operation contour violation is still stored as GenreResult::Fail",
    )

# Semantic evaluation and publication policy must not be one inline decision.
require(
    "DevelopmentDisposition evaluateDisposition" not in DEV,
    "D0-B RED: semantic header still owns omnibus publication disposition",
)
require(
    "classification.genre == GenreResult::Pass" not in DEV
    or "DevelopmentDisposition::Publish" not in DEV,
    "D0-B RED: GenreResult::Pass still directly implies publication",
)

# Permanent negative contract.
for forbidden in (
    "DevelopmentDistance",
    "developmentDistance",
    "SimilarityPercent",
    "similarityPercent",
    "IdeaFingerprint",
    "AdmissibilityResult",
):
    require(
        forbidden not in DEV,
        f"D0-B firewall: forbidden global shortcut introduced: {forbidden}",
    )

if failures:
    for failure in failures:
        print(failure)
    print(f"D0-B source regressions: RED ({len(failures)} normative gaps)")
    sys.exit(1)

print("D0-B source regressions: PASS")