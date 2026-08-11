#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TYPES = (ROOT / "src/generation/rhythm/rhythm_types.h").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/generation/rhythm/rhythm_catalog.cpp").read_text(encoding="utf-8")
REALIZER = (ROOT / "src/generation/rhythm/rhythm_realizer.cpp").read_text(encoding="utf-8")
LIVE_BRIDGE = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text(encoding="utf-8")
REQUEST_STATE = (ROOT / "src/state/generation_request_state.h").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


# Stage 14.1 extends the mutation ABI without shifting the legacy aggregate
# fields. The new budgets must remain appended after allowedIntents.
legacy_tail = """uint16_t flags = 0;\n  TransformationIntentMask allowedIntents = 0;\n\n  // Independent realization budgets."""
require(TYPES, legacy_tail, "MutationBudget legacy field order changed")
require(TYPES, "uint8_t maxSecondaryAdds = 0;", "secondary budget missing")
require(TYPES, "uint8_t maxGhostAdds = 0;", "ghost budget missing")

# Catalog validation must understand the new fields rather than accepting
# malformed explicit budgets that only fail or saturate later in realization.
for needle in (
    "budget.maxSecondaryAdds > kMaxEventsPerBar",
    "budget.maxGhostAdds > kMaxEventsPerBar",
    "budget.maxSecondaryAdds ||",
    "budget.maxGhostAdds)",
):
    require(CATALOG, needle, f"Stage 14.1 catalog validation missing: {needle}")

# P3 is cumulative over P2 ornaments. The pre-existing variation class must keep
# the old unsalted seed so P2 remains bit-compatible; only the newly-added
# second pass receives a disjoint salt.
for needle in (
    "uint8_t secondaryBudgetFor(const MutationBudget& budget)",
    "uint8_t ghostBudgetFor(const RhythmArchetype& archetype,",
    "level != RealizationLevel::P3Transformation",
    "RealizationLevel::P2Variation",
    "addVariationPass(archetype, seed,",
    "const uint32_t ghostSeed = secondaryAdds != 0",
    "seed ^ 0x47484F31u",
    ": seed;",
):
    require(REALIZER, needle, f"Stage 14.1 realizer contract missing: {needle}")

if "seed ^ 0x53454331u" in REALIZER:
    raise AssertionError("legacy secondary/P2-compatible variation path was re-salted")

# 0.9.1 now exposes P1/P2/P3 as a runtime request selector. Preserve the old
# Stage 14.1 compatibility baseline by keeping P2 as the boot/invalid default,
# while requiring the live bridge to read the shared selector rather than
# hard-coding P2 forever.
for needle in (
    "RealizationLevel::P2Variation",
    "currentGenerationLevel",
    "cycleGenerationLevel",
):
    require(REQUEST_STATE, needle, f"P-level request-state contract missing: {needle}")

require(
    LIVE_BRIDGE,
    "context.level = GroovePuterState::currentGenerationLevel();",
    "live bridge stopped consuming the shared P-level selector",
)
if "context.level = RealizationLevel::P2Variation;" in LIVE_BRIDGE:
    raise AssertionError("live bridge regressed to hard-coded P2 production level")

print("Stage 14.1 P-level source regressions: OK")
