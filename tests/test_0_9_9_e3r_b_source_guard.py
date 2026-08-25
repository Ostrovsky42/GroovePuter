#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
E3A = "2549bbfdda84581a14783a775d3c8577f8855c54"
V0R = "872fef9331a34e8d48f5703015b1126f11e581c3"
INTEGRATION = "f01c4639e2cf3f485efb94167fce5a4c0a007721"
FROZEN_V0R = (
    "tools/research/v0r_e2_variant_graph.cpp",
    "tools/research/v0r_report.py",
    "tools/research/v0r_compact_summary.py",
    "tests/data/v0r_e2_variant_graph_summary.csv",
    "tests/data/v0r_e2_variant_graph_summary.json",
)


def git(*args: str, check: bool = True) -> str:
    completed = subprocess.run(
        ["git", *args], cwd=ROOT, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if check and completed.returncode != 0:
        raise SystemExit(
            f"git {' '.join(args)} failed: {completed.stderr.strip()}")
    return completed.stdout


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


for ancestor, label in ((E3A, "E3a"), (V0R, "V0R"), (INTEGRATION, "integration")):
    completed = subprocess.run(
        ["git", "merge-base", "--is-ancestor", ancestor, "HEAD"], cwd=ROOT)
    require(completed.returncode == 0, f"E3R-B missing {label} ancestor {ancestor}")

src_delta = [
    line for line in git("diff", "--name-only", f"{E3A}..HEAD", "--", "src/").splitlines()
    if line
]
require(not src_delta, "E3R-B production src delta is non-zero: " + ", ".join(src_delta))

for path in FROZEN_V0R:
    completed = subprocess.run(
        ["git", "diff", "--quiet", f"{V0R}..HEAD", "--", path], cwd=ROOT)
    require(completed.returncode == 0, f"E3R-B modified frozen V0R authority file: {path}")

tool = (ROOT / "tools/research/e3r_b_drop_displace_graph.cpp").read_text(encoding="utf-8")
report = (ROOT / "tools/research/e3r_b_report.py").read_text(encoding="utf-8")

for token in (
    '"BASE", false, false',
    '"DROP", true, false',
    '"DISPLACE", false, true',
    '"DROP_DISPLACE", true, true',
    "constexpr uint8_t kPrimaryCap = 1u;",
    "budget.maxDrops = kPrimaryCap;",
    "AllowPreferredDrops",
    "budget.maxDisplacements = kPrimaryCap;",
    "AllowOptionalDisplace",
    "produceRhythmMutationCandidates(",
    "applyRhythmMutationDelta(",
    "canonicalRhythmCandidateValid(",
    "materializeFrozenAddGhost(",
    "ENUMERATION INCOMPLETE",
):
    require(token in tool, f"E3R-B graph contract missing: {token}")

adapter_start = tool.index("bool materializeFrozenAddGhost(")
adapter_end = tool.index("std::string hex16", adapter_start)
adapter = tool[adapter_start:adapter_end]
require("RhythmMutationOp::ADD" in adapter and "RhythmMutationOp::GHOST" in adapter,
        "E3R-B frozen ADD/GHOST adapter incomplete")
require("RhythmMutationOp::DROP" not in adapter and
        "RhythmMutationOp::DISPLACE" not in adapter,
        "E3R-B invented a DROP/DISPLACE research materializer")

executor_call = tool.index("applyStatus = applyRhythmMutationDelta(")
require("delta.operation == RhythmMutationOp::DROP" in tool[:executor_call] and
        "delta.operation == RhythmMutationOp::DISPLACE" in tool[:executor_call],
        "E3R-B executor call is not guarded by DROP/DISPLACE operation")

compact = " ".join(tool.split())
require(
    "canonicalRhythmCandidateValid( archetype, canonical, candidate, 0, level,"
    in compact,
    "E3R-B legality is not canonical-relative legal(C,W)",
)
require(
    "canonicalRhythmCandidateValid( archetype, current, candidate"
    not in compact,
    "E3R-B resets canonical budget per local hop",
)

require("budget.maxAccentChanges =" not in tool,
        "E3R-B overlay modifies ACCENT budget")
require("AllowAccentVariation)" in tool,
        "E3R-B does not guard ACCENT production policy")
require("producer emitted forbidden/invalid proposal" in tool,
        "E3R-B must fail closed if ACCENT appears")

for forbidden in (
    "ActivityPolicy", "RhythmEvolutionActivity", "lifecycleState",
    "candidateCache", "selector", "PatternPicker", "VariationUI",
):
    require(forbidden not in tool, f"E3R-B took forbidden ownership: {forbidden}")

for token in (
    "nodes_in_nontrivial_sccs",
    "reverse_reachable_to_canonical_rate",
    "DROP_reverse_ADD_edges",
    "DROP_reverse_GHOST_edges",
    "DISPLACE_distance1",
    "DISPLACE_distance2",
    "DISPLACE_structural",
    "DISPLACE_secondary",
    "DISPLACE_ghost",
    "MUSICAL LISTENING: PENDING",
):
    require(token in report, f"E3R-B report missing required metric/corpus token: {token}")

print("E3R-B source guard: ZERO src delta, frozen V0R + E3a authority boundaries PASS")
