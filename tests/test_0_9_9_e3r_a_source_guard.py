#!/usr/bin/env python3
"""E3R-A source guard: audit DROP/DISPLACE without inventing execution semantics."""

from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "872fef9331a34e8d48f5703015b1126f11e581c3"


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
require(changed, "E3R-A branch has no audit delta")
require(
    not any(path.startswith("src/") for path in changed),
    "E3R-A modified production src/: " +
    ", ".join(path for path in changed if path.startswith("src/")),
)

header = text("src/generation/rhythm/rhythm_realizer.h")
evolution = text("src/generation/rhythm/rhythm_realizer_evolution.cpp")
diff_impl = text("src/generation/rhythm/rhythm_canonical_diff.cpp")
e2a_test = text("tests/test_0_9_9_e2a_mutation_producer.cpp")
e2b_test = text("tests/test_0_9_9_e2b_canonical_diff_budget.cpp")
v0r = text("tools/research/v0r_e2_variant_graph.cpp")

# E2c grammar/value representation exists for both operations.
for token in (
    "RhythmMutationOp::DROP",
    "RhythmMutationOp::DISPLACE",
    "delta.targetStep == kNoMutationStep",
    "rhythmMutationDisplacementDistance(",
    "kDisplaceRadius = 2u",
    "rhythmMutationDisplacementGrammarLegal(",
):
    require(token in header, f"missing frozen E2c evidence: {token}")

# E2a can produce both delta shapes in its synthetic contract fixture. This is
# representation evidence only; it is deliberately not treated as execution.
for token in (
    "RhythmMutationOp::DROP, 4, kNoMutationStep",
    "RhythmMutationOp::DISPLACE, 4, 2",
    "fixtureCounts.drop > 0",
    "fixtureCounts.displace > 0",
):
    require(token in e2a_test, f"missing E2a synthetic fixture evidence: {token}")

# The authoritative public executor remains BarFunction-based. There is no
# delta argument and therefore no caller-selected sourceStep/targetStep path.
require(
    "bool applyRhythmBarFunctionMutation(const RhythmArchetype& archetype," in header and
    "BarFunction function," in header,
    "authoritative BarFunction executor signature changed",
)
executor_start = evolution.index("bool applyRhythmBarFunctionMutation(")
executor = evolution[executor_start:]
require(
    "RhythmMutationDelta" not in executor and
    "RhythmMutationOp::" not in executor,
    "BarFunction executor unexpectedly became a delta executor",
)
require(
    "case BarFunction::Reduction:" in executor and
    "case BarFunction::Break:" in executor and
    executor.count("clearGhosts(archetype, plan, bar);") >= 2 and
    executor.count("dropOneStructuralEvent(") >= 2,
    "legacy Reduction/Break destructive behavior changed",
)

drop_start = evolution.index("bool dropOneStructuralEvent(")
drop_end = evolution.index("void clearGhosts(", drop_start)
drop_helper = evolution[drop_start:drop_end]
require(
    "RhythmMutationDelta" not in drop_helper and
    "sourceStep" not in drop_helper and
    "targetStep" not in drop_helper,
    "legacy drop helper became a caller-selected per-delta executor",
)
require(
    "role.secondary =" in drop_helper and
    "role.structural =" in drop_helper and
    "recomputeRoleGates(lane, role);" in drop_helper,
    "legacy selected-event removal evidence changed",
)
require(
    "role.accents" not in drop_helper,
    "legacy drop helper now owns accent cleanup; re-audit DROP contract",
)

# DISPLACE exists only in candidate/diff grammar paths, not BarFunction
# execution. This absence is the contract-gap finding, not permission to mirror
# a move in research tooling.
require(
    "case RhythmMutationOp::DISPLACE:" in evolution[:executor_start],
    "E2a DISPLACE producer/topology evidence missing",
)
require(
    "DISPLACE" not in executor,
    "authoritative BarFunction executor now mentions DISPLACE; re-audit required",
)

# E2b already recognizes manually constructed DROP/DISPLACE material.
for token in (
    "result.stats.drops == 1",
    "deltas[0].operation == RhythmMutationOp::DROP",
    "result.stats.displacements == 1",
    "deltas[0].operation == RhythmMutationOp::DISPLACE",
):
    require(token in e2b_test, f"missing E2b diff evidence: {token}")
require(
    "result.candidatePlanValid = rhythmMutationPlanValid(archetype, candidate);"
    in diff_impl,
    "E2b no longer delegates structural legality to rhythm_realizer",
)

# Frozen V0R is still fail-closed for operations without an accepted
# materialization path. E3R-A must not weaken that boundary.
adapter_start = v0r.index("bool materializeCurrentProductionProposal(")
adapter_end = v0r.index("std::string hex16", adapter_start)
adapter = v0r[adapter_start:adapter_end]
for operation in ("DROP", "DISPLACE", "ACCENT"):
    require(
        f"case RhythmMutationOp::{operation}:" in adapter,
        f"V0R fail-closed case missing for {operation}",
    )
require(
    "return false;" in adapter,
    "V0R unsupported-operation branch no longer fails closed",
)

print("E3R-A source guard: ZERO src delta, DROP/DISPLACE execution gaps preserved")
