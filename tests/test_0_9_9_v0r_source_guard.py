#!/usr/bin/env python3
"""V0R source guard: research-only graph tooling over frozen E2 contracts."""

from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "0992f3d8b7af317d91ff166b641b893f806a921c"


def text(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


changed = subprocess.check_output(
    ["git", "diff", "--name-only", f"{BASE}..HEAD"],
    cwd=ROOT,
    text=True,
).splitlines()
require(changed, "V0R branch has no tooling delta")
require(
    not any(path.startswith("src/") for path in changed),
    "V0R modified production src/: " +
    ", ".join(path for path in changed if path.startswith("src/")),
)

header = text("src/generation/rhythm/rhythm_realizer.h")
producer = text("src/generation/rhythm/rhythm_realizer_evolution.cpp")
diff_header = text("src/generation/rhythm/rhythm_canonical_diff.h")
diff_impl = text("src/generation/rhythm/rhythm_canonical_diff.cpp")
reference = text("src/generation/rhythm/reference_vocabulary.cpp")
tool = text("tools/research/v0r_e2_variant_graph.cpp")
report = text("tools/research/v0r_report.py")

for token in (
    "enum class RhythmMutationOp",
    "struct RhythmMutationDelta",
    "kDisplaceRadius = 2u",
    "rhythmMutationDeltaLess",
    "rhythmMutationDisplacementGrammarLegal",
    "produceRhythmMutationCandidates(",
):
    require(token in header, f"missing frozen E2 contract token: {token}")
require(
    producer.count("RhythmMutationProducerResult produceRhythmMutationCandidates(")
    == 1,
    "E2a canonical producer implementation is not unique",
)
require(
    "canonicalRhythmCandidateValid(" in diff_header and
    diff_impl.count(
        "CanonicalRhythmCandidateValidation canonicalRhythmCandidateValid(")
    == 1,
    "E2b canonical-relative legality handoff missing",
)
require(
    "result.candidatePlanValid = rhythmMutationPlanValid(archetype, candidate);"
    in diff_impl,
    "E2b no longer delegates archetype/structural identity to rhythm_realizer",
)

mappings = (
    ("StraightDrive", "401", "straight_drive"),
    ("OffbeatOpenHat", "402", "offbeat_open_hat"),
    ("SparseFastBreak", "415", "sparse_fast_break"),
    ("HalftimeSwitch", "416", "halftime_switch"),
    ("HypnoticSparse", "403", "hypnotic_sparse"),
    ("RollingAcid", "406", "rolling_acid"),
)
for enum_name, archetype_id, production_name in mappings:
    needle = (
        f'{{Archetype::{enum_name}, {archetype_id}, "{production_name}"')
    require(needle in reference, f"reference mapping changed: {enum_name}")

require(
    "value.trajectories = &kStatementRef;" in reference and
    "constexpr BarTrajectory kStatementTrajectory" in reference,
    "ReferenceVocabulary trajectory ownership changed",
)

for token in (
    "produceRhythmMutationCandidates(",
    "canonicalRhythmCandidateValid(",
    "const RhythmPhrasePlan canonical = canonicalPlanFor",
    "materializeCurrentProductionProposal(",
    "std::map<std::string, uint32_t>",
    "kNodeCeiling",
    "kTransitionCeiling",
):
    require(token in tool, f"V0R implementation missing: {token}")

# The guard cares about argument ownership, not source formatting. Normalize
# whitespace so harmless indentation/wrapping changes cannot weaken the actual
# canonical-relative invariant.
compact_tool = " ".join(tool.split())
require(
    "canonicalRhythmCandidateValid( *archetype, canonical, candidate, 0, level,"
    in compact_tool,
    "V0R canonical-relative legality is not legal(C,W)",
)
require(
    "canonicalRhythmCandidateValid( *archetype, current, candidate"
    not in compact_tool,
    "V0R launders budget via legal(V,W)",
)

adapter_start = tool.index("bool materializeCurrentProductionProposal(")
adapter_end = tool.index("std::string hex16", adapter_start)
adapter = tool[adapter_start:adapter_end]
require(
    "case RhythmMutationOp::ADD:" in adapter and
    "role.secondary =" in adapter and
    "case RhythmMutationOp::GHOST:" in adapter and
    "role.ghosts =" in adapter,
    "V0R ADD/GHOST representation adapter changed",
)
for operation in ("DROP", "DISPLACE", "ACCENT"):
    require(
        f"case RhythmMutationOp::{operation}:" in adapter,
        f"V0R must fail closed for {operation}",
    )
require(
    "Synthetic" not in tool and "contractFixture" not in tool,
    "synthetic E2a fixtures leaked into production graph measurement",
)

require(
    'delta.operation == RhythmMutationOp::KEEP' in tool and
    '"KEEP graph edge"' in report,
    "KEEP self-edge guard missing",
)
for forbidden in (
    "patternAddress",
    "generationAttemptOrdinal",
    "ActivityPolicy",
    "RhythmEvolutionActivity",
    "candidateCache",
    "lifecycleState",
):
    require(forbidden not in tool, f"V0R took forbidden ownership: {forbidden}")

for field in (
    "value.structural",
    "value.secondary",
    "value.ghosts",
    "value.shortGate",
    "value.heldGate",
    "value.tieGate",
    "value.accents",
    "plan.barCount",
    "plan.trajectoryId",
    "plan.level",
    "plan.intent",
    "plan.bars[bar].function",
):
    require(field in tool, f"node key missing observable field: {field}")
for bad in ("memcmp(", "reinterpret_cast<const char", "sizeof(RhythmPhrasePlan)"):
    require(bad not in tool, f"node identity uses struct bytes: {bad}")

print("V0R source guard: production semantics unchanged, E2 authority boundary OK")
