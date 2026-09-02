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
    receipts = (ROOT / "src/state/undo_receipts.h").read_text(encoding="utf-8")
    song_edit = (ROOT / "src/state/song_edit.h").read_text(encoding="utf-8")
    owner = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")
    song_header = (ROOT / "src/ui/pages/song_page.h").read_text(encoding="utf-8")
    song_cpp = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
    song_r4 = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")
    pattern_header = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
    pattern_cpp = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
    drum_cpp = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
    phrase_workspace = (ROOT / "src/phrase/phrase_workspace.h").read_text(encoding="utf-8")
    phrase_header = (ROOT / "src/ui/pages/phrase_page.h").read_text(encoding="utf-8")
    phrase_cpp = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")

    # Fixed receipts only: Song is one bounded arrangement before-image; Phrase
    # is one fixed PhraseBank. Neither is a Scene snapshot or retained history.
    require("struct SongUndoPayload" in receipts and
            "Song before{}" in receipts and
            "sizeof(SongUndoPayload) <= 1040" in receipts,
            "R4 Song receipt must remain one fixed ~1 KiB arrangement value")
    require("struct PhraseUndoPayload" in receipts and
            "PhraseCore::PhraseBank before{}" in receipts and
            "sizeof(PhraseUndoPayload) <= 248" in receipts,
            "R4 Phrase receipt must remain one fixed PhraseBank value")
    for forbidden in ("Scene before", "std::vector", "new ", "malloc", "ArduinoJson", "SD."):
        require(forbidden not in receipts,
                f"R4 receipts leaked unbounded/snapshot storage: {forbidden}")

    # Song PREPARE is a pure detached-value layer.
    for name in ("patternAt", "setPattern", "clearPattern", "flipBankReference",
                 "insertRow", "deleteRow", "reset"):
        require(f"{name}(" in song_edit, f"missing pure Song primitive: {name}")
    for forbidden in ("MiniAcid", "SceneManager", "UndoOwner", "markSceneMutated",
                      "std::vector", "ArduinoJson", "SD."):
        require(forbidden not in song_edit,
                f"Song PREPARE leaked runtime dependency: {forbidden}")

    song_commit = between(song_header, "template <typename PrepareFn>",
                          "void drawMinimalStyle")
    require("captureCurrentSongUndo" in song_commit and
            "Song after = before.before" in song_commit and
            "sameSong(before.before, after)" in song_commit and
            "songUndoTargetAvailable" in song_commit and
            "undoOwner().commitPrepared" in song_commit and
            "UndoKind::Song" in song_commit,
            "Song page must use detached PREPARE + canonical bounded COMMIT")
    require(song_commit.find("sameSong(before.before, after)") <
            song_commit.find("undoOwner().commitPrepared"),
            "Song no-op rejection must happen before Undo publication")
    require("markSceneMutated" not in song_commit,
            "Song page helper must not double-own revision advancement")

    guard = between(song_header, "template <typename F>",
                    "void startSelection")
    require("audio_guard_" in guard and "markSceneMutated" not in guard,
            "Song audio guard must be runtime-only after R4")
    require("bool SongPage::handleEventLegacyUnowned" in song_cpp and
            '#include "song_page_r4_owner.inc"' in song_cpp,
            "R4 must retain legacy Song routing behind the canonical owner")
    for token in ("GROOVEPUTER_APP_EVENT_UNDO", "GROOVEPUTER_APP_EVENT_CUT",
                  "GROOVEPUTER_APP_EVENT_PASTE", "commitSongMutation",
                  "SongEdit::insertRow", "SongEdit::deleteRow",
                  "SongEdit::flipBankReference", "SongEdit::clearPattern"):
        require(token in song_r4, f"Song R4 owner missing mutation surface: {token}")
    require("g_undo_history" not in song_r4,
            "R4 Song owner must not use the page-local vector UndoHistory")
    require("songR4GenerationGesture" in song_r4 and
            "return handleEventLegacyUnowned(ui_event);" in song_r4,
            "generation must remain outside R4 Song receipt ownership")
    require("songR4QueuedReverseGesture" in song_r4 and
            "return handleEventLegacyUnowned(ui_event);" in song_r4,
            "D3 must preserve Ctrl+R gesture routing while moving reverse persistence to the canonical Song owner")
    require("hasPendingSongReverseToggle" not in song_r4 and
            "GroovePuterState::markSceneMutated();" not in song_r4,
            "D3 must retire the private reverse queue and manual revision invalidation")

    # Cross-page Song writers must use the same canonical owner. Pattern/Drum
    # slot selection itself remains runtime-only.
    require("commitSongMutation" in pattern_header and
            "Song after = before.before" in pattern_header and
            "UndoKind::Song" in pattern_header,
            "Pattern chaining needs the canonical Song owner helper")
    pattern_selector = between(
        pattern_cpp,
        "// Q-I is the only keyboard path for slots 1-8 outside NOTE ENTRY.",
        "// Bank selection has one unambiguous binding.")
    require("commitSongMutation" in pattern_selector and
            "SongEdit::setPattern" in pattern_selector and
            "markSceneMutated" not in pattern_selector,
            "Pattern chaining must be persistent while selector navigation remains runtime-only")

    drum_selector = between(
        drum_cpp,
        "// Q-I changes the active Drum slot as runtime state.",
        "if (ui_event.ctrl && !ui_event.alt && !ui_event.meta")
    require("captureCurrentSongUndo" in drum_selector and
            "Song after = before.before" in drum_selector and
            "undoOwner().commitPrepared" in drum_selector and
            "UndoKind::Song" in drum_selector and
            "SongEdit::setPattern" in drum_selector,
            "Drum chaining must publish the same canonical Song receipt")
    require("page->withAudioGuard" not in drum_selector and
            "markSceneMutated" not in drum_selector,
            "Drum slot selection must not dirty the Scene or bypass Song ownership")

    # PhraseWorkspace is PREPARE-only. Page helpers own publication.
    for token in ("capturePrepared", "derivePrepared", "clearPrepared",
                  "writeToSongPrepared"):
        require(token in phrase_workspace, f"missing Phrase PREPARE API: {token}")
    for forbidden in ("UndoOwner", "undoOwner", "markSceneMutated", "AudioGuard",
                      "std::vector", "ArduinoJson", "SD."):
        require(forbidden not in phrase_workspace,
                f"Phrase PREPARE leaked ownership/runtime dependency: {forbidden}")
    require("commitPhraseMutation" in phrase_header and
            "PhraseCore::PhraseBank after = before.before" in phrase_header and
            "UndoKind::Phrase" in phrase_header and
            "commitSongMutation" in phrase_header and
            "Song after = before.before" in phrase_header and
            "UndoKind::Song" in phrase_header,
            "Phrase page must publish Phrase/Song domains through canonical owner helpers")
    for token in ("PhraseWorkspace::capturePrepared", "PhraseWorkspace::derivePrepared",
                  "PhraseWorkspace::clearPrepared", "PhraseWorkspace::writeToSongPrepared",
                  "undoPreparedOwnedState"):
        require(token in phrase_cpp, f"Phrase page missing R4 route: {token}")

    generated = between(phrase_cpp, "bool PhrasePage::generatePhraseToSong()",
                        "bool PhrasePage::deriveFromParent()")
    require("GeneratedPhraseSong::generate" in generated and
            "commitPhraseMutation" not in generated and
            "commitSongMutation" not in generated,
            "Generated Phrase -> Song must remain outside R4 PREPARE/COMMIT ownership")

    # Canonical owner remains the only resident history/revision publisher.
    require("kUndoPayloadBytes = 1536" in owner and
            "GroovePuterState::markSceneMutated();" in owner,
            "R4 must reuse the accepted R2 UndoOwner<1536>")

    print("0.9.8 R4 Song/Phrase ownership source regressions: OK")


if __name__ == "__main__":
    main()
