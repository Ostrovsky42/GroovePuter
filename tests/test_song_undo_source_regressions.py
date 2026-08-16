#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SONG = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
CORE = (ROOT / "src/ui/ui_core.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require("GROOVEPUTER_APP_EVENT_UNDO" in CORE,
            "shared UI event contract must retain Undo")
    require("bool key_z" in SONG and "GROOVEPUTER_Z" in SONG,
            "Song must normalize the Z shortcut across key/scancode paths")
    require("(ui_event.ctrl || ui_event.meta) && !ui_event.alt && key_z" in SONG,
            "Song Undo must be reachable via Ctrl/Meta+Z without stealing Alt+Z")
    require("app_evt.app_event_type = GROOVEPUTER_APP_EVENT_UNDO" in SONG,
            "Song shortcut must route through the existing application Undo event")
    require("case GROOVEPUTER_APP_EVENT_UNDO" in SONG,
            "existing Song Undo handler must remain reachable")

    require("static constexpr int kMaxCells = Song::kMaxPositions * kMaxTracks" in SONG,
            "Undo payload must have an explicit compile-time cell bound")
    require("static_assert(UndoHistory::kMaxCells == 512" in SONG,
            "Undo bound must stay explicit for the current Song schema")
    require("static_assert(sizeof(UndoHistory) <= 1056" in SONG,
            "Undo DRAM budget must stay compile-time guarded")
    require("std::vector<UndoCell>" not in SONG,
            "Undo history must not allocate a dynamic cell vector")
    require("std::vector<int> old_patterns" not in SONG,
            "destructive Song edits must not allocate transient undo vectors")
    require("SongTrack tracks[kMaxTracks]" in SONG,
            "Undo must capture stable SongTrack identity, not lane-focus columns")
    require("mutation_revision == GroovePuterState::sceneRevisionSnapshot().currentRevision" in SONG,
            "Undo receipt must expire after a newer Scene mutation")
    require('showToast("Undo expired"' in SONG,
            "stale Undo must fail closed with feedback")
    require("old_song_length" in SONG and "old_song_reverse" in SONG,
            "Undo must restore Song structural metadata affected by whole-Song operations")
    require("captureWholeSong(UndoActionType::Cut" in SONG and
            "captureWholeSong(UndoActionType::Paste" in SONG,
            "whole-Song cut/paste must participate in bounded one-step Undo")
    require("g_undo_history.clear();" in SONG,
            "successful Undo must consume the one-step receipt")
    require("GROOVEPUTER_APP_EVENT_REDO" not in CORE and
            "GROOVEPUTER_APP_EVENT_REDO" not in SONG,
            "0.9.8-B2 must not introduce Redo")
    print("Song Undo source regressions: OK")


if __name__ == "__main__":
    main()
