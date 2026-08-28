#!/usr/bin/env python3
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
P1R = "a413561136b274a1b16b079f95f8d3ce3353fac5"

production = {
    "src/dsp/generated_phrase_p1r_materializer.h",
    "src/dsp/generated_phrase_song.h",
    "src/ui/pages/pattern_edit_page.h",
    "src/ui/pages/synth_sequencer_page.cpp",
}

changed_src = subprocess.check_output(
    ["git", "diff", "--name-only", P1R + "...HEAD", "--", "src/"],
    cwd=ROOT,
    text=True,
).splitlines()
assert set(changed_src) == production, f"unexpected I1 production delta: {changed_src}"

protected = [
    "src/generation/migration/phrase_execution.h",
    "src/generation/migration/phrase_execution.cpp",
    "src/generation/migration/strong_rhythm_migration.h",
    "src/generation/migration/strong_rhythm_migration.cpp",
    "src/generation/roles/chord_progression.h",
    "src/generation/roles/chord_progression.cpp",
    "src/generation/roles/harmonic_rhythm.h",
    "src/generation/composition/phrase_harmonic_clock_projection.h",
    "src/generation/migration/phrase_live_arrangement_activation.h",
    "src/generation/migration/quantized_generation_commit.h",
    "src/generation/migration/quantized_generation_commit_impl.h",
    "src/dsp/miniacid_engine.h",
    "src/dsp/miniacid_engine.cpp",
]
subprocess.run(
    ["git", "diff", "--exit-code", P1R, "--", *protected],
    cwd=ROOT,
    check=True,
)

helper = (ROOT / "src/dsp/generated_phrase_p1r_materializer.h").read_text()
song = (ROOT / "src/dsp/generated_phrase_song.h").read_text()
pattern_h = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text()
synth_cpp = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text()
combined = "\n".join((helper, song, pattern_h, synth_cpp))

assert "kLogicalPhraseAttemptChannel = 0xFFFF" in helper
assert "kMaxGlobalPatterns < kLogicalPhraseAttemptChannel" in helper
assert "allocateGenerationAttempt(" not in helper, (
    "repeatable P1R PREPARE must not consume session attempt state"
)
assert "generationAttemptOrdinal" in helper
assert "attemptAvailable" in helper
assert "phraseSeed(" not in helper
assert "songStart" not in helper
assert "PreparedPhraseExecution" in helper
assert "preparePhraseExecution(" in helper
assert "materializePreparedPhraseBar(" in helper
assert "execution.selection.realizationGeneration.projectSeed" in helper
assert "return PreparationDisposition::LegacyRoute;" in helper

assert "GeneratedPhraseP1R::prepare(" in song
assert "prepareWithGenerationAttempt(" in song
assert "engine, bars, songStart, 0u, false, prepared" in song, (
    "direct PREPARE must use deterministic non-consuming preview attempt 0"
)
assert re.search(
    r"allocateGenerationAttempt\s*\([^;]*GeneratedPhraseP1R::kLogicalPhraseAttemptChannel\s*\)",
    song,
    re.S,
), "real G request must allocate through the reserved logical channel"
assert (
    "p1rDisposition == GeneratedPhraseP1R::PreparationDisposition::Failed"
    in song
)
assert (
    "p1rDisposition == GeneratedPhraseP1R::PreparationDisposition::Ready"
    in song
)
assert "Legacy strong-rhythm routes retain the frozen D2 physical preparer exactly." in song
assert "PHRASE LENGTH REJECTED" in song
assert "forceSingleBarRows = true" in song
assert "armPhraseActivation(" in song
assert "completePhraseActivation(" in song
assert "UndoKind::Generation" in song
assert "applyPreparedPersistent(" in song

allocation_start = song.index(
    "const auto attempt = GroovePuterState::allocateGenerationAttempt("
)
allocation_end = song.index("if (!attempt.ok())", allocation_start)
allocation_slice = song[allocation_start:allocation_end]
generate_start = song.index("Result generate(")
prepare_preview_start = song.index("inline bool prepare(\n")
assert allocation_start > generate_start, (
    "session attempt allocation must live inside the real generate request"
)
assert allocation_start > prepare_preview_start, (
    "repeatable PREPARE must precede and remain outside attempt allocation"
)
for token in ("pageIndex", "firstLocalSlot", "songStart"):
    assert token not in allocation_slice, f"physical destination leaked into identity: {token}"
assert "GeneratedPhraseP1R::kLogicalPhraseAttemptChannel" in allocation_slice
assert "generationAttemptOrdinal = attempt.ordinal;" in song
assert "attemptAvailable = true;" in song

assert "syncSongPatternContext" in pattern_h
assert "current303PatternIndex(voice_index_)" in pattern_h
assert "current303BankIndex(voice_index_)" in pattern_h
assert "pattern_page_->syncSongPatternContext();" in synth_cpp

for forbidden in (
    "MelodicCrossBarLifetime",
    "A_ONSET",
    "A_CONTINUATION",
):
    assert forbidden not in combined, f"I1 crossed C2/R1 lifetime boundary: {forbidden}"

for forbidden_path_token in (
    "phrase_c2",
    "phrase_r1",
    "phrase_i2",
):
    assert forbidden_path_token not in combined.lower(), forbidden_path_token

for forbidden in (
    "std::queue",
    "std::deque",
    "new Song",
    "g_i1Pending",
    "g_phraseI1Pending",
):
    assert forbidden not in combined, f"second runtime owner introduced: {forbidden}"

print("I1 source guard: production owner set=EXACT")
print("I1 source guard: P1R/H1/W1R/H2R owners=UNCHANGED")
print("I1 source guard: D2 transport/activation owner=REUSED")
print("I1 source guard: PREPARE attempt state mutation=NO")
print("I1 source guard: real G attempt owner=EXISTING_SESSION_OWNER")
print("I1 source guard: logical phrase identity independent of destination=YES")
print("I1 source guard: P1R typed reject legacy fallback=NO")
print("I1 source guard: C2/R1 lifetime policy imported=NO")
print("I1 source guard: Synth NOTES follows authoritative Song selection=YES")
