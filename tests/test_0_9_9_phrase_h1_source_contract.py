#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "f63db9bbc18db32a9cf494dddd6610a3cc403a1b"

progression_h = (ROOT / "src/generation/roles/chord_progression.h").read_text()
progression_cpp = (ROOT / "src/generation/roles/chord_progression.cpp").read_text()
migration_cpp = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
timeline_h = (ROOT / "src/generation/composition/phrase_harmonic_timeline.h").read_text()

# H1 is research/contract only. Production semantic execution belongs to P1.
delta = subprocess.check_output(
    ["git", "diff", "--name-only", f"{BASE}...HEAD", "--", "src/"],
    cwd=ROOT,
    text=True,
).strip()
assert delta == "", f"PHRASE-H1 must have zero src delta, got: {delta}"

# ChordProgression already owns WHAT and already owns cyclic grammar lookup.
assert "struct Grammar" in progression_cpp
assert "const Grammar* selectGrammar" in progression_cpp
assert "selected->events[index % selected->count]" in progression_cpp

# Source identity is deterministic and does not depend on materialized eventCount.
assert "request.generation" in progression_cpp
assert "request.phraseBars" in progression_cpp
assert "request.requestedId" in progression_cpp
select_grammar = progression_cpp.split("const Grammar* selectGrammar", 1)[1].split(
    "bool validEvent", 1
)[0]
assert "harmonicEventCount" not in select_grammar

# The bounded plan remains an 8-event carrier; H1 does not enlarge it.
assert "constexpr uint8_t kMaxHarmonicEvents = 8;" in progression_h
assert "HarmonicEvent events[kMaxHarmonicEvents]{};" in progression_h

# C1 owns WHEN-only phrase coordinates up to 32 positions and stores no WHAT.
assert "kMaxPhraseHarmonicEventPositions" in timeline_h
assert "kMaxSemanticPhraseBars = 8" in timeline_h
assert "HarmonicEvent" not in timeline_h

# Frozen single-bar migration is the old consumer reset, not the WHAT owner.
assert "progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);" in migration_cpp
assert "progressionRequest.phraseBars = 1;" in migration_cpp

# Physical destination is not part of the ChordProgression request/source key.
request_struct = progression_h.split("struct ChordProgressionRequest", 1)[1].split(
    "};", 1
)[0]
for forbidden in ("patternAddress", "song", "storage", "physical"):
    assert forbidden not in request_struct

print("PHRASE-H1 source contract: owner=ChordProgression grammar")
print("PHRASE-H1 production src delta: ZERO")
