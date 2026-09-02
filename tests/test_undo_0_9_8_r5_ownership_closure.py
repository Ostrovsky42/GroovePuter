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
    owner = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")
    pattern_h = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
    song_h = (ROOT / "src/ui/pages/song_page.h").read_text(encoding="utf-8")
    song_cpp = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
    song_r4 = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")
    phrase_h = (ROOT / "src/ui/pages/phrase_page.h").read_text(encoding="utf-8")
    phrase_cpp = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
    workspace = (ROOT / "src/phrase/phrase_workspace.h").read_text(encoding="utf-8")
    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")

    require("kUndoPayloadBytes = 1536" in owner and "class UndoOwner" in owner,
            "canonical bounded UndoOwner<1536> is missing")
    for forbidden in ("UndoActionType", "struct UndoCell", "struct UndoHistory",
                      "g_undo_history", "old_patterns"):
        require(forbidden not in song_cpp,
                f"legacy Song history residue remains: {forbidden}")
    require("std::vector<int> pattern_indices;" in song_cpp,
            "Song area clipboard storage must remain available")
    require("GROOVEPUTER_APP_EVENT_UNDO" not in song_cpp,
            "retained Song legacy handler must not own a second Undo route")
    require("GROOVEPUTER_APP_EVENT_UNDO" in song_r4 and
            "undoPreparedSongState" in song_r4,
            "Song Undo must route only through the R4 canonical owner")

    pattern_commit = between(pattern_h, "template <typename PrepareFn>",
                             "template <typename F>")
    require("undoOwner().commitPrepared" in pattern_commit and
            "UndoKind::Pattern" in pattern_commit and
            "markSceneMutated" not in pattern_commit,
            "Pattern persistent edits escaped canonical ownership")

    song_commit = between(song_h, "template <typename PrepareFn>",
                          "template <typename F>")
    require("undoOwner().commitPrepared" in song_commit and
            "UndoKind::Song" in song_commit and
            "markSceneMutated" not in song_commit,
            "Song persistent edits escaped canonical ownership")

    phrase_commit = between(phrase_h, "template <typename PrepareFn>",
                            "bool undoPreparedOwnedState")
    require("undoOwner().commitPrepared" in phrase_commit and
            "UndoKind::Phrase" in phrase_commit and
            "UndoKind::Song" in phrase_commit and
            "markSceneMutated" not in phrase_commit,
            "Phrase/Song persistent edits escaped canonical ownership")

    for forbidden in ("UndoOwner", "undoOwner", "markSceneMutated", "AudioGuard",
                      "std::vector", "ArduinoJson", "SD."):
        require(forbidden not in workspace,
                f"Phrase PREPARE layer leaked ownership/runtime state: {forbidden}")

    generated = between(phrase_cpp, "bool PhrasePage::generatePhraseToSong()",
                        "bool PhrasePage::deriveFromParent()")
    require("GeneratedPhraseSong::generate" in generated and
            "commitPhraseMutation" not in generated and
            "commitSongMutation" not in generated,
            "R5 crossed the generation/materialization ownership boundary")

    require("markSceneSaveSucceeded();" in project and
            "markSceneLoadSucceeded();" in project,
            "project persistence clean-baseline hooks are missing")

    print("0.9.8 R5 mutation ownership closure: OK")


if __name__ == "__main__":
    main()
