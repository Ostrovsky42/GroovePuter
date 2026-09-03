#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "c02d9ae04fcd43b5e3ced11e5aa50e850e26b4e6"
ALLOWED_PRODUCTION = {
    "src/phrase/runtime_synth_events.h",
    "src/phrase/runtime_synth_events.cpp",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def changed_paths() -> list[str]:
    result = subprocess.run(
        ["git", "diff", "--name-only", f"{BASE}...HEAD"],
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
    )
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


paths = changed_paths()
production = {path for path in paths if path.startswith("src/")}
require(
    production.issubset(ALLOWED_PRODUCTION),
    f"P1 production firewall violated: {sorted(production - ALLOWED_PRODUCTION)}",
)

header = ROOT / "src/phrase/runtime_synth_events.h"
source = ROOT / "src/phrase/runtime_synth_events.cpp"
if not header.exists() or not source.exists():
    print("P1 source contract: pre-implementation RED (runtime files absent)")
    raise SystemExit(0)

text = header.read_text(encoding="utf-8") + "\n" + source.read_text(encoding="utf-8")
for forbidden in (
    "gridSteps",
    "AudioMutationGate",
    "UndoOwner",
    "PhraseBank",
    "PhraseCore::",
    "std::vector",
    "malloc(",
    "calloc(",
    "realloc(",
    "new Runtime",
):
    require(forbidden not in text, f"forbidden P1 dependency/token: {forbidden}")

for required in (
    "RuntimeSynthEvent",
    "RuntimeSynthEventBuffer",
    "PatternProjectionSettings",
    "PatternProjectionStatus",
    "projectPatternToRuntimeEvents",
    "kTicksPerBar",
    "kSubticksPerTick",
    "kMaxPhraseBars",
    "kMaxSynthEvents",
):
    require(required in text, f"missing P1 contract token: {required}")

require("miniacid_engine" not in text.lower(),
        "P1 projector must not depend on MiniAcid runtime")
print("P1 source contract: OK")
