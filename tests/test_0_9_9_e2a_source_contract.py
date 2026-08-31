#!/usr/bin/env python3
"""E2a source contract: one bounded candidate producer under rhythm_realizer."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def source(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def between(text: str, first: str, second: str) -> str:
    start = text.find(first)
    end = text.find(second, start + len(first))
    require(start >= 0 and end > start, f"cannot isolate {first}")
    return text[start:end]


header = source("src/generation/rhythm/rhythm_realizer.h")
mutation = source("src/generation/rhythm/rhythm_realizer_evolution.cpp")
realizer = source("src/generation/rhythm/rhythm_realizer.cpp")
bar = source("src/generation/rhythm/bar_evolution.cpp")
phrase = source("src/generation/phrase/phrase_evolution.cpp")
bridge = source("src/generation/migration/strong_rhythm_live_bridge.cpp")
types = source("src/generation/rhythm/rhythm_types.h")
reference = source("src/generation/rhythm/reference_vocabulary.cpp")

for token in (
    "enum class RhythmMutationProducerStatus",
    "struct RhythmMutationProducerRequest",
    "struct RhythmMutationProducerResult",
    "produceRhythmMutationCandidates(",
):
    require(token in header, f"missing E2a producer API token: {token}")

require(header.count("produceRhythmMutationCandidates(") == 1,
        "E2a producer API is not unique")
require(mutation.count("produceRhythmMutationCandidates(") == 1,
        "E2a producer implementation is not unique")
for competing in (realizer, bar, phrase, bridge):
    require("produceRhythmMutationCandidates(" not in competing,
            "E2a producer escaped the authoritative rhythm_realizer owner")

helpers = between(
    mutation,
    "// E2A_CANDIDATE_PRODUCER_HELPERS_BEGIN",
    "// E2A_CANDIDATE_PRODUCER_HELPERS_END",
)
producer = between(
    mutation,
    "RhythmMutationProducerResult produceRhythmMutationCandidates(",
    "bool rhythmMutationPlanValid(",
)
e2a = helpers + producer

for quota in (
    "maxAdds",
    "maxDrops",
    "maxDisplacements",
    "maxAccentChanges",
    "maxSecondaryAdds",
    "maxGhostAdds",
):
    require(quota not in e2a,
            f"E2a illegally owns canonical-relative numeric budget: {quota}")

require("budget.flags" in e2a and "budget.allowedIntents" in e2a,
        "E2a stopped consuming existing operation permissions")
require("rhythmMutationDisplacementGrammarLegal(" in producer,
        "E2a DISPLACE bypasses frozen E2c grammar")
require("rule.suppressibleCanonical & bit" in helpers,
        "E2a canonical DROP lacks an explicit existing transform rule")
require("rule.displaceableCanonical & sourceBit" in header,
        "E2c canonical DISPLACE rule was redefined or bypassed")
require("hardRelationshipsSatisfied" in helpers,
        "E2a topology proposals ignore relationship safety")

# Stable order must be emitted directly by role ordinal, logical step and E2c
# operation ordinal. KEEP is a semantic value, never a self-edge candidate.
require("for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex)" in producer,
        "E2a does not enumerate roles in stable ordinal order")
require("for (uint8_t step = 0; step < kStepsPerBar; ++step)" in producer,
        "E2a does not enumerate logical steps in stable 0..15 order")
markers = [
    "RhythmMutationOp::ADD",
    "RhythmMutationOp::DROP",
    "RhythmMutationOp::DISPLACE",
    "RhythmMutationOp::ACCENT",
    "RhythmMutationOp::GHOST",
]
positions = [producer.find(marker) for marker in markers]
require(all(position >= 0 for position in positions) and positions == sorted(positions),
        "E2a producer does not emit non-KEEP operations in E2c order")
require("RhythmMutationOp::KEEP," not in producer,
        "E2a emitted KEEP as a candidate/self-edge")
require("std::sort" not in e2a and "unordered_" not in e2a,
        "E2a candidate order depends on sorting/unordered containers")

require("kMaxRhythmMutationDeltasPerBar" in producer and
        "boundedCapacity" in producer and "result.truncated" in helpers,
        "E2a output is not explicitly bounded")
for allocation in ("std::vector", "new ", "malloc(", "realloc("):
    require(allocation not in e2a,
            f"E2a producer introduced dynamic allocation: {allocation}")

# No cadence, lifecycle, persistence, or transport-derived identity in producer.
for forbidden in (
    "patternAddress",
    "generationAttemptOrdinal",
    "lifecycle",
    "KEEP / REVERT / EVOLVE",
    "candidateCache",
    "candidate_cache",
    "persist",
    "history",
    "scheduler",
    "transport",
):
    require(forbidden not in e2a,
            f"E2a crossed an ownership boundary: {forbidden}")
require("request.generation" not in e2a,
        "E2a GenerationContext became cadence/ranking ownership")

# The frozen E2c vocabulary/shape/order remains the sole representation.
require(header.count("enum class RhythmMutationOp") == 1 and
        header.count("struct RhythmMutationDelta") == 1,
        "E2a introduced a second mutation vocabulary")
require("kDisplaceRadius = 2u" in header and
        "rhythmMutationDeltaLess" in header,
        "E2a redefined E2c radius/order")
require("struct MutationBudget" in types and "enum MutationFlags" in types,
        "existing mutation policy disappeared")

# Current reference policy is factual evidence, not a request for fake coverage.
policy_start = reference.find("constexpr MutationPolicy referenceMutationPolicy()")
policy_end = reference.find("constexpr BarTrajectory", policy_start)
require(policy_start >= 0 and policy_end > policy_start,
        "cannot isolate current reference mutation policy")
policy = reference[policy_start:policy_end]
require("RealizationLevel::P2Variation" in policy and
        "AllowGhostConversion" in policy,
        "reference P2 ghost permission changed")
require("RealizationLevel::P3Transformation" in policy and
        "AllowOptionalAdds" in policy,
        "reference P3 optional-add permission changed")
for absent in ("AllowPreferredDrops", "AllowOptionalDisplace", "AllowAccentVariation"):
    require(absent not in policy,
            f"reference policy unexpectedly enables E2a operation: {absent}")

print("E2A canonical rhythm mutation producer source contract: OK")
