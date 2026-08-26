#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = "5ad44bb9400ea38d349b7f815f84f833fb18ce6a"

changed = subprocess.check_output(
    ["git", "diff", "--name-only", BASE, "HEAD"], cwd=ROOT, text=True
).splitlines()

allowed_paths = {
    "docs/audits/M4_A1_PHRASE_LENGTH_OWNERSHIP.md",
    "tests/run_generation_stage13_tests.sh",
    "tests/test_0_9_9_m4_a1_phrase_coordinates.cpp",
    "tests/test_0_9_9_m4_a1_source_contract.py",
}
unexpected = [path for path in changed if path not in allowed_paths]
assert not unexpected, f"M4-A1 unexpected delta: {unexpected}"
assert not any(path.startswith("src/") for path in changed), changed

profile_header = (
    ROOT / "src/generation/composition/generation_profile.h"
).read_text(encoding="utf-8")
profile_cpp = (
    ROOT / "src/generation/composition/generation_profile.cpp"
).read_text(encoding="utf-8")
migration_cpp = (
    ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
).read_text(encoding="utf-8")
live_header = (
    ROOT / "src/generation/migration/strong_rhythm_live_bridge.h"
).read_text(encoding="utf-8")
live_cpp = (
    ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp"
).read_text(encoding="utf-8")
scene = (ROOT / "scenes.h").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    assert start >= 0, f"missing function signature: {signature}"
    brace = source.find("{", start)
    assert brace >= 0, f"missing function body: {signature}"
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated function body: {signature}")


# FEEL/transport has its own persisted 1/2/4/8 request, but composition does
# not read it and does not accept a physical pattern address as phrase policy.
assert "uint8_t patternBars = 1;" in scene
for path in (ROOT / "src/generation/composition").iterdir():
    if path.suffix not in {".h", ".cpp"}:
        continue
    text = path.read_text(encoding="utf-8")
    assert "patternBars" not in text, path
    assert "patternAddress" not in text, path

# resolveGenerationComposition remains the current phraseBars policy owner:
# profile PhraseLawSelection chooses a packed (law,bars) candidate, and the
# resolver alone decodes that selected candidate into result.phraseBars.
assert "GenerationCompositionResult resolveGenerationComposition(" in profile_header
assert "GenerationDomain::PhraseLawSelection" in profile_cpp
assert "result.phraseLaw = static_cast<PhraseEvolutionLawId>(phraseChoice >> 4u);" in profile_cpp
assert "result.phraseBars = static_cast<uint8_t>(phraseChoice & 0x0Fu);" in profile_cpp
assert "scene.feel.patternBars" not in profile_cpp
assert "patternAddress" not in profile_cpp

# Ordinary strong-rhythm production remains one-bar even though the selected
# composition result carries planning phraseBars metadata.
assert "result.phraseBars = composition.phraseBars;" in migration_cpp
assert "request.phraseBars = 1;" in migration_cpp
assert "progressionRequest.phraseBars = 1;" in migration_cpp
assert "scene.feel.patternBars" not in migration_cpp

genre_g = function_body(
    live_cpp,
    "StrongRhythmMigrationResult regenerateWithStrongRhythmMigration("
)
drums_g = function_body(
    live_cpp,
    "StrongRhythmMigrationResult regenerateDrumsWithStrongRhythmMigration("
)
for body in (genre_g, drums_g):
    assert "patternBars" not in body
assert "migrateStrongRhythmMaterial(" in genre_g
assert "migrateStrongRhythmDrums(" in drums_g
assert "It never replaces normal G." in live_header

# CurrentWired phrase audition deliberately has two independent observations:
# requestedBars comes from FEEL/transport state, while profileBars comes from
# the independently resolved composition selection. Neither assignment feeds
# the other.
audition = function_body(
    live_cpp,
    "PhraseAuditionResult regeneratePhraseAuditionWithProbe("
)
requested_assignment = (
    "result.requestedBars = m1ListeningCase(listeningCase)\n"
    "      ? kGrooveVocabularyPhraseBars\n"
    "      : normalizedPhraseBars(scene.feel.patternBars);"
)
profile_assignment = (
    "result.profileBars = normalizedPhraseBars(selection.phraseBars);"
)
assert requested_assignment in audition
assert profile_assignment in audition
assert "selection.phraseBars" not in requested_assignment
assert "scene.feel.patternBars" not in profile_assignment

print("M4-A1 source contract: PASS")
print("  composition_owner=resolveGenerationComposition")
print("  plain_G_phraseBars=1")
print("  patternBars_owner=FEEL_transport_not_composition")
print("  patternAddress_phrase_length_owner=NO")
print("  audition_requestedBars_vs_profileBars=INDEPENDENT")
