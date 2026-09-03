#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "2860f99d254baa96e06d48b3a52d3e729c2e707a"
ALLOWED_PRODUCTION = {
    "src/phrase/runtime_synth_events.h",
    "src/phrase/runtime_synth_events.cpp",
}
PROTECTED_PATHS = {
    "src/input/performance_keyboard.cpp",
    "src/input/performance_keyboard.h",
    "src/input/internal_synth_output.cpp",
    "src/input/internal_synth_output.h",
    "src/dsp/miniacid_engine.cpp",
    "src/dsp/miniacid_engine.h",
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


subprocess.run(["git", "cat-file", "-e", f"{BASE}^{{commit}}"], cwd=ROOT, check=True)
merge_base = subprocess.run(
    ["git", "merge-base", "HEAD", BASE],
    cwd=ROOT,
    check=True,
    text=True,
    capture_output=True,
).stdout.strip()
require(merge_base == BASE, f"P1C base mismatch: merge-base={merge_base} expected={BASE}")

paths = changed_paths()
production = {path for path in paths if path.startswith("src/")}
require(
    production.issubset(ALLOWED_PRODUCTION),
    f"P1C production firewall violated: {sorted(production - ALLOWED_PRODUCTION)}",
)
require(
    not (set(paths) & PROTECTED_PATHS),
    f"P1C Performance/scheduler firewall violated: {sorted(set(paths) & PROTECTED_PATHS)}",
)

header = ROOT / "src/phrase/runtime_synth_events.h"
source = ROOT / "src/phrase/runtime_synth_events.cpp"
if not header.exists() or not source.exists():
    print("P1C source contract: pre-implementation RED (runtime files absent)")
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
    "miniacid_engine",
    "performance_keyboard",
    "internal_synth_output",
):
    require(forbidden not in text, f"forbidden P1C dependency/token: {forbidden}")

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
    require(required in text, f"missing P1C contract token: {required}")

print("P1C source contract: OK")
