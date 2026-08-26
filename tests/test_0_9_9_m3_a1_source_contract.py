#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "5ad44bb9400ea38d349b7f815f84f833fb18ce6a"

changed = subprocess.check_output(
    ["git", "diff", "--name-only", BASE, "HEAD"], cwd=ROOT, text=True
).splitlines()

allowed_paths = (
    ".github/workflows/research-0-9-9-m3-a1-harmonic-crossing.yml",
    "docs/audits/M3_A1_HARMONIC_CROSSING.md",
    "tests/run_0_9_9_m3_a1_tests.sh",
    "tests/run_generation_stage13_tests.sh",
    "tests/test_0_9_9_m3_a1_harmonic_crossing.cpp",
    "tests/test_0_9_9_m3_a1_source_contract.py",
)

unexpected = [path for path in changed if path not in allowed_paths]
assert not unexpected, f"M3-A1 unexpected delta: {unexpected}"
assert not any(path.startswith("src/") for path in changed), changed

migration = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
migration_header = (ROOT / "src/generation/migration/strong_rhythm_migration.h").read_text()
tonal_header = (ROOT / "src/generation/tonal/tonal_materializer.h").read_text()
progression_header = (ROOT / "src/generation/roles/chord_progression.h").read_text()
stage13_runner = (ROOT / "tests/run_generation_stage13_tests.sh").read_text()

# Exact frozen-M1 characterization: harmonic timing is still local ChordRhythm
# timing, and progression is re-realized as one bar.
assert "progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);" in migration
assert "progressionRequest.phraseBars = 1;" in migration
assert "chord.plan.onsets, melodicPitch.plan.onsets" in migration

# M1 carries a logical phrase bar coordinate and frozen phrase identity.
assert "uint8_t phraseBarOrdinal = kUnspecifiedPhraseBarOrdinal;" in migration_header
assert "uint32_t phraseGenerationIdentity = 0;" in migration_header
assert "context.phraseBarOrdinal != kUnspecifiedPhraseBarOrdinal" in migration
assert "phraseVocabularyBarOrdinal(context.phraseBarOrdinal)" in migration

# The local tonal request has no phrase-wide harmonic source coordinate/carrier.
assert "StepMask harmonicEventOnsets = 0;" in tonal_header
assert "ChordProgressionPlan progression{};" in tonal_header
for forbidden in (
    "phraseBarOrdinal",
    "phraseHarmonicPosition",
    "previousHarmonic",
    "harmonicSourceOrdinal",
    "PhraseHarmonicTimeline",
):
    assert forbidden not in tonal_header, forbidden

# ChordProgression is bounded and has no event-base/source-offset coordinate.
assert "constexpr uint8_t kMaxHarmonicEvents = 8;" in progression_header
assert "uint8_t phraseBars = 1;" in progression_header
for forbidden in (
    "harmonicSourceOrdinal",
    "eventBaseOrdinal",
    "previousHarmonic",
    "PhraseHarmonicTimeline",
):
    assert forbidden not in progression_header, forbidden

# F08/F08.1 are parallel evidence, not silently merged into the M1 base.
assert not (ROOT / "src/generation/roles/harmonic_rhythm.h").exists()

# The focused executable characterization is reached from the pre-existing
# Core regressions pull-request workflow via its Stage 13/14 entrypoint.
assert 'bash "${ROOT_DIR}/tests/run_0_9_9_m3_a1_tests.sh"' in stage13_runner

print("M3-A1 source contract: zero src delta; executable audit gate wired")
