#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    tracker = (ROOT / "src/state/scene_revision.h").read_text(encoding="utf-8")
    status = (ROOT / "src/ui/ui_status_chrome.h").read_text(encoding="utf-8")
    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    pattern = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
    feel = (ROOT / "src/ui/pages/feel_texture_page.h").read_text(encoding="utf-8")
    mode = (ROOT / "src/ui/pages/mode_page.h").read_text(encoding="utf-8")
    settings = (ROOT / "src/ui/pages/settings_page.cpp").read_text(encoding="utf-8")
    song_header = (ROOT / "src/ui/pages/song_page.h").read_text(encoding="utf-8")
    song_source = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
    genre_source = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
    voice_source = (ROOT / "src/ui/pages/voice_page.cpp").read_text(encoding="utf-8")
    sampler_source = (ROOT / "src/ui/pages/sampler_page.cpp").read_text(encoding="utf-8")
    drum_source = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
    automation_source = (ROOT / "src/ui/pages/drum_automation_page.cpp").read_text(encoding="utf-8")
    tape_source = (ROOT / "src/ui/pages/tape_page.cpp").read_text(encoding="utf-8")

    require("uint32_t currentRevision" in tracker and
            "uint32_t persistedRevision" in tracker,
            "revision tracker must use two fixed-width counters")
    require("static_assert(sizeof(SceneRevisionState) == 8" in tracker,
            "revision tracker RAM contract is missing")
    require("bool dirty{GroovePuterState::sceneDirty()}" in status and
            'status.dirty ? " *" : ""' in status,
            "status chrome must snapshot and render dirty state")
    require("markSceneLoadSucceeded();" in project,
            "successful project load must establish a clean baseline")
    require("markSceneSaveSucceeded();" in project,
            "successful project save/new must establish a clean baseline")
    require("restoreSceneRevision(revisionBefore);" in project,
            "failed manual save must restore the previous revision baseline")
    require("if (persisted) GroovePuterState::markSceneSaveSucceeded();" in project,
            "successful MIDI import persistence must establish a clean baseline")
    for mutation in (
        "MiniAcidParamId::MainVolume",
        "genre.applySoundMacros",
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
    require("markSceneMutated();" in pattern,
            "step editor mutations must reach the tracker")
    require("markSceneMutated();" in feel,
            "Feel mutations must reach the tracker")
    require("markSceneMutated();" in mode,
            "phrase generation/randomize must reach the tracker")
    require("markSceneMutated();" in settings,
            "generator parameter edits must reach the tracker")
    require("void withRuntimeAudioGuard" in song_header,
            "Song runtime controls need a non-persistent audio guard")
    require("withRuntimeAudioGuard([&]() { mini_acid_.setLiveMixMode" in song_source,
            "LiveMix must remain runtime-only")
    require("withRuntimeAudioGuard([&]() { mini_acid_.setSongPlaybackSlot" in song_source,
            "Song playback slot must remain runtime-only")
    require("withAudioGuard([&]() { mini_acid_.toggleSongMode();" in song_source,
            "persisted Song mode must remain a tracked mutation")
    require("withAudioGuard([&]() { mini_acid_.setSongPosition(next);" in song_source,
            "persisted Song position must remain a tracked mutation")
    require(genre_source.count("cycleApplyMode(gs);\n        GroovePuterState::markSceneMutated();") == 2,
            "both Genre apply-mode controls must mark persistent changes")
    require("if (v != before) GroovePuterState::markSceneMutated();" in genre_source,
            "Genre texture amount must mark only real value changes")
    curated_start = genre_source.index("void GenrePage::setCuratedMode")
    curated_end = genre_source.index("int GenrePage::visibleTextureCount", curated_start)
    require("markSceneMutated" in genre_source[curated_start:curated_end],
            "Genre curated mode must reach the revision tracker")
    require("withRuntimeAudioGuard([&]()" in voice_source,
            "Voice preview and runtime controls need a non-persistent guard")
    require("if (persistentTarget && changed) GroovePuterState::markSceneMutated();" in sampler_source,
            "Sampler pad edits must mark only actual persistent changes")
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
