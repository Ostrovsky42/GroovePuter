#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "0992f3d8b7af317d91ff166b641b893f806a921c"
ALLOWED_PRODUCTION = {
    "src/generation/rhythm/rhythm_realizer.h",
    "src/generation/rhythm/rhythm_realizer_evolution.cpp",
}


def git(*args: str) -> str:
    return subprocess.check_output(
        ["git", *args], cwd=ROOT, text=True
    )


changed = {
    line.strip()
    for line in git("diff", "--name-only", BASE, "HEAD").splitlines()
    if line.strip()
}
production = {path for path in changed if path.startswith("src/")}
if production != ALLOWED_PRODUCTION:
    raise SystemExit(
        f"E3a production scope drift: {sorted(production)} != "
        f"{sorted(ALLOWED_PRODUCTION)}"
    )

current = (ROOT / "src/generation/rhythm/rhythm_realizer_evolution.cpp").read_text(
    encoding="utf-8"
)
base = git(
    "show",
    f"{BASE}:src/generation/rhythm/rhythm_realizer_evolution.cpp",
)

legacy_marker = "bool applyRhythmBarFunctionMutation("
if current[current.index(legacy_marker):] != base[base.index(legacy_marker):]:
    raise SystemExit("E3a modified legacy applyRhythmBarFunctionMutation behavior")

producer_begin = "RhythmMutationProducerResult produceRhythmMutationCandidates("
producer_end = "bool rhythmMutationPlanValid("
current_producer = current[
    current.index(producer_begin):current.index(producer_end)
]
base_producer = base[
    base.index(producer_begin):base.index(producer_end)
]
if current_producer != base_producer:
    raise SystemExit("E3a modified frozen E2a production producer")

apply_begin = "RhythmMutationApplyStatus applyRhythmMutationDelta("
apply_end = legacy_marker
apply_source = current[current.index(apply_begin):current.index(apply_end)]
required = [
    "RhythmPhrasePlan trial = plan;",
    "rhythmMutationDeltaShapeValid(delta)",
    "RhythmMutationOp::DROP",
    "RhythmMutationOp::DISPLACE",
    "RhythmMutationApplyStatus::UnsupportedOperation",
    "RhythmMutationApplyStatus::UnsupportedSourceKind",
    "rhythmMutationDisplacementGrammarLegal(",
    "recomputeRoleGates(*lane, role);",
    "rhythmMutationPlanValid(archetype, trial)",
    "plan = trial;",
]
for token in required:
    if token not in apply_source:
        raise SystemExit(f"E3a exact executor contract missing: {token}")

for forbidden in ("bestDropCandidate(", "clearGhosts(", "MutationBudget"):
    if forbidden in apply_source:
        raise SystemExit(f"E3a exact executor leaked legacy/policy semantics: {forbidden}")

header = (ROOT / "src/generation/rhythm/rhythm_realizer.h").read_text(
    encoding="utf-8"
)
for token in (
    "enum class RhythmMutationApplyStatus",
    "RhythmMutationApplyStatus applyRhythmMutationDelta(",
    "Only DROP and DISPLACE are",
):
    if token not in header:
        raise SystemExit(f"E3a public contract missing: {token}")

print("E3a source contract: owner/scope/legacy/E2a policy guards PASS")
