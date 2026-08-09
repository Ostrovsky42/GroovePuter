#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/rhythm/bar_evolution.h").read_text()
SOURCE = (ROOT / "src/generation/rhythm/bar_evolution.cpp").read_text()
COMBINED = HEADER + "\n" + SOURCE


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


for token in (
    "BarEvolutionRequest",
    "BarEvolutionResult",
    "evolveRhythmPhrase",
    "GenerationDomain::BarEvolution",
    "requestedTrajectoryId",
    "BarFunction::Repeat",
    "BarFunction::RepeatWithGhosts",
    "BarFunction::Response",
    "BarFunction::Reduction",
    "BarFunction::Build",
    "BarFunction::Turnaround",
    "BarFunction::Break",
    "BarFunction::Return",
    "evolvedPlanValid",
):
    require(token in COMBINED, f"missing Stage 6 contract token: {token}")

# Stage 6 is a transient grammar core. It may manipulate fixed-capacity rhythm
# value objects only; storage/runtime ownership stays with later callers.
for forbidden_include in (
    '#include "../../../scenes.h"',
    '#include "../../dsp/miniacid_engine.h"',
    '#include "../../phrase/phrase_types.h"',
):
    require(forbidden_include not in COMBINED,
            f"Stage 6 leaked storage/runtime ownership: {forbidden_include}")

for forbidden_call in (
    "editCurrentDrumPattern",
    "editCurrentSynthPattern",
    "setSongPattern",
    "setSongPosition",
    "setCurrentBankIndex",
    "setCurrentDrumPatternIndex",
    "setCurrentSynthPatternIndex",
):
    require(forbidden_call not in COMBINED,
            f"Stage 6 mutates external storage: {forbidden_call}")

require("std::vector" not in COMBINED and
        "std::string" not in COMBINED and
        "malloc(" not in COMBINED and
        "new " not in COMBINED,
        "Stage 6 introduced heap-owning runtime structures")

# The production Stage 5 bridge remains one-bar. Stage 6 must not silently turn
# normal GENRE MATERIALIZE into multi-bar storage writes.
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
require("request.phraseBars = 1;" in MIGRATION,
        "Stage 6 unexpectedly changed Stage 5 production phrase length")
require("bar_evolution.h" not in MIGRATION,
        "Stage 5 production migration directly owns BarEvolution")

print("Groove Vocabulary Stage 6 source ownership: OK")
