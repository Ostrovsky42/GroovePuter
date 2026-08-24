#!/usr/bin/env python3
"""E2c source contract: one owner, one canonical mutation-delta vocabulary."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def source(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


header = source("src/generation/rhythm/rhythm_realizer.h")
types = source("src/generation/rhythm/rhythm_types.h")
realizer = source("src/generation/rhythm/rhythm_realizer.cpp")
mutation = source("src/generation/rhythm/rhythm_realizer_evolution.cpp")
bar_header = source("src/generation/rhythm/bar_evolution.h")
bar = source("src/generation/rhythm/bar_evolution.cpp")

for token in (
    "enum class RhythmMutationOp",
    "KEEP = 0",
    "ADD = 1",
    "DROP = 2",
    "DISPLACE = 3",
    "ACCENT = 4",
    "GHOST = 5",
    "struct RhythmMutationDelta",
    "kNoMutationStep = 0xFFu",
    "kDisplaceRadius = 2u",
    "kMaxRhythmMutationDeltasPerBar",
    "kMaxRhythmMutationDeltasPerPhrase",
    "rhythmMutationDeltaShapeValid",
    "rhythmMutationDeltaLess",
    "rhythmMutationDisplacementGrammarLegal",
):
    require(token in header, f"missing E2c contract token: {token}")

require(
    header.count("enum class RhythmMutationOp") == 1 and
    header.count("struct RhythmMutationDelta") == 1,
    "canonical mutation operation/delta representation is not unique",
)

require(
    "static_cast<uint8_t>(GenerationDomain::BarEvolution) == 12" in header,
    "GenerationDomain ABI guard for BarEvolution=12 is missing",
)

budget_start = types.find("struct MutationBudget")
budget_end = types.find("struct MutationPolicy", budget_start)
require(budget_start >= 0 and budget_end > budget_start,
        "cannot isolate existing MutationBudget")
budget = types[budget_start:budget_end]
for field in (
    "maxAdds",
    "maxDrops",
    "maxDisplacements",
    "maxAccentChanges",
    "flags",
    "allowedIntents",
    "maxSecondaryAdds",
    "maxGhostAdds",
):
    require(field in budget, f"existing MutationBudget field missing: {field}")

require(
    "struct AnchorTransformRule" in types and
    "displaceableCanonical" in types and
    "rule.displaceableCanonical & sourceBit" in header,
    "canonical displacement no longer uses existing grammar permission",
)

require(
    "struct MutationBudget" not in header and
    "enum MutationFlags" not in header and
    "mutationCost" not in header and
    "budgetClass" not in header,
    "E2c introduced a competing budget/flag hierarchy",
)

for implementation in (realizer, mutation):
    require(
        "RhythmMutationDelta" not in implementation and
        "RhythmMutationOp" not in implementation,
        "E2c must not wire the delta contract into production execution",
    )

require(
    "bool applyRhythmBarFunctionMutation(" in header and
    "bool rhythmMutationPlanValid(" in header and
    mutation.count("bool applyRhythmBarFunctionMutation(") == 1,
    "rhythm_realizer is no longer the unique mutation owner",
)

require(
    "RhythmMutationDelta" not in bar_header and
    "RhythmMutationOp" not in bar_header and
    "RhythmMutationDelta" not in bar and
    "RhythmMutationOp" not in bar and
    "dropOneStructuralEvent" not in bar and
    "addGhostCue" not in bar and
    "applyRhythmBarFunctionMutation(" in bar and
    "return rhythmMutationPlanValid(archetype, plan);" in bar,
    "bar_evolution became a competing mutation owner",
)

variation_start = realizer.find(
    "void addVariation(const RhythmArchetype& archetype"
)
variation_end = realizer.find("bool requestValid(", variation_start)
require(variation_start >= 0 and variation_end > variation_start,
        "cannot isolate production addVariation")
variation = realizer[variation_start:variation_end]
require(
    "secondaryAdds" in variation and
    "ghostAdds" in variation and
    "maxDisplacements" not in variation and
    "maxAccentChanges" not in variation and
    "dropOneStructuralEvent" not in variation,
    "E2c changed characterized production musical mutation output",
)

ghost_start = realizer.find("bool addPlanGhost(")
ghost_end = realizer.find("uint8_t legacySecondaryBudget(", ghost_start)
require(ghost_start >= 0 and ghost_end > ghost_start,
        "cannot isolate production ghost-add primitive")
ghost_add = realizer[ghost_start:ghost_end]
require(
    "(rolePlan.structural | rolePlan.secondary |" in ghost_add and
    "rolePlan.ghosts = static_cast<StepMask>(" in ghost_add,
    "production GHOST evidence no longer means an added ghost onset",
)

print("E2C canonical rhythm mutation delta source contract: OK")
