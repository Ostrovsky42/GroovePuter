#!/usr/bin/env python3
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = "0f694187fa65e51c08468ce8b28ed88bb6bb8699"

ALLOWED_PRODUCTION = {
    "src/generation/migration/strong_rhythm_migration.h",
    "src/generation/migration/strong_rhythm_migration.cpp",
    "src/generation/migration/phrase_execution.h",
    "src/generation/migration/phrase_execution.cpp",
}

FROZEN_OWNERS = [
    "src/generation/roles/chord_progression.h",
    "src/generation/roles/chord_progression.cpp",
    "src/generation/roles/harmonic_rhythm.h",
    "src/generation/composition/phrase_harmonic_clock_projection.h",
    "src/generation/composition/phrase_harmonic_timeline.h",
    "src/generation/migration/phrase_semantic_result.h",
    "src/generation/composition/phrase_length_request.h",
    "src/generation/composition/phrase_length_request.cpp",
    "src/generation/composition/generation_profile.h",
    "src/generation/composition/generation_profile.cpp",
]


def git(*args: str) -> str:
    return subprocess.check_output(
        ["git", *args], cwd=ROOT, text=True
    ).strip()


def fail(message: str) -> None:
    print(f"P1R SOURCE GUARD FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


changed_src = {
    line for line in git("diff", "--name-only", BASE, "HEAD", "--", "src/generation").splitlines()
    if line
}
unexpected = changed_src - ALLOWED_PRODUCTION
if unexpected:
    fail("unexpected production source delta: " + ", ".join(sorted(unexpected)))

for owner in FROZEN_OWNERS:
    if git("diff", "--name-only", BASE, "HEAD", "--", owner):
        fail(f"frozen semantic owner drifted: {owner}")

strong_h = (ROOT / "src/generation/migration/strong_rhythm_migration.h").read_text()
strong_cpp = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
execution_h = (ROOT / "src/generation/migration/phrase_execution.h").read_text()
execution_cpp = (ROOT / "src/generation/migration/phrase_execution.cpp").read_text()
production_text = "\n".join((strong_h, strong_cpp, execution_h, execution_cpp))

required_fragments = [
    "StrongRhythmPhraseExecutionOverride",
    "phraseExecutionOverride",
    "resolveStrongRhythmFrozenSelectionForPhraseBars",
    "resolveGenerationCompositionForPhraseBars",
    "if (context.phraseExecutionOverride == nullptr)",
    "chordProgressionSourceEventAt(",
    "execution.firstGlobalHarmonicOrdinal + ordinal",
    "PreparedPhraseExecution",
    "preparePhraseExecution(",
    "materializePreparedPhraseBar(",
]
for fragment in required_fragments:
    if fragment not in production_text:
        fail(f"required P1R seam missing: {fragment}")

for forbidden in (
    "globalOrdinal % progression.plan.eventCount",
    "phraseGlobalHarmonicOrdinal % progression.plan.eventCount",
    "firstGlobalHarmonicOrdinal % progression.plan.eventCount",
):
    if forbidden in production_text:
        fail(f"finite consumer plan used as WHAT source: {forbidden}")

heap_patterns = [
    r"\bmalloc\s*\(",
    r"\bcalloc\s*\(",
    r"\brealloc\s*\(",
    r"\bfree\s*\(",
    r"\bnew\s+[A-Za-z_:]",
    r"\bdelete\s+",
]
for pattern in heap_patterns:
    if re.search(pattern, production_text):
        fail(f"heap operation present in P1R production source: {pattern}")

publication_patterns = [
    r"\bSong\b",
    r"\bBank\s*<",
    r"\bScene\b",
    r"\bactiveSongSlot\b",
    r"\bdrumBanks\b",
    r"\bsynthABanks\b",
    r"\bsynthBBanks\b",
    r"\bsceneTransactionScratch\b",
    r"\bsongPatternFrom",
]
for pattern in publication_patterns:
    if re.search(pattern, execution_h + "\n" + execution_cpp):
        fail(f"publication/storage owner leaked into phrase execution: {pattern}")

if "PhraseExecutionScratch" not in execution_h:
    fail("caller-owned single-bar semantic scratch is missing")
if re.search(r"(DrumPatternSet|SynthPattern)\s+\w+\s*\[\s*kMaxSemanticPhraseBars\s*\]", execution_h + execution_cpp):
    fail("P1R retains a physical N-bar array")

print("P1R source firewall: OK")
print("P1R frozen semantic owners: unchanged")
print("P1R heap/publication guards: OK")
