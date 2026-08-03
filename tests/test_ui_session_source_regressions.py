#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    state = (ROOT / "src/state/ui_session_state.h").read_text(encoding="utf-8")
    platform = (ROOT / "src/platform/cardputer_ui_session.cpp").read_text(encoding="utf-8")
    display_h = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    engine_h = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    storage = (ROOT / "scene_storage.h").read_text(encoding="utf-8")
    card_storage = (ROOT / "scene_storage_cardputer.cpp").read_text(encoding="utf-8")
    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")

    require("static_assert(sizeof(UiSessionState) <= 12" in state,
            "UI session RAM contract is missing")
    require("lastPageByWorkflow" in state and "workflowNavigationTarget" in state,
            "per-workflow page memory model is missing")
    require("Preferences" in platform and 'kSessionNamespace = "gp-session"' in platform,
            "Cardputer UI session must use a bounded NVS namespace")
    require("checksumRecord" in platform and "sanitizeUiSessionState" in platform,
            "NVS session record needs integrity and range validation")

    require("UiSessionState ui_session_" in display_h and
            "rememberWorkflowPage(ui_session_, index)" in display,
            "display transitions must update workflow page memory")
    require(display.count("workflowNavigationTarget") >= 2 and
            "rememberedWorkflowPage" in display,
            "workflow navigation must restore remembered pages")
    require("ui_session_save_due_ms_ = millis() + 1000" in display and
            "!mini_acid_.isPlaying()" in display,
            "NVS writes must be debounced and deferred until transport stops")
    require("masterVolumePermille" in state and
            "setDeviceMasterVolume" in display,
            "master volume must restore as device session state")

    require("autoSaveSceneRecovery" in engine_h and
            "writeSceneAuto(sceneManager_)" in engine,
            "dirty project recovery autosave is not wired")
    require("if (!sceneStorage_ || playing) return false" in engine,
            "recovery autosave must reject playback-time writes")
    require("hasSceneAuto" in storage and "clearSceneAuto" in storage,
            "storage recovery lifecycle contract is incomplete")
    require("return false;\n}\n\nbool SceneStorageCardputer::hasSceneAuto" in card_storage,
            "recovery reader must not silently fall back to the main project")
    require("sceneStorage_->clearSceneAuto()" in engine,
            "manual saves must clear stale recovery state")
    require("lastSceneLoadRecoveredAutosave" in project and
            '"Recovered unsaved project"' in project,
            "recovered projects must remain visibly dirty")

    volume_pos = project.index("MiniAcidParamId::MainVolume")
    volume_block = project[volume_pos:volume_pos + 260]
    require("markSceneMutated" not in volume_block,
            "device master volume must not dirty the musical project")


if __name__ == "__main__":
    main()
