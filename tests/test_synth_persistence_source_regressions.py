#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")

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

if '\"synthParams\"' in scenes_h[scenes_h.index("bool SceneManager::writeSceneJson"):]:
    raise AssertionError("new streaming writer must not reserialize legacy synthParams")

apply_start = engine.index("void MiniAcid::applySceneStateFromManager()")
apply_end = engine.index("void MiniAcid::applyTextureFromScene_()", apply_start)
apply = engine[apply_start:apply_end]
for forbidden in ("genreManager_.applyGenreTimbre(*this)", "genreManager_.applyTexture(*this)"):
    if forbidden in apply:
        raise AssertionError(f"normal Scene Load still rewrites patch: {forbidden}")
if "syncTextureBiasBaselineFromCurrentState" not in apply:
    raise AssertionError("load must synchronize texture bias without applying it")
if "if (sceneManager_.hasVersionedSynthState())" not in apply:
    raise AssertionError("versioned synth state is not authoritative on load")
if "legacySynthParametersPresent(idx)" not in apply:
    raise AssertionError("legacy non-TB defaults cannot distinguish missing params")

sync_start = engine.index("void MiniAcid::syncSceneStateToManager()")
sync = engine[sync_start:]
if "getState()" not in sync or "setSynthPatch" not in sync:
    raise AssertionError("save path must capture normalized SwappableSynthVoice state")

print("synth persistence/load ownership source regressions: OK")
