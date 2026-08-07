#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/dsp/genre_manager.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/dsp/genre_manager.h").read_text(encoding="utf-8")
engine = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
page = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")

start = source.index("void GenreSceneView::applyGenreTimbre")
end = source.index("void GenreSceneView::applyTexture", start)
block = source[start:end]

required = (
    "recipe() == 6 || recipe() == 7",
    "recipe() >= 8 && recipe() <= 11",
    'engine.setSynthEngine(0, "TB303")',
    'engine.setSynthEngine(1, "TB303")',
    'engine.setSynthEngine(1, "OPL2")',
    'engine.currentSynthEngineName(v) != "TB303"',
)
for item in required:
    if item not in block:
        raise AssertionError(f"missing Atlas sound-profile invariant: {item}")

engine_switch = block.index('engine.setSynthEngine(0, "TB303")')
parameter_write = block.index("engine.set303ParameterNormalized")
if engine_switch >= parameter_write:
    raise AssertionError("preview engines must be selected before parameter writes")

texture_start = source.index("void GenreSceneView::applyTexture")
texture = source[texture_start:]
if texture.count('currentSynthEngineName(voice) == "TB303"') < 2:
    raise AssertionError("texture biases must not write TB303 parameters into OPL2/SID/AY")

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

for required_scene_write in (
    "settings.generativeMode =",
    "settings.recipe =",
    "settings.morphTarget =",
    "settings.morphAmount =",
    "GenreCatalog::grooveboxModeForRecipe",
):
    if required_scene_write not in page:
        raise AssertionError(f"Genre page Scene write missing: {required_scene_write}")
if ".genreManager()" in page:
    raise AssertionError("Genre page must not edit genre state through a manager facade")

apply_start = page.index("void GenrePage::applyCurrent()")
apply_end = page.index("void GenrePage::updateFromEngine()", apply_start)
apply_block = page[apply_start:apply_end]
if apply_block.count("GroovePuterState::markSceneMutated();") != 1:
    raise AssertionError("Genre APPLY must produce exactly one Scene revision")

print("Atlas sound profile and Scene ownership regression: OK")
