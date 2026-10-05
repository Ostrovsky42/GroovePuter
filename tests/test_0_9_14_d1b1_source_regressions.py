#!/usr/bin/env python3
"""D1-B1 source audit: exact pitch-class origin witness.

Evidence only: the witness comes from the bass TonalMaterializationPlan that
constructed Synth A, never from the committed Pattern, replay, or heuristics.
"""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


WITNESS = read("src/generation/tonal/bass_pitch_class_witness.h")
STRONG_H = read("src/generation/migration/strong_rhythm_migration.h")
STRONG_CPP = read("src/generation/migration/strong_rhythm_migration.cpp")
P1R = read("src/dsp/generated_phrase_p1r_materializer.h")
ORIGIN = read("src/state/generated_synth_a_origin.h")
SONG = read("src/dsp/generated_phrase_song.h")

failures: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def block(text: str, start: str, end: str) -> str:
    a = text.find(start)
    b = text.find(end, a + 1) if a >= 0 else -1
    return text[a:b] if a >= 0 and b > a else ""


# 1. Packing contract: 16 nibbles, 8 bytes, fail closed.
require("sizeof(BassPitchClassWitness) == 8" in WITNESS, "D1-B1: witness must be 8 bytes")
require("pitchClass >= 12" in WITNESS and "step >= kStepsPerBar" in WITNESS,
        "D1-B1: packing must fail closed on step>=16 / pitchClass>=12")

# 2. Owner: the witness is built from the exact bass tonal plan adapted into Synth A.
require(re.search(r"makeBassPitchClassWitness\(\s*bassTonal\.plan,\s*bass\.plan\.onsets,\s*result\.bassPitchClassWitness\)",
                  STRONG_CPP) is not None,
        "D1-B1: witness must be built from bassTonal.plan (paired with bass.plan.onsets)")
bass_block = block(STRONG_CPP, "const TonalMaterializationResult bassTonal = materializeRole(",
                   "result.bassFeelStatus = applyFeelToSemanticPattern(")
adapt_pos = bass_block.find("adaptTonalPlanToSynthPattern(")
witness_pos = bass_block.find("makeBassPitchClassWitness(")
require(0 <= adapt_pos < witness_pos,
        "D1-B1: availability must be set only after the tonal plan was adapted into Synth A")
require("bassPitchClassWitnessAvailable" in STRONG_H and "BassPitchClassWitness bassPitchClassWitness{}" in STRONG_H,
        "D1-B1: StrongRhythmMigrationResult must export the witness with explicit availability")

# 3. Witness derivation never touches the committed Pattern / replay sources.
helper = block(WITNESS, "inline bool makeBassPitchClassWitness(", "}  // namespace GroovePuterRhythm")
for token in ("SynthPattern", "steps[", "RuntimeSynthEventBuffer", "ChordProgression",
              "BassPitchBehaviorPlan", "genre", "Scene"):
    require(token not in helper, f"D1-B1: witness builder must not read {token!r}")
require("plan.onsetSteps" in helper and "plan.midiNotes" in helper,
        "D1-B1: witness builder must use TonalMaterializationPlan onsetSteps/midiNotes")

# 4. P1R bar evidence requires BOTH owner results.
require("bassPitchClasses" in block(P1R, "struct MaterializedSynthABarEvidence", "static_assert"),
        "D1-B1: bar evidence must carry the witness")
seam = block(P1R, "MaterializedSynthABarEvidence& evidence) {", "inline PreparationDisposition prepare(")
require("result.bassRhythmPlanAvailable &&" in seam and "result.bassPitchClassWitnessAvailable" in seam
        and "evidence.bassPitchClasses = result.bassPitchClassWitness;" in seam,
        "D1-B1: evidence valid only when rhythm plan AND witness are available")

# 5. Sidecar + budget.
require("BassPitchClassWitness bassPitchClasses{}" in ORIGIN, "D1-B1: bar origin must store the witness")
require("sizeof(GeneratedSynthAOrigin) <= 352" in ORIGIN, "D1-B1: explicit 352-byte ceiling missing")
require("sizeof(GeneratedSynthAOrigin) <= 512" not in ORIGIN, "D1-B1: no broad 512-byte budget allowed")
fill = block(SONG, "if (candidate != nullptr && prepared.useP1RRoute) {", "SongPosition& position")
require("entry.bassPitchClasses = barEvidence.bassPitchClasses;" in fill,
        "D1-B1: witness must be copied from the P1R bar evidence, not derived from the Pattern")
for token in ("steps[", ".note", "% 12"):
    require(token not in fill, f"D1-B1: origin fill must not derive pitch from the Pattern ({token!r})")

# 6. No semantic verdict vocabulary in the new evidence.
for name, text in (("witness header", WITNESS), ("origin header", ORIGIN)):
    for token in ("IdeaClassification", "LineageEvaluator", "NEW_IDEA", "CONTINUES", "StateRelation",
                  "TrajectoryAssessment", "ReturnAssessment", "GenreResult", "DevelopmentDisposition"):
        require(token not in text, f"D1-B1 firewall: {token!r} in {name}")

# 7. Publication owners unchanged from D1-B.
require("engine.publishGeneratedSynthAOrigin(" in SONG, "D1-B1: GeneratedPhraseSong must remain the publisher")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    print(f"0.9.14 D1-B1 source regressions: FAIL ({len(failures)} issues)")
    sys.exit(1)
print("0.9.14 D1-B1 source regressions: PASS")
