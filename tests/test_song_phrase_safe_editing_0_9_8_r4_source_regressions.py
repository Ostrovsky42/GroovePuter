#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
song = (ROOT / "src/ui/pages/song_page.cpp").read_text()
song_h = (ROOT / "src/ui/pages/song_page.h").read_text()
phrase = (ROOT / "src/ui/pages/phrase_page.cpp").read_text()
workspace = (ROOT / "src/phrase/phrase_workspace.h").read_text()
receipts = (ROOT / "src/state/song_phrase_undo_receipts.h").read_text()


def require(cond, message):
    if not cond:
        raise AssertionError(message)

# One authoritative owner. The retained legacy handler may keep a compile-only
# shim, but it must not retain a dynamic or bounded second history payload.
require("std::vector<UndoCell> cells" not in song,
        "Song still owns vector-backed page-local Undo history")
require("LegacyUndoHistoryDisabled" in song,
        "legacy Song handler is not explicitly disabled as an Undo owner")
require("std::array<UndoCell, 0> cells" in song,
        "legacy Song compatibility shim unexpectedly stores history")
require("commitPreparedSong" in song and "UndoKind::Song" in song,
        "Song manual edits do not route through canonical UndoOwner")
require("handleEventLegacyUnowned" in song and
        "GROOVEPUTER_APP_EVENT_UNDO" in song,
        "Song application Undo is not intercepted before legacy handling")
require("withRuntimeAudioGuard" in song_h,
        "Song canonical COMMIT must not reuse the revision-owning legacy guard")

# R4 boundary: generation and activation/reverse remain on their pre-existing
# paths; they are intentionally consolidated later.
require("generateCurrentCellPattern" in song and "generateEntireRow" in song,
        "R4 unexpectedly removed Song generation path")
require("queueSongReverseToggle" in song,
        "R4 crossed the reverse activation boundary")

# Phrase PREPARE must be pure and revision ownership must be canonical.
require("GroovePuterState::markSceneMutated" not in workspace,
        "PhraseWorkspace still owns Scene revision directly")
require("PhraseCore::PhraseBank after = before.before" in workspace,
        "Phrase PREPARE is not performed on a bounded PhraseBank copy")
require("Song after = before.before" in workspace,
        "Phrase->Song PREPARE is not performed on a bounded Song copy")
require("commitPrepared(\n      UndoKind::Phrase" in workspace,
        "PhraseBank mutation does not publish a canonical Phrase receipt")
require("commitPrepared(\n      UndoKind::Song" in workspace,
        "Phrase->Song mutation does not publish a canonical Song receipt")
require("PhraseBankUndoPayload" in receipts and "nextPhraseId" in receipts,
        "Phrase receipt rationale no longer covers allocator state")
require("GROOVEPUTER_APP_EVENT_UNDO" in phrase and "undoCurrentEdit" in phrase,
        "Phrase page has no application Undo path")
require("generatePhraseToSong" in phrase,
        "R4 unexpectedly absorbed generated Phrase->Song materialization")

print("0.9.8 R4 Song/Phrase source regressions: PASS")
