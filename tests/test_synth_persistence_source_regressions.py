#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")

required = [
    "kSynthStateSchemaVersion = 1",
    "PersistedSynthPatch",
    "kMaxParams = 6",
    '"synthState"',
    '"aType"', '"bType"', '"aCount"', '"bCount"',
    "hasVersionedSynthState()",
    "legacySynthParametersPresent",
]
for token in required:
    if token not in scenes_h and token not in scenes_cpp:
        raise AssertionError(f"missing synth persistence contract: {token}")

writer = scenes_h[scenes_h.index("bool SceneManager::writeSceneJson"):]
if '\"synthParams\"' in writer:
    raise AssertionError("new streaming writer must not reserialize legacy synthParams")
if '"synthState"' not in scenes_cpp:
    raise AssertionError("document writer must emit versioned synthState")

apply_start = engine.index("void MiniAcid::applySceneStateFromManager()")
apply_end = engine.index("void MiniAcid::applyTextureFromScene_()", apply_start)
apply = engine[apply_start:apply_end]
for forbidden in ("genreManager_.applyGenreTimbre(*this)", "genreManager_.applyTexture(*this)"):
    if forbidden in apply:
        raise AssertionError(f"normal Scene Load still rewrites patch: {forbidden}")
if "TextureMode" in engine and "syncTextureBiasBaselineFromCurrentState" not in apply:
    raise AssertionError("load must synchronize texture bias while TextureMode exists")
if "if (sceneManager_.hasVersionedSynthState())" not in apply:
    raise AssertionError("versioned synth state is not authoritative on load")
if "legacySynthParametersPresent(idx)" not in apply:
    raise AssertionError("legacy non-TB defaults cannot distinguish missing params")

select_pos = apply.index("setSynthEngine(idx, patch.engineName)")
params_pos = apply.index("synthVoices_[idx]->setState(runtimeState)")
fx_pos = apply.index("distortion303.setEnabled")
if not (select_pos < params_pos < fx_pos):
    raise AssertionError("load order must be TYPE -> normalized params -> DST/DLY")

sync_start = engine.index("void MiniAcid::syncSceneStateToManager()")
sync = engine[sync_start:]
if "getState()" not in sync or "setSynthPatch" not in sync:
    raise AssertionError("save path must capture normalized SwappableSynthVoice state")

load_start = project.index("bool ProjectPage::loadSceneAtSelection()")
load_end = project.index("void ProjectPage::randomizeSaveName()", load_start)
load_block = project[load_start:load_end]
if "if (loaded)" not in load_block or "markSceneLoadSucceeded()" not in load_block:
    raise AssertionError("successful explicit Load must establish clean revision baseline")
if "lastSceneLoadRecoveredAutosave()" not in load_block or "markSceneMutated()" not in load_block:
    raise AssertionError("recovery load must remain dirty rather than impersonating explicit Load")
if load_block.index("markSceneMutated()") > load_block.index("markSceneLoadSucceeded()"):
    raise AssertionError("recovery branch must be handled before explicit Load success hook")

save_start = project.index("bool ProjectPage::saveCurrentScene()")
save_end = project.index("bool ProjectPage::createNewScene()", save_start)
save_block = project[save_start:save_end]
for token in ("sceneRevisionSnapshot()", "if (saved)", "markSceneSaveSucceeded()", "restoreSceneRevision(revisionBefore)"):
    if token not in save_block:
        raise AssertionError(f"Save revision result contract missing: {token}")

recovery_start = engine.index("bool MiniAcid::autoSaveSceneRecovery()")
recovery_end = engine.index("float MiniAcid::mainVolume()", recovery_start)
recovery_block = engine[recovery_start:recovery_end]
if "markSceneSaveSucceeded" in recovery_block or "markSceneLoadSucceeded" in recovery_block:
    raise AssertionError("recovery/autosave must not alter explicit persistence revision baseline")

print("synth persistence/load ownership source regressions: OK")
