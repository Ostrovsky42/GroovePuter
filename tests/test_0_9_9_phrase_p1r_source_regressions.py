#!/usr/bin/env python3
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"P1R SOURCE GUARD FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


# P1R has accumulated accepted descendants (GF2, Pattern/Phrase runtime and UI
# closure work), so repository-wide git-diff equality against the original P1R
# checkpoint is no longer a live contract. Keep the actual ownership firewall:
# the execution seam must remain bounded, caller-owned, storage-free and must
# still consume the finalized H1-F1 WHAT source rather than a finite consumer
# plan as its progression source.
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
    "chordProgressionEventAt(source, globalHarmonicOrdinal)",
    "ChordProgressionEventResult",
    "firstGlobalHarmonicOrdinal",
    "PreparedPhraseExecution",
    "preparePhraseExecution(",
    "materializePreparedPhraseBar(",
]
for fragment in required_fragments:
    if fragment not in production_text:
        fail(f"required live P1R seam missing: {fragment}")

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

if "std::is_trivially_copyable<PreparedPhraseExecution>" not in execution_h:
    fail("PreparedPhraseExecution fixed-capacity contract is missing")
if "phraseBarOrdinal >= prepared.length.effectivePhraseBars" not in execution_cpp:
    fail("random-access phrase materialization lost its explicit length guard")

print("P1R source firewall: OK")
print("P1R live semantic seam invariants: OK")
print("P1R finalized H1-F1 WHAT-source contract: active")
print("P1R heap/publication guards: OK")
