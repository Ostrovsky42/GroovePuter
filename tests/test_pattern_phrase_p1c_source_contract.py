#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "2860f99d254baa96e06d48b3a52d3e729c2e707a"
P1C_TIP = "9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38"
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


def output(*args: str) -> str:
    return subprocess.run(
        ["git", *args],
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
    ).stdout.strip()


def changed_paths(base: str, head: str) -> list[str]:
    text = output("diff", "--name-only", f"{base}...{head}")
    return [line.strip() for line in text.splitlines() if line.strip()]


for sha in (BASE, P1C_TIP):
    subprocess.run(["git", "cat-file", "-e", f"{sha}^{{commit}}"], cwd=ROOT, check=True)

require(
    output("merge-base", P1C_TIP, BASE) == BASE,
    "P1C canonical base ancestry mismatch",
)
require(
    output("merge-base", "HEAD", P1C_TIP) == P1C_TIP,
    "P1C canonical integrated tip is not an ancestor of HEAD",
)

canonical_paths = changed_paths(BASE, P1C_TIP)
canonical_production = {path for path in canonical_paths if path.startswith("src/")}
require(
    canonical_production.issubset(ALLOWED_PRODUCTION),
    f"P1C canonical production firewall violated: {sorted(canonical_production - ALLOWED_PRODUCTION)}",
)
require(
    not (set(canonical_paths) & PROTECTED_PATHS),
    f"P1C canonical Performance/scheduler firewall violated: {sorted(set(canonical_paths) & PROTECTED_PATHS)}",
)

for path in sorted(ALLOWED_PRODUCTION):
    unchanged = subprocess.run(
        ["git", "diff", "--quiet", P1C_TIP, "HEAD", "--", path],
        cwd=ROOT,
    ).returncode == 0
    require(unchanged, f"P1C representation changed after canonical integration: {path}")

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
