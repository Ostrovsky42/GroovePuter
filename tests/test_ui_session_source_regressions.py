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
    launcher = (ROOT / "src/ui/workspace_launcher_overlay.h").read_text(encoding="utf-8")
    engine_h = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    storage = (ROOT / "scene_storage.h").read_text(encoding="utf-8")
    card_storage = (ROOT / "scene_storage_cardputer.cpp").read_text(encoding="utf-8")
    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")

    require("static_assert(sizeof(UiSessionState) <= 12" in state,
            "UI session RAM contract is missing")
    require("lastPageByWorkflow" in state and "workflowNavigationTarget" in state,
            "per-workflow page memory model is missing")
    for forbidden_include in (
            "ui_core.h", "ui_config.h", "workflow_mode.h", "key_normalize.h"):
        require(forbidden_include not in state,
                f"session codec must not include Arduino-fragile UI header: {forbidden_include}")
    require("SessionWorkflow" in state and "SessionPages" in state,
            "session codec needs an independent fixed page/workflow wire map")
    require("Preferences" in platform and 'kSessionNamespace = "gp-session"' in platform,
            "Cardputer UI session must use a bounded NVS namespace")
    require("checksumRecord" in platform and "sanitizeUiSessionState" in platform,
            "NVS session record needs integrity and range validation")
    require("state.activePage == GroovePuterState::SessionPages::kPlayer" in platform and
            "GroovePuterState::rememberWorkflowPage(" in platform and
            "GroovePuterState::SessionPages::kPerform" in platform,
            "cold boot from persisted MIDI Player must start the PERFORM workflow on MIDI keyboard")

    require("UiSessionState ui_session_" in display_h and
            "rememberWorkflowPage(ui_session_, index)" in display,
            "display transitions must update workflow page memory")
    require("void switchWorkflow_(int direction);" in display_h and
            "rememberedAdjacentWorkflowPage" in display,
            "all top-level workflow switches need one remembered-page route")
    require(display.count("switchWorkflow_(") >= 6,
            "Fn+Tab, Fn brackets and physical bracket helpers must share switchWorkflow_()")
    require("ui_session_.lastPageByWorkflow" in display and
            "kWorkflowSessionCount" in display,
            "Fn+M launcher must receive all persisted workflow pages")

    page_dispatch = display.index("currentPage->handleEvent(event)")
    fn_left = display.index("event.meta && (event.key == '['")
    fn_right = display.index("event.meta && (event.key == ']'")
    require(fn_left < page_dispatch and fn_right < page_dispatch,
            "Fn brackets must be handled before page first refusal")
    require("[SESSION] load=" in display and
            "[SESSION] saved active=" in display and
            "[NAV] workflow dir=" in display,
            "hardware session routing must expose exact runtime state on Serial")

    require("child_by_workflow_" in launcher and
            "rememberedChild_(selected_)" in launcher and
            "loadRememberedPages_" in launcher,
            "launcher workflow selection must restore remembered child pages")
    require(launcher.count("child_by_workflow_[selected_] = child_") >= 2,
            "launcher must retain both the current and explicitly selected child page")
    require("child_ = 0;" not in launcher[launcher.index("GROOVEPUTER_UP"):launcher.index("GROOVEPUTER_LEFT")],
            "vertical launcher navigation must not reset every workflow to page zero")
    require("GROOVEPUTER / NAV R3" in launcher and
            "MEM %d %d %d %d %d" in launcher,
            "hardware build must expose its revision and remembered child pages")

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