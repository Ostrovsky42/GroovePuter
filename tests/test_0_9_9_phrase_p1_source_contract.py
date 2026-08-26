#!/usr/bin/env python3
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "74456bcfec0fc74138ec0d8c652dde642c7e16b6"


def text(path: str) -> str:
    return (ROOT / path).read_text()


def strip_cpp(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    source = re.sub(r"//.*", "", source)
    source = re.sub(r'"(?:\\.|[^"\\])*"', '""', source)
    source = re.sub(r"'(?:\\.|[^'\\])*'", "''", source)
    return source

# Decision-B characterization must not smuggle a production implementation in.
src_delta = subprocess.check_output(
    ["git", "diff", "--name-only", f"{BASE}...HEAD", "--", "src/"],
    cwd=ROOT,
    text=True,
).strip()
assert src_delta == "", f"P1 blocker checkpoint must have zero src delta: {src_delta}"

profile_h = strip_cpp(text("src/generation/composition/generation_profile.h"))
profile_cpp = text("src/generation/composition/generation_profile.cpp")
timeline_h = strip_cpp(text("src/generation/composition/phrase_harmonic_timeline.h"))
migration_cpp = strip_cpp(text("src/generation/migration/strong_rhythm_migration.cpp"))
tonal_h = strip_cpp(text("src/generation/tonal/tonal_materializer.h"))
semantic_h = strip_cpp(text("src/generation/migration/phrase_semantic_result.h"))

# Exact 8-bar phrase admission exists; the blocker is not M4 length capacity.
assert "kPhraseSlow" in profile_cpp
assert "PhraseEvolutionLawId::SparseDrift, 8" in profile_cpp

# C1 provides only a bounded WHEN representation fed by caller-owned masks.
assert "makePhraseHarmonicTimeline" in timeline_h
assert "eventPositionsByBar" in timeline_h
assert "PhraseHarmonicTimeline harmonicTimeline" in semantic_h

# Frozen production profile/composition has no separate harmonic WHEN owner.
for owner_token in (
    "HarmonicRhythm",
    "HarmonicClock",
    "harmonicRhythm",
    "harmonicClock",
):
    assert owner_token not in profile_h, (
        f"unexpected production harmonic WHEN owner in GenerationProfile: {owner_token}"
    )

# Existing StrongRhythm is still the legacy one-bar consumer and collapses
# harmonic timing onto ChordRhythm articulation. Reusing this in P1 would violate
# the frozen ownership split rather than implement it.
assert "progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);" in migration_cpp
assert "progressionRequest.phraseBars = 1;" in migration_cpp
assert "chord.plan.onsets" in migration_cpp
assert "request.harmonicEventOnsets = harmonicEventOnsets;" in migration_cpp

# TonalMaterializer stays bar-local and receives a finite harmonic view; it does
# not own phrase-wide harmonic timing.
assert "StepMask harmonicEventOnsets" in tonal_h
assert "ChordProgressionPlan progression" in tonal_h
assert "PhraseHarmonicTimeline" not in tonal_h

# P1 must not introduce forbidden downstream owners while stopped at Decision B.
changed = subprocess.check_output(
    ["git", "diff", "--name-only", f"{BASE}...HEAD"], cwd=ROOT, text=True
).splitlines()
for path in changed:
    assert not path.startswith("src/ui/"), path
    assert not path.startswith("src/midi/"), path
    assert "song" not in path.lower(), path
    assert "bank" not in path.lower(), path

print("P1 source guard: production src delta=ZERO")
print("P1 source guard: exact_8bar_phrase_admission=YES")
print("P1 source guard: phrase_wide_harmonic_when_owner=ABSENT")
print("P1 source guard: chord_articulation_cannot_be_promoted_to_when_owner")
print("P1 source guard: forbidden_publication_runtime_policy=ABSENT")
