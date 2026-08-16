#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PUBLIC = (ROOT / "src/generation/migration/quantized_generation_commit.h").read_text()
OWNER_IMPL = (ROOT / "src/generation/migration/quantized_generation_undo_owner_impl.h").read_text()
LEGACY_IMPL = (ROOT / "src/generation/migration/quantized_generation_commit_impl.h").read_text()
GENRE_PAGE = (ROOT / "src/ui/pages/genre_page.cpp").read_text()

required = [
    "GenerationUndoPayload",
    "GroovePuterUndo::undoOwner().commitPrepared",
    "GroovePuterUndo::UndoKind::Generation",
    "undoLastQuantizedGeneration",
    "preparePlayingCandidate(",
    "prepareSynthCandidate(",
]
for needle in required:
    assert needle in OWNER_IMPL, f"missing canonical generation owner contract: {needle}"

assert "quantized_generation_undo_owner_impl.h" in PUBLIC
assert "legacyRegenerateWithQuantizedCommit" in PUBLIC
assert "legacyRegenerateSynthWithQuantizedCommit" in PUBLIC

# The accepted A implementation is retained only as a compile-time reference.
# The selected B implementation owns revision via UndoOwner and never performs
# generation inside the bounded commit callback.
assert "GroovePuterState::markSceneMutated" not in OWNER_IMPL
commit_block = OWNER_IMPL.split("inline bool commitPreparedGeneration", 1)[1]
commit_block = commit_block.split("inline bool validateGenerationUndo", 1)[0]
for forbidden in (
    "generatePattern(",
    "generateDrumPattern(",
    "migrateStrongRhythm",
    "regeneratePatternsWithGenre",
):
    assert forbidden not in commit_block, f"COMMIT performs PREPARE work: {forbidden}"

# GenrePage may still own PROFILE ONLY mutation, but a completed generation
# must not receive a second page-level revision after UndoOwner committed it.
apply_block = GENRE_PAGE.split("void GenrePage::applyCurrent", 1)[1]
apply_block = apply_block.split("void GenrePage::updateFromEngine", 1)[0]
assert "if (changed && !doRegenerate)" in apply_block
assert "generationResult == GroovePuterRhythm::QuantizedGenerationResult::CommittedNow" not in apply_block

# Historical direct mutation remains textually present in the renamed reference
# implementation, but it is no longer selected by the public header.
assert "GroovePuterState::markSceneMutated();" in LEGACY_IMPL
print("0.9.9-B generation UndoOwner source contracts passed")
