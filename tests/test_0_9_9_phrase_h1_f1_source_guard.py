#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "74456bcfec0fc74138ec0d8c652dde642c7e16b6"
ALLOWED_PRODUCTION = {
    "src/generation/roles/chord_progression.h",
    "src/generation/roles/chord_progression.cpp",
}


def git_show(path: str) -> str:
    return subprocess.check_output(
        ["git", "show", f"{BASE}:{path}"], cwd=ROOT, text=True
    )


def struct_body(text: str, name: str) -> str:
    return text.split(f"struct {name}", 1)[1].split("};", 1)[0]


production_delta = subprocess.check_output(
    ["git", "diff", "--name-only", f"{BASE}...HEAD", "--", "src/"],
    cwd=ROOT,
    text=True,
).splitlines()
assert set(production_delta) == ALLOWED_PRODUCTION, (
    "PHRASE-H1-F1 production firewall violated: " + repr(production_delta)
)

header_path = ROOT / "src/generation/roles/chord_progression.h"
cpp_path = ROOT / "src/generation/roles/chord_progression.cpp"
header = header_path.read_text(encoding="utf-8")
cpp = cpp_path.read_text(encoding="utf-8")
base_header = git_show("src/generation/roles/chord_progression.h")
base_cpp = git_show("src/generation/roles/chord_progression.cpp")

# Existing finite carrier/public request/result remain byte-identical declarations.
for name in (
    "ChordProgressionRequest",
    "ChordProgressionPlan",
    "ChordProgressionResult",
):
    assert struct_body(header, name) == struct_body(base_header, name), (
        f"legacy declaration changed: {name}"
    )
assert "constexpr uint8_t kMaxHarmonicEvents = 8;" in header

# Grammar vocabulary and authoritative selection implementation are frozen.
assert cpp.split("bool validPhraseBars", 1)[0] == base_cpp.split(
    "bool validPhraseBars", 1
)[0], "grammar vocabulary changed"
assert cpp.split("ProgressionId selectId", 1)[1].split(
    "const Grammar* selectGrammar", 1
)[0] == base_cpp.split("ProgressionId selectId", 1)[1].split(
    "const Grammar* selectGrammar", 1
)[0], "selectId changed"
assert cpp.split("const Grammar* selectGrammar", 1)[1].split(
    "bool validEvent", 1
)[0] == base_cpp.split("const Grammar* selectGrammar", 1)[1].split(
    "bool validEvent", 1
)[0], "selectGrammar changed"

source_request = struct_body(header, "ChordProgressionSourceRequest")
assert "harmonicEventCount" not in source_request
for required in ("requestedId", "family", "generation", "phraseBars"):
    assert required in source_request
for forbidden in ("patternAddress", "song", "storage", "physical"):
    assert forbidden not in source_request

source = struct_body(header, "ChordProgressionSource")
assert "uint8_t period = 0;" in source
assert "events[kMaxChordProgressionSourceEvents]" in source
assert "constexpr uint8_t kMaxChordProgressionSourceEvents = 4;" in header
assert "selected->count" in cpp
assert "source.period = selected->count;" in cpp
assert "globalHarmonicOrdinal % source.period" in cpp
assert "validSourcePeriod" in cpp and "grammarSetFor(source.id)" in cpp

# The source API must reuse the old selection owner rather than cloning tables/seeds.
source_realization = cpp.split("ChordProgressionSourceResult realizeChordProgressionSource", 1)[1].split(
    "ChordProgressionEventResult chordProgressionEventAt", 1
)[0]
assert "selectId(selection)" in source_realization
assert "selectGrammar(selection, id)" in source_realization
assert "deriveGenerationSeed" not in source_realization
assert "deterministicValue" not in source_realization

for forbidden in (
    "std::vector",
    "std::map",
    "std::function",
    "malloc(",
    "calloc(",
    "realloc(",
    "new ",
):
    assert forbidden not in header
    assert forbidden not in cpp

print("PHRASE-H1-F1 production firewall: chord_progression.* ONLY")
print("PHRASE-H1-F1 grammar/selection vocabulary: FROZEN")
print("PHRASE-H1-F1 harmonicEventCount: ABSENT_FROM_SOURCE_REQUEST")
print("PHRASE-H1-F1 heap/dynamic containers: ABSENT")