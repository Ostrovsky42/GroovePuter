#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    begin = text.find(start)
    require(begin >= 0, f"missing source anchor: {start}")
    finish = text.find(end, begin + len(start))
    require(finish >= 0, f"missing source end anchor: {end}")
    return text[begin:finish]


def main() -> None:
    song_cpp = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
    song_owner = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")
    song_header = (ROOT / "src/ui/pages/song_page.h").read_text(encoding="utf-8")
    pattern_header = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
    phrase_workspace = (ROOT / "src/phrase/phrase_workspace.h").read_text(encoding="utf-8")
    phrase_header = (ROOT / "src/ui/pages/phrase_page.h").read_text(encoding="utf-8")
    phrase_cpp = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
    project_cpp = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")
    undo_owner = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")

    # R5 physically closes the old Song page-local retained history. Dynamic
    # vectors are allowed only as clipboard payload storage.
    for forbidden in ("UndoHistory", "UndoActionType", "UndoCell", "g_undo_history"):
        require(forbidden not in song_cpp,
                f"legacy Song retained owner still exists: {forbidden}")
    require("std::vector<int> pattern_indices;" in song_cpp,
            "Song area clipboard vector must remain available")
    require("SongAreaClipboard" in song_cpp and "SongSlotClipboard" in song_cpp,
            "R5 must not remove clipboard storage while removing legacy Undo")

    # The application-level Undo event has one active Song route: the canonical
    # R4 owner wrapper. Legacy routing must not provide a second implementation.
    legacy_app = between(
        song_cpp,
        "if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT)",
        "if (ui_event.event_type != GROOVEPUTER_KEY_DOWN)")
    require("GROOVEPUTER_APP_EVENT_UNDO" not in legacy_app,
            "legacy Song application routing still owns Undo")
    require("GROOVEPUTER_APP_EVENT_UNDO" in song_owner and
            "undoPreparedSongState" in song_owner,
            "canonical Song wrapper must remain the only Song Undo route")
    require("GROOVEPUTER_APP_EVENT_CUT" in song_owner and
            "GROOVEPUTER_APP_EVENT_PASTE" in song_owner and
            "commitSongMutation" in song_owner,
            "Song CUT/PASTE must remain canonical persistent mutations")

    # Canonical Song COMMIT owns revision publication via UndoOwner; audio
    # exclusion and navigation must not become mutation ownership.
    song_commit = between(song_header, "template <typename PrepareFn>",
                          "void drawMinimalStyle")
    require("undoOwner().commitPrepared" in song_commit and
            "UndoKind::Song" in song_commit and
            "markSceneMutated" not in song_commit,
            "Song COMMIT must stay routed through canonical UndoOwner")
    owner_commit = between(undo_owner, "bool commitPrepared",
                           "template <typename Payload>\n  bool read")
    require("GroovePuterState::markSceneMutated();" in owner_commit,
            "canonical owner must remain the sole revision publisher for owned edits")

    # Pattern and Phrase retain the same canonical ownership boundary.
    pattern_commit = between(pattern_header, "template <typename PrepareFn>",
                             "template <typename F>")
    require("undoOwner().commitPrepared" in pattern_commit and
            "markSceneMutated" not in pattern_commit,
            "Pattern persistent edits must stay on canonical owner")
    for token in ("capturePrepared", "derivePrepared", "clearPrepared",
                  "writeToSongPrepared"):
        require(token in phrase_workspace,
                f"Phrase PREPARE API missing after ownership closure: {token}")
    for forbidden in ("undoOwner(", "markSceneMutated(", "withAudioGuard",
                      "audio_guard_", "std::vector"):
        require(forbidden not in phrase_workspace,
                f"Phrase PREPARE leaked retained/runtime ownership: {forbidden}")
    require("commitPhraseMutation" in phrase_header and
            "commitSongMutation" in phrase_header and
            "undoPreparedOwnedState" in phrase_cpp,
            "Phrase page must keep canonical Phrase/Song publication")

    # Generation/activation remain outside 0.9.8-R5. Do not accidentally fold
    # them back into generic Song ownership while deleting the legacy owner.
    require("songR4GenerationGesture" in song_owner and
            "return handleEventLegacyUnowned(ui_event);" in song_owner,
            "generation boundary must remain outside R5 Song ownership")
    require("hasPendingSongReverseToggle" in song_owner and
            "GroovePuterState::markSceneMutated();" in song_owner,
            "queued reverse must remain the explicit 0.9.9 activation boundary")

    # Save/load are persistence baselines, not fake user mutations. Save must
    # not be rewritten as an Undo-owning edit during closure.
    require("markSceneLoadSucceeded();" in project_cpp,
            "project load must establish a clean revision baseline")
    require("markSceneSaveSucceeded();" in project_cpp,
            "project save must establish a clean revision baseline")

    print("0.9.8 R5 mutation ownership closure: OK")


if __name__ == "__main__":
    main()
