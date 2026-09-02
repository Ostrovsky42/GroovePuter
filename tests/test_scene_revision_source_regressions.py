#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    tracker = (ROOT / "src/state/scene_revision.h").read_text(encoding="utf-8")
    undo_owner = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")
    status = (ROOT / "src/ui/ui_status_chrome.h").read_text(encoding="utf-8")
    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    pattern = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
    feel_header = (ROOT / "src/ui/pages/feel_page.h").read_text(encoding="utf-8")
    feel_source = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
    song_header = (ROOT / "src/ui/pages/song_page.h").read_text(encoding="utf-8")
    song_source = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
    song_owner = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")
    genre_source = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
    voice_source = (ROOT / "src/ui/pages/voice_page.cpp").read_text(encoding="utf-8")
    sampler_source = (ROOT / "src/ui/pages/sampler_page.cpp").read_text(encoding="utf-8")
    drum_source = (ROOT / "src/ui/pages/drum_sequencer_page_legacy.h").read_text(encoding="utf-8")
    automation_source = (ROOT / "src/ui/pages/drum_automation_page.cpp").read_text(encoding="utf-8")
    tape_source = (ROOT / "src/ui/pages/tape_page.cpp").read_text(encoding="utf-8")

    require(not (ROOT / "src/ui/pages/texture_page.h").exists(),
            "removed TEXTURE page header must not return")
    require(not (ROOT / "src/ui/pages/texture_page.cpp").exists(),
            "removed TEXTURE page source must not return")
    require(not (ROOT / "src/ui/pages/generation_page.h").exists(),
            "removed standalone GENERATION page header must not return")
    require(not (ROOT / "src/ui/pages/generation_page.cpp").exists(),
            "removed standalone GENERATION page source must not return")

    require("uint32_t currentRevision" in tracker and
            "uint32_t persistedRevision" in tracker,
            "revision tracker must use two fixed-width counters")
    require("static_assert(sizeof(SceneRevisionState) == 8" in tracker,
            "revision tracker RAM contract is missing")
    require("UiStatusDirtyStamp dirty{}" in status and
            "sceneRevisionSnapshot()" in status and
            "revision.dirty()" in status and
            'status.dirty ? " *" : ""' in status,
            "status chrome must snapshot revision-aware dirty state and render it")
    require("markSceneLoadSucceeded();" in project,
            "successful project load must establish a clean baseline")
    require("markSceneSaveSucceeded();" in project,
            "successful project save/new must establish a clean baseline")
    require("restoreSceneRevision(revisionBefore);" in project,
            "failed manual save must restore the previous revision baseline")
    require("if (persisted) GroovePuterState::markSceneSaveSucceeded();" in project,
            "successful MIDI import persistence must establish a clean baseline")
    for mutation in (
        "led.mode",
        "led.source",
        "led.color",
        "led.brightness",
        "led.flashMs",
    ):
        position = project.index(mutation)
        window = project[position:position + 700]
        require("markSceneMutated" in window,
                f"Project persistent setting bypasses revision tracker: {mutation}")

    require("applySoundMacros" not in project,
            "removed Project sound-macro policy must not return")

    pattern_commit_start = pattern.index("template <typename PrepareFn>")
    pattern_commit_end = pattern.index("template <typename F>", pattern_commit_start)
    pattern_commit = pattern[pattern_commit_start:pattern_commit_end]
    owner_commit_start = undo_owner.index("bool commitPrepared")
    owner_commit_end = undo_owner.index("template <typename Payload>\n  bool read", owner_commit_start)
    owner_commit = undo_owner[owner_commit_start:owner_commit_end]
    require("undoOwner().commitPrepared" in pattern_commit and
            "markSceneMutated" not in pattern_commit and
            "GroovePuterState::markSceneMutated();" in owner_commit,
            "step editor mutations must reach the tracker through canonical Undo ownership")
    require("markSceneMutated();" in feel_header,
            "FEEL timing/velocity mutations must reach the tracker")

    # R4 separates committed Song arrangement from transport/TIME. Arrangement
    # mutations publish through UndoOwner; audio exclusion alone is runtime-only.
    require("SongPatternMaterializer::Result materializeSongTracks" in song_header and
            "bool generateCurrentCellPattern" in song_header and
            "bool generateEntireRow" in song_header,
            "Song must retain current generation/materialization entry points")
    song_commit_start = song_header.index("template <typename PrepareFn>")
    song_guard_start = song_header.index("template <typename F>", song_commit_start)
    song_commit = song_header[song_commit_start:song_guard_start]
    runtime_guard = song_header.index("void withRuntimeAudioGuard", song_guard_start)
    song_guard = song_header[song_guard_start:runtime_guard]
    require("undoOwner().commitPrepared" in song_commit and
            "UndoKind::Song" in song_commit and
            "markSceneMutated" not in song_commit and
            "GroovePuterState::markSceneMutated();" in owner_commit,
            "Song arrangement mutations must reach revision through canonical Undo ownership")
    require("markSceneMutated" not in song_guard,
            "Song audio guard must remain runtime-only after R4")
    require("commitSongMutation" in song_owner and
            "handleEventLegacyUnowned" in song_owner,
            "Song R4 owner wrapper must separate persistent edits from retained runtime routing")
    require("generateCurrentCellPattern();" in song_source and
            "generateEntireRow();" in song_source,
            "Song generation gestures must retain their materialization owner")

    preset_guard = feel_source.index("if (focus_ == FocusRow::Preset)")
    feel_guard_end = feel_source.index("Scene& scene", preset_guard)
    require("withAudioGuard" not in feel_source[preset_guard:feel_guard_end],
            "browsing a FEEL preset must remain UI-only until apply")

    require("void withRuntimeAudioGuard" in song_header,
            "Song runtime controls need a non-persistent audio guard")
    require("withRuntimeAudioGuard([&]() { mini_acid_.setLiveMixMode" in song_source,
            "LiveMix must remain runtime-only")
    ctrl_b_start = song_source.index("if (ui_event.ctrl && key_b)")
    ctrl_b_end = song_source.index("if (ui_event.alt && !ui_event.ctrl && key_b)", ctrl_b_start)
    ctrl_b = song_source[ctrl_b_start:ctrl_b_end]
    require("requestSongPlaybackSwitch" in ctrl_b and
            "setSongPlaybackSlot" not in ctrl_b and
            "commitSongMutation" not in ctrl_b and
            "markSceneMutated" not in ctrl_b,
            "Song playback slot must remain runtime-only through the D3 boundary owner")
    require("withAudioGuard([&]() { mini_acid_.toggleSongMode();" in song_source,
            "Song mode must retain its audio-safe runtime route")
    require("withAudioGuard([&]() { mini_acid_.setSongPosition(next);" in song_source,
            "Song position must retain its audio-safe TIME route")

    apply_mode_start = genre_source.index("void GenrePage::cycleApplyMode")
    apply_mode_end = genre_source.index("void GenrePage::applyCurrent", apply_mode_start)
    require("markSceneMutated" in genre_source[apply_mode_start:apply_mode_end],
            "GENRE apply policy must reach the revision tracker")

    require("withRuntimeAudioGuard([&]()" in voice_source,
            "Voice preview and runtime controls need a non-persistent guard")

    # Sampler E splits one persistent mutation path into two ownership phases:
    # sample selection becomes persistent only after successful control-side
    # preload + short ID publication, while the remaining pad parameters mark
    # the Scene dirty only if their value actually changed.
    require("if (selectIndexedSample(direction)) GroovePuterState::markSceneMutated();" in sampler_source,
            "Sampler sample assignment must mark only a successfully published replacement")
    require("if (changed) GroovePuterState::markSceneMutated();" in sampler_source,
            "Sampler pad parameter edits must mark only actual persistent changes")

    require("if (changed) GroovePuterState::markSceneMutated();" in drum_source,
            "Global Drum FX must mark actual persistent changes")
    require("GroovePuterState::markSceneMutated();" in automation_source,
            "Drum Automation edits must reach the revision tracker")
    require("Runtime-only stutter" not in tape_source,
            "source comments must not replace executable Tape boundaries")
    require(tape_source.count("GroovePuterState::markSceneMutated();") >= 10,
            "persisted Tape controls are missing revision boundaries")
    stutter_pos = tape_source.index("case '\\n': // Enter = stutter toggle")
    runtime_end = tape_source.index("case '\\b': // Backspace/Del = eject", stutter_pos)
    require("markSceneMutated" not in tape_source[stutter_pos:runtime_end],
            "Tape stutter must remain runtime-only")

    transport_start = display.index("// Pages get first refusal on Space")
    transport_end = display.index("if (event.event_type == GROOVEPUTER_KEY_DOWN &&\n        !event.alt", transport_start)
    transport_block = display[transport_start:transport_end]
    require("markSceneMutated" not in transport_block,
            "transport play/stop must remain runtime-only")

    live_start = display.index("WorkflowPages::allowsPerformanceKeyboard")
    live_end = display.index("if (event.event_type == GROOVEPUTER_KEY_DOWN)", live_start)
    require("markSceneMutated" not in display[live_start:live_end],
            "live note input must not mark the project dirty")


if __name__ == "__main__":
    main()
