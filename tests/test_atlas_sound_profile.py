#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/dsp/genre_manager.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/dsp/genre_manager.h").read_text(encoding="utf-8")
engine = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
page = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")

# Genre/Atlas metadata may constrain generation, but it must not project a
# physical synth engine or overwrite the persisted user patch.
for path, text in (
    ("genre_manager.h", header),
    ("genre_manager.cpp", source),
    ("genre_page.cpp", page),
):
    if "applyGenreTimbre" in text:
        raise AssertionError(f"dead genre timbre projection API returned: {path}")
if "setSynthEngine(" in source or "set303ParameterNormalized" in source:
    raise AssertionError("GenreSceneView must not project synth TYPE or parameters")
if '"miniacid_engine.h"' in source:
    raise AssertionError("genre catalog/view must not depend on synth engine runtime")

for removed_texture_symbol in (
    "TextureMode",
    "TextureParams",
    "kTexturePresets",
    "applyTexture(",
    "setTextureMode(",
    "textureMode()",
):
    if removed_texture_symbol in header or removed_texture_symbol in source:
        raise AssertionError(
            f"removed TextureMode runtime symbol returned: {removed_texture_symbol}"
        )

for forbidden in (
    "class GenreManager",
    "struct GenreState",
    "GenreState state_",
    "cachedDirty_",
    "cachedGenerativeParams_",
    "cachedDrumOverride_",
    "pendingRecipe_",
    "pendingMorphTarget_",
    "pendingMorphAmount_",
):
    if forbidden in header or forbidden in source:
        raise AssertionError(f"genre ownership layer returned: {forbidden}")

for required_owner_boundary in (
    "class GenreSceneView",
    "explicit GenreSceneView(SceneManager& scenes)",
    "SceneManager& scenes_",
    "namespace GenreCatalog",
    "compiledGenerativeParams(const GenreSettings& settings)",
    "behavior(const GenreSettings& settings)",
    "grooveRecipe(const GenreSettings& settings)",
):
    if required_owner_boundary not in header:
        raise AssertionError(
            f"Scene-backed genre ownership contract missing: {required_owner_boundary}"
        )

if "GenreManager genreManager_{sceneManager_};" not in engine:
    raise AssertionError("engine genre adapter must be bound to the canonical SceneManager")

# GenrePage now constructs one complete requested GenreSettings transaction.
# STOP/PROFILE commits it to Scene directly; PLAY/full-generation passes the
# same transaction to the bounded quantized owner and BAR_START publishes it.
# Pin that ownership contract instead of depending on the historical local
# variable name `settings` used before quantized generation was introduced.
for required_request_write in (
    "requestedSettings.generativeMode =",
    "requestedSettings.recipe =",
    "requestedSettings.morphTarget =",
    "requestedSettings.morphAmount =",
    "requestedSettings.rhythmSelectionMode =",
    "requestedSettings.rhythmArchetypeId =",
):
    if required_request_write not in page:
        raise AssertionError(
            f"Genre requested-settings transaction missing: {required_request_write}"
        )
if "GenreCatalog::grooveboxModeForRecipe" not in page:
    raise AssertionError("Genre page must still derive the linked Groovebox mode")
if "activeSettings = requestedSettings;" not in page:
    raise AssertionError("Genre PROFILE/STOP path must commit the complete requested settings")
if "regenerateWithQuantizedCommit(\n        mini_acid_, requestedSettings" not in page:
    raise AssertionError("Genre PLAY path must pass the complete requested settings to quantized generation")
if ".genreManager()" in page:
    raise AssertionError("Genre page must not edit genre state through a manager facade")

apply_start = page.index("void GenrePage::applyCurrent(bool forceRegenerate)")
apply_end = page.index("void GenrePage::updateFromEngine()", apply_start)
apply_block = page[apply_start:apply_end]
if "forceRegenerate || applyMode != ApplyMode::ProfileOnly" not in apply_block:
    raise AssertionError("Genre APPLY must preserve Enter ApplyMode and forced-G split")
if apply_block.count("GroovePuterState::markSceneMutated();") != 1:
    raise AssertionError("Genre APPLY must produce exactly one Scene revision")

print("Atlas sound profile and Scene ownership regression: OK")
