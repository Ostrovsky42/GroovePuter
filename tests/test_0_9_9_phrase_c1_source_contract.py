#!/usr/bin/env python3
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = "5ad44bb9400ea38d349b7f815f84f833fb18ce6a"
ALLOWED_SRC = {
    "src/generation/composition/phrase_semantic_contract.h",
    "src/generation/composition/phrase_length_request.h",
    "src/generation/composition/phrase_length_request.cpp",
    "src/generation/composition/phrase_harmonic_timeline.h",
    "src/generation/migration/phrase_semantic_result.h",
}

def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()

changed = {
    line for line in git("diff", "--name-only", f"{BASE}..HEAD", "--", "src").splitlines()
    if line
}
if changed != ALLOWED_SRC:
    print("PHRASE-C1 source guard: unexpected src delta", file=sys.stderr)
    print("expected:", *sorted(ALLOWED_SRC), sep="\n  ", file=sys.stderr)
    print("actual:", *sorted(changed), sep="\n  ", file=sys.stderr)
    raise SystemExit(1)

for path in sorted(ALLOWED_SRC):
    exists_at_base = subprocess.run(
        ["git", "cat-file", "-e", f"{BASE}:{path}"],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    ).returncode == 0
    if exists_at_base:
        print(f"PHRASE-C1 source guard: {path} unexpectedly modified an existing owner", file=sys.stderr)
        raise SystemExit(2)

forbidden_runtime = {
    "src/generation/migration/strong_rhythm_migration.cpp",
    "src/generation/materialization/pattern_materializer.cpp",
    "src/generation/migration/tonal_pattern_adapter.cpp",
    "src/generation/tonal/tonal_materializer.cpp",
    "src/generation/roles/chord_progression.cpp",
    "src/generation/roles/chord_progression.h",
    "scenes.h",
    "scenes.cpp",
}
all_changed = {
    line for line in git("diff", "--name-only", f"{BASE}..HEAD").splitlines() if line
}
for path in forbidden_runtime:
    if path in all_changed:
        print(f"PHRASE-C1 source guard: forbidden live/runtime owner changed: {path}", file=sys.stderr)
        raise SystemExit(3)

combined = "\n".join((ROOT / path).read_text(encoding="utf-8") for path in sorted(ALLOWED_SRC))
for token in ("std::vector", "std::list", "std::deque", "malloc(", "calloc(", "realloc(", "new "):
    if token in combined:
        print(f"PHRASE-C1 source guard: heap/unbounded token found: {token}", file=sys.stderr)
        raise SystemExit(4)

semantic_only = "\n".join(
    (ROOT / path).read_text(encoding="utf-8")
    for path in (
        "src/generation/composition/phrase_semantic_contract.h",
        "src/generation/composition/phrase_harmonic_timeline.h",
        "src/generation/migration/phrase_semantic_result.h",
    )
)
if "patternAddress" in semantic_only:
    print("PHRASE-C1 source guard: physical patternAddress leaked into semantic carrier", file=sys.stderr)
    raise SystemExit(5)

progression = (ROOT / "src/generation/roles/chord_progression.h").read_text(encoding="utf-8")
if "constexpr uint8_t kMaxHarmonicEvents = 8;" not in progression:
    print("PHRASE-C1 source guard: progression WHAT capacity is no longer frozen at 8", file=sys.stderr)
    raise SystemExit(6)

rhythm_types = (ROOT / "src/generation/rhythm/rhythm_types.h").read_text(encoding="utf-8")
if "constexpr uint8_t kMaxPhraseBars = 4;" not in rhythm_types:
    print("PHRASE-C1 source guard: existing rhythm-vocabulary 4-bar capacity changed", file=sys.stderr)
    raise SystemExit(7)

print("PHRASE-C1 source guard: PASS")
print("  src delta=new semantic contract files only")
print("  live/runtime owners unchanged")
print("  progression WHAT capacity=8 unchanged")
print("  rhythm vocabulary capacity=4 unchanged")
print("  heap/unbounded containers=none")
print("  semantic patternAddress ownership=none")
