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

header = ROOT / "src/phrase/runtime_synth_events.h"
source = ROOT / "src/phrase/runtime_synth_events.cpp"
if not header.exists() or not source.exists():
    print("P1C source contract: pre-implementation RED (runtime files absent)")
    raise SystemExit(0)

header_text = header.read_text(encoding="utf-8")
source_text = source.read_text(encoding="utf-8")

# P2 is allowed one additive companion projection API so the compact retained
# carrier can preserve physical-step execution order without changing the
# chronological P1C value representation. Everything else in the canonical
# header remains byte-frozen: event/buffer layouts, constants, flags, settings,
# status, original projector declaration, and ABI static_asserts.
companion_api = '''// P2 companion projection metadata. The existing RuntimeSynthEvent ABI and
// chronological RuntimeSynthEventBuffer order remain unchanged; this helper
// only exposes which physical Pattern step produced each projected onset.
PatternProjectionStatus projectPatternToRuntimeEventsWithSourceSteps(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination,
    uint8_t (&sourceSteps)[SynthPattern::kSteps]);

'''
require(
    header_text.count(companion_api) == 1,
    "P1C/P2 source-step companion API missing, duplicated, or changed unexpectedly",
)
canonical_header = subprocess.run(
    ["git", "show", f"{P1C_TIP}:src/phrase/runtime_synth_events.h"],
    cwd=ROOT,
    check=True,
    text=True,
    capture_output=True,
).stdout
header_without_companion = header_text.replace(companion_api, "", 1)
require(
    header_without_companion == canonical_header,
    "P1C public runtime-event ABI/declaration surface changed outside the reviewed additive source-step companion API",
)

text = header_text + "\n" + source_text
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

# Projection remains deterministic/pure. Runtime owns ghost/probability RNG;
# the projector may only classify whether a future onset is guaranteed.
for forbidden in (
    "rand(",
    "random(",
    "esp_random",
):
    require(forbidden not in source_text,
            f"P1C/P2 projector must not consume runtime RNG: {forbidden}")

for required in (
    "RuntimeSynthEvent",
    "RuntimeSynthEventBuffer",
    "PatternProjectionSettings",
    "PatternProjectionStatus",
    "projectPatternToRuntimeEvents",
    "projectPatternToRuntimeEventsWithSourceSteps",
    "kTicksPerBar",
    "kSubticksPerTick",
    "kMaxPhraseBars",
    "kMaxSynthEvents",
):
    require(required in text, f"missing P1C contract token: {required}")

# If P2 has changed the canonical projector implementation, pin the reviewed
# compatibility rule rather than allowing arbitrary implementation drift.
source_changed = subprocess.run(
    ["git", "diff", "--quiet", P1C_TIP, "HEAD", "--",
     "src/phrase/runtime_synth_events.cpp"],
    cwd=ROOT,
).returncode != 0
if source_changed:
    for required in (
        "isGuaranteedOnset",
        "!step.ghost",
        "step.probability >= 100",
        "if (tokenTime >= end) break;",
        "if (tokenTime > end) break;",
    ):
        require(required in source_text,
                f"missing reviewed P2 conditional-lifetime compatibility rule: {required}")

print("P1C source contract: OK")
