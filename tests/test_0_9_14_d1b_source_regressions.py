#!/usr/bin/env python3
"""D1-B source audit: owner-derived Synth A origin evidence.

D1-B carries evidence only. It must not classify, judge lineage, analyse the
generated Pattern, persist provenance, or become a second Material owner.
"""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


ORIGIN = read("src/state/generated_synth_a_origin.h")
SONG = read("src/dsp/generated_phrase_song.h")
P1R = read("src/dsp/generated_phrase_p1r_materializer.h")
STRONG_H = read("src/generation/migration/strong_rhythm_migration.h")
STRONG_CPP = read("src/generation/migration/strong_rhythm_migration.cpp")
ENGINE_H = read("src/dsp/miniacid_engine.h")
LINEAGE = read("src/state/material_lineage.h")

failures: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def block(text: str, start: str, end: str) -> str:
    a = text.find(start)
    b = text.find(end, a + 1) if a >= 0 else -1
    return text[a:b] if a >= 0 and b > a else ""


# 1. Resolved BassRhythmPlan is exported from its actual owner (the object that
#    feeds the Synth A projection), not regenerated later.
require("BassRhythmPlan bassRhythmPlan{}" in STRONG_H and
        "bool bassRhythmPlanAvailable" in STRONG_H,
        "D1-B: StrongRhythmMigrationResult must export the resolved BassRhythmPlan")
require(re.search(r"result\.bassRhythmPlan\s*=\s*bass\.plan;", STRONG_CPP) is not None,
        "D1-B: exported plan must be the exact `bass.plan` from realizeBassRhythm")
require("projectLegacyPitchPattern(\n        synthA, bass.plan.onsets, bass.plan.continuations" in STRONG_CPP or
        "bass.plan.onsets, bass.plan.continuations, nextSynthA" in STRONG_CPP,
        "D1-B: Synth A must still be built from the same bass.plan")

# 2. P1R one-bar seam forwards evidence only when actually Applied.
require("struct MaterializedSynthABarEvidence" in P1R,
        "D1-B: P1R bar evidence type missing")
seam = block(P1R, "MaterializedSynthABarEvidence& evidence) {", "inline PreparationDisposition prepare(")
require("applied && result.bassRhythmPlanAvailable" in seam and
        "evidence.bassRhythm = result.bassRhythmPlan;" in seam,
        "D1-B: bar evidence must come from the migration result and only when Applied")

# 3. Sidecar type: bounded, evidence-only, no verdict/analysis vocabulary.
require("sizeof(GeneratedSynthAOrigin) <= 352" in ORIGIN,
        "D1-B: sidecar size budget assertion missing")
require("is_trivially_copyable<GeneratedSynthAOrigin>" in ORIGIN,
        "D1-B: sidecar must be trivially copyable")
for token in (
    "IdeaClassification", "GenreResult", "DevelopmentDisposition",
    "LineageEvaluator", "NEW_IDEA", "CONTINUES", "StateRelation",
    "ReturnAssessment", "TrajectoryAssessment", "TrajectoryId",
    "PhraseEvolutionLaw", "PhraseRhythmIdentity", "PhraseSemanticResult",
    "PreparedPhraseExecution", "RuntimeSynthEventBuffer", "similarity",
    "mutationCount", "std::vector", "new ", "malloc",
):
    require(token not in ORIGIN, f"D1-B firewall: forbidden token {token!r} in origin header")

# 4. Publication order: only GeneratedPhraseSong publishes, and only after
#    commitPrepared succeeded.
gen = SONG[SONG.find("Result generate("):]
commit_pos = gen.find("GroovePuterUndo::undoOwner().commitPrepared")
fail_pos = gen.find("if (!committed) {")
publish_pos = gen.find("engine.publishGeneratedSynthAOrigin(")
require(0 <= commit_pos < fail_pos < publish_pos,
        "D1-B: origin must be published only after commitPrepared succeeded")
require("beginOriginCandidate(*prepared)" in gen[:commit_pos],
        "D1-B: unpublished candidate must be created before COMMIT")
require("&originCandidate" in gen[commit_pos:fail_pos],
        "D1-B: COMMIT must fill the unpublished candidate")
require("engine.clearGeneratedSynthAOrigin();" in gen[publish_pos - 200:publish_pos + 400],
        "D1-B: Legacy/evidence-less generation must clear the sidecar")
require("engine.clearGeneratedSynthAOrigin();" in
        block(SONG, "if (result == GroovePuterUndo::UndoResult::Restored) {", "return result;"),
        "D1-B: Undo of the generated phrase must clear the sidecar")

writers = []
for path in list((ROOT / "src").rglob("*")) + list((ROOT).glob("*.ino")) + list((ROOT).glob("*.cpp")):
    if path.suffix not in {".h", ".cpp", ".ino"} or not path.is_file():
        continue
    if "publishGeneratedSynthAOrigin(" in path.read_text(encoding="utf-8", errors="ignore"):
        writers.append(path.relative_to(ROOT).as_posix())
require(sorted(writers) == ["src/dsp/generated_phrase_song.h", "src/dsp/miniacid_engine.h"],
        f"D1-B: only GeneratedPhraseSong may publish origin; found {writers}")

# 5. PMB-P1: the sidecar is NOT part of the prepared arrangement.
prepared = block(SONG, "struct PreparedPhraseArrangement {", "struct GeneratedPhraseUndoPayload")
require("GeneratedSynthAOrigin" not in prepared and "BassRhythmPlan" not in prepared,
        "D1-B: PreparedPhraseArrangement must not carry the sidecar or bass plans")
require("static_assert(sizeof(PreparedPhraseArrangement) <= 1024" in SONG,
        "D1-B: PMB-P1 1024-byte limit must remain")
payload = block(SONG, "struct GeneratedPhraseUndoPayload {", "inline ")
require("Origin" not in payload, "D1-B: Undo payload must not carry provenance")

# 6. No reverse analysis / verdicts in the origin publication path.
fill = block(SONG, "if (candidate != nullptr && prepared.useP1RRoute) {", "SongPosition& position")
for token in ("steps[", ".note", "IdeaClassification", "classif", "similar", "distance"):
    require(token not in fill, f"D1-B: origin fill must not analyse the Pattern ({token!r})")
require("projectPhraseHarmonicRhythmForBar(" in fill
        and "execution.harmonicTimeline" in fill
        and "inSecond ? *secondSection : prepared.p1rExecution" in SONG,
        "D1-B: harmonic WHEN must be copied from PREPARE, not re-derived")
require("versionForPattern(" in fill, "D1-B: origin version must be the committed Pattern's exact version")

# 7. Storage / ownership: no persistent or per-slot provenance.
for path in ("src/state/material_slot.h", "src/state/working_material_storage.h",
             "src/audio/pattern_paging.cpp", "src/audio/pattern_paging.h",
             "scene_storage.h", "scenes.h"):
    if (ROOT / path).exists():
        require("GeneratedSynthAOrigin" not in read(path),
                f"D1-B: provenance must not enter persistent/slot state ({path})")
require("PreparationBasis" in LINEAGE and "return reference.id.valid() && version.valid();" in LINEAGE,
        "D1-B: PreparationBasis ownership must be unchanged")
require("generatedSynthAOriginValid_" in ENGINE_H,
        "D1-B: sidecar must live in a dedicated engine-owned state object")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    print(f"0.9.14 D1-B source regressions: FAIL ({len(failures)} issues)")
    sys.exit(1)
print("0.9.14 D1-B source regressions: PASS")
