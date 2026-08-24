#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SONG = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text()
HEADER = (ROOT / "src/ui/pages/song_page.h").read_text()
PAGING = (ROOT / "src/audio/pattern_paging.cpp").read_text()


def body(source: str, signature: str, next_signature: str) -> str:
    return source.split(signature, 1)[1].split(next_signature, 1)[0]


generate = body(SONG, "bool SongPage::generatePatternPickerCandidate()", "bool SongPage::enterPatternPickerGenerateMode()")
accept = body(SONG, "bool SongPage::acceptPatternPicker()", "bool SongPage::discardPatternPicker()")
discard = body(SONG, "bool SongPage::discardPatternPicker()", "void SongPage::onExit()")
on_exit = body(SONG, "void SongPage::onExit()", "bool SongPage::handlePatternPickerEvent")
events = body(SONG, "bool SongPage::handlePatternPickerEvent", "bool SongPage::undoPreparedSongState")
commit = body(HEADER, "bool commitSongMutation", "bool handlePatternPickerEvent")

# Existing browse/audition is lease- and history-free; only accept commits.
existing = accept.split("if (mode == PatternPickerMode::Existing)", 1)[1].split("auto& owner", 1)[0]
assert "patternLeaseOwner" not in existing
assert "commitSongMutation" in existing
assert existing.index("commitSongMutation") < existing.rindex("closePatternPickerState")

# Generate has one state-owned lease, selected-track mask, and exact-address reroll.
assert "pattern_picker_.lease" in generate
assert "pattern_picker_.trackMask" in generate
assert "owner.acquire(" in generate
assert re.search(r"owner\.acquire\([\s\S]*?pattern_picker_\.page,\s*1,", generate)
assert "SongPatternCandidate::produce" in generate
assert "SongPatternCandidate::writeToLeasedAddress" in generate
assert "commitSongMutation" not in generate

# Persistent transfer order is frozen and failed commit keeps rollback ownership.
prepare = accept.index("owner.preparePersistentTransfer")
song_commit = accept.index("commitSongMutation", prepare)
complete = accept.index("owner.completePersistentTransfer", song_commit)
assert prepare < song_commit < complete
failed_commit = accept.split("if (!committed)", 1)[1].split("const PhrasePatternLease::LeaseStatus completedStatus", 1)[0]
assert "completePersistentTransfer" not in failed_commit
assert ".discard(" not in failed_commit

# Cancel, modal exit, and Generate->Existing all release through the sole owner.
assert "patternLeaseOwner().discard" in discard
assert "discardPatternPicker" in on_exit
assert "patternLeaseOwner().discard" in events
assert events.split("if (pattern_picker_.mode == PatternPickerMode::Generate)", 1)[1].count("return true") >= 2

# Exactly the canonical Song Undo owner records mutation; Picker owns no history.
assert "captureCurrentSongUndo" in commit
assert "undoOwner().commitPrepared" in commit
assert "UndoKind::Song" in commit
assert "UndoOwner" not in SONG

# Every physical-page replacement path is pinned by the existing singleton lease owner.
assert "patternLeaseOwner().activeLeaseCount() != 0" in PAGING
for signature in (
    "bool PatternPagingService::savePage",
    "bool PatternPagingService::loadPage",
    "bool PatternPagingService::restoreBackup",
):
    operation = PAGING.split(signature, 1)[1].split("\n}", 1)[0]
    assert "if (pageStoragePinnedByLease()) return false;" in operation

# Frozen ownership: no Picker allocator, Phrase KEEP, evolution, or second Undo path.
for forbidden in ("findFirstFreePattern", "Phrase KEEP", "EVOLVE", "DERIVE", "RELATED"):
    assert forbidden not in generate + accept + discard

print("0.9.9-IUI1 source ownership/lifecycle contract: PASS")
