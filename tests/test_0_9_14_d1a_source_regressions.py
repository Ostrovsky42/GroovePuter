#!/usr/bin/env python3
"""D1-A source audit: verify Material identity closure and source firewall invariants.

D1-A implements ONLY the Material-identity prerequisites discovered by D0-F.
Provenance sidecars, preservation evaluators, and Development semantics changes
remain strictly forbidden.
"""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

SLOT = read("src/state/material_slot.h")
SLOT_ACCESS = read("src/state/material_slot_access.h")
IDENTITY = read("src/state/material_identity.h")
PAGING_H = read("src/audio/pattern_paging.h")
PAGING_CPP = read("src/audio/pattern_paging.cpp")
PHRASE_GEN = read("src/dsp/phrase_generator.h")
SONG = read("src/dsp/generated_phrase_song.h")
LINEAGE = read("src/state/material_lineage.h")
ENGINE_CPP = read("src/dsp/miniacid_engine.cpp")
ENGINE_H = read("src/dsp/miniacid_engine.h")

failures: list[str] = []

def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)

# Checkpoint A0: Descriptor-aware safe slot predicate
require("residentSlotIsFree" in PHRASE_GEN and
        "kMaterialVoices" in PHRASE_GEN,
        "D1-A A0: PhraseGenerator::localSlotIsSafeForPhrase must check residentSlotIsFree for all material voices")
require("isCanonicalFree" in SLOT or "isCanonicalFree" in SLOT_ACCESS,
        "D1-A A0: canonical free descriptor helper missing")
require("bool isFree()" in SLOT or "isFree() const" in SLOT,
        "D1-A A0: MaterialSlotDescriptor.isFree() missing")

# Checkpoint A1-A4: Batch MaterialId reservation
require("struct MaterialIdReservation" in IDENTITY,
        "D1-A A1: MaterialIdReservation value object missing from material_identity.h")
require("valid()" in IDENTITY and "idAt(" in IDENTITY,
        "D1-A A1: MaterialIdReservation must provide valid() and idAt()")
require("reserveMaterialIds(uint8_t count)" in PAGING_H,
        "D1-A A1: PatternPagingService::reserveMaterialIds declaration missing")
require("PatternPagingService::reserveMaterialIds(" in PAGING_CPP,
        "D1-A A1: PatternPagingService::reserveMaterialIds implementation missing")
require("writeIdentityHighWater" in PAGING_CPP and
        "count > 8" in PAGING_CPP,
        "D1-A A1: reserveMaterialIds must update high-water mark and fail closed for count > 8")
require("reserveMaterialIds(1).first" in PAGING_CPP,
        "D1-A A1: allocateMaterialId must delegate to reserveMaterialIds(1)")

# Carrier in PreparedPhraseArrangement (PMB-P1 bound)
require("synthAReservation" in SONG,
        "D1-A Carrier: PreparedPhraseArrangement missing synthAReservation")
require("static_assert(sizeof(PreparedPhraseArrangement) <= 1024" in SONG,
        "D1-A Carrier: PMB-P1 1024 B bound assertion missing")

# Publication in applyPreparedPersistent
require("setResidentDescriptor" in SONG,
        "D1-A Commit: applyPreparedPersistent must publish Synth A descriptor")
require("clearResidentDescriptor" in SONG,
        "D1-A Undo: restoreUndo must clear Synth A descriptor")

# Pre-commit ID reservation in generate() occurs outside audio mutation critical section
commit_start = SONG.find("template <typename Guard>\nResult generate(")
if commit_start >= 0:
    gen_body = SONG[commit_start:]
    res_pos = gen_body.find("PatternPagingService::reserveMaterialIds")
    commit_pos = gen_body.find("GroovePuterUndo::undoOwner().commitPrepared")
    require(res_pos >= 0 and commit_pos > res_pos,
            "D1-A I/O Boundary: reserveMaterialIds must execute before commitPrepared critical section")
    # F5 invariant: PREPARE -> preparedTargetStillCommitSafe -> reservation ->
    # COMMIT is synchronous under the write lease, so an invalid target must
    # exit before any id is reserved or anything is published.
    safe_pos = gen_body.find("preparedTargetStillCommitSafe(engine, *prepared)")
    require(0 <= safe_pos < res_pos,
            "D1-A F5: commit-safe revalidation must precede id reservation")
    safe_block = gen_body[safe_pos:res_pos]
    require("LifecycleStatus::TargetChanged" in safe_block and "return output;" in safe_block,
            "D1-A F5: invalid target must return TargetChanged before reservation")
    require("applyPreparedPersistent" not in gen_body[:commit_pos],
            "D1-A F5: physical publication must happen only inside commitPrepared")

# Hard ownership rules: PreparationBasis requires valid MaterialId + version
require("return reference.id.valid() && version.valid();" in LINEAGE,
        "D1-A Hard rule violated: PreparationBasis::valid() must require reference.id.valid() && version.valid()")
require("PatternPagingService" in PAGING_H,
        "D1-A Hard rule violated: PatternPagingService must remain canonical MaterialId owner")

# No forbidden provenance sidecars or lineage evaluators leaked in
# ("GeneratedSynthAOrigin" was forbidden here during D1-A; the D1-B gate now
# owns its publication contract.)
forbidden_tokens = (
    "GeneratedSynthADevelopmentOrigin",
    "P0LineageEvaluator",
    "P0PreservationEvaluator",
    "SessionMaterialReference",
    "TransientMaterialId",
    "MaterialSemanticProvenance",
    "LineageEvaluator",
    "DevelopmentCoordinator",
    "ReverseHarmonicAnalysis",
    "StrongRhythmMigrationResult evidence",
)
for token in forbidden_tokens:
    for path in (
        "src/state/material_slot.h",
        "src/state/material_lineage.h",
        "src/dsp/generated_phrase_song.h",
        "src/dsp/miniacid_engine.cpp",
        "src/dsp/miniacid_engine.h",
    ):
        content = read(path)
        require(token not in content, f"D1-A Source Firewall: forbidden '{token}' found in {path}")

if failures:
    for f in failures:
        print(f"FAIL: {f}")
    print(f"0.9.14 D1-A source regressions: FAIL ({len(failures)} issues)")
    sys.exit(1)

print("0.9.14 D1-A source regressions: PASS")
