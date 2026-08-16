#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SONG = ROOT / "src/ui/pages/song_page.cpp"
text = SONG.read_text(encoding="utf-8")


def sub_once(pattern: str, replacement: str, label: str, flags: int = 0) -> None:
    global text
    updated, count = re.subn(pattern, replacement, text, count=1, flags=flags)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, got {count}")
    text = updated


# The R4 public owner intercepts every current Song CUT/PASTE/UNDO/destructive
# edit before the retained legacy handler. Remove the obsolete page-local
# heap-backed owner so it cannot remain a second resident history mechanism.
sub_once(
    r"enum class UndoActionType \{.*?UndoHistory g_undo_history;\n",
    "SongPatternClipboard g_song_pattern_clipboard;\n"
    "SongAreaClipboard g_song_area_clipboard;\n"
    "SongSlotClipboard g_song_slot_clipboard;\n",
    "legacy Song Undo owner",
    re.S,
)

# The legacy UNDO branch is unreachable behind the R4 application-event owner
# and would otherwise retain an alternate restore path if routing changed later.
sub_once(
    r"      case GROOVEPUTER_APP_EVENT_UNDO: \{\n.*?\n      \}\n      default:",
    "      default:",
    "legacy Song UNDO event branch",
    re.S,
)

# Remove history publication calls left in retained legacy mutation code. Those
# gestures are intercepted by R4, but the dead calls must not keep a second
# ownership vocabulary alive.
text = re.sub(
    r"^[ \t]*g_undo_history\.(?:action_type|saveArea|saveSingleCell).*;\n",
    "",
    text,
    flags=re.M,
)

# Remove history-only before-images. Clipboard vectors remain untouched.
sub_once(
    r"\n[ \t]*// Save old patterns for undo\n[ \t]*std::vector<int> old_patterns;.*?\n[ \t]*withAudioGuard\(\[&\]\(\) \{",
    "\n          withAudioGuard([&]() {",
    "legacy paste before-image capture",
    re.S,
)
text = re.sub(r"^[ \t]*std::vector<int> old_patterns;\n", "", text, flags=re.M)
text = re.sub(r"^[ \t]*old_patterns\.reserve\([^\n]*\);\n", "", text, flags=re.M)
text = re.sub(r"^[ \t]*old_patterns\.push_back\([^\n]*\);\n", "", text, flags=re.M)
text = re.sub(r"^[ \t]*int track_idx = cursorTrack\(\);\n", "", text, flags=re.M)
text = re.sub(
    r"^[ \t]*int old_pattern = mini_acid_\.songPatternAt\(row, track\);\n",
    "",
    text,
    flags=re.M,
)
text = re.sub(
    r"([ \t]*// Save undo state\n)[ \t]*int current_pattern = mini_acid_\.songPatternAt\(row, track\);\n(?=[ \t]*\n[ \t]*withAudioGuard)",
    "",
    text,
    count=1,
)
text = re.sub(r"^[ \t]*// Save undo (?:state|history).*\n", "", text, flags=re.M)

for forbidden in (
    "UndoActionType",
    "struct UndoCell",
    "struct UndoHistory",
    "g_undo_history",
    "old_patterns",
    "old_pattern =",
    "track_idx = cursorTrack()",
    "case GROOVEPUTER_APP_EVENT_UNDO:",
):
    if forbidden in text:
        raise RuntimeError(f"legacy Song Undo residue remains: {forbidden}")

if "std::vector<int> pattern_indices;" not in text:
    raise RuntimeError("Song clipboard vector was accidentally removed")

SONG.write_text(text, encoding="utf-8")

TEST = ROOT / "tests/test_undo_0_9_8_r5_ownership_closure.py"
TEST.write_text(r'''#!/usr/bin/env python3
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
''', encoding="utf-8")

RUNNER = ROOT / "tests/run_undo_0_9_8_r5_tests.sh"
RUNNER.write_text('''#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bash tests/run_undo_0_9_8_r4_tests.sh
python3 tests/test_undo_0_9_8_r5_ownership_closure.py

echo "0.9.8 R5 ownership closure tests passed"
''', encoding="utf-8")
RUNNER.chmod(0o755)

DOC = ROOT / "docs/releases/0_9_8_R5_MUTATION_OWNERSHIP_CLOSURE.md"
DOC.write_text('''# 0.9.8 R5 — Mutation ownership closure

## Purpose

R5 is a closure audit, not a new editing feature. It verifies that the destructive
persistent editing domains completed in R2–R4 have one retained Undo owner and
that legacy page-local history cannot silently re-enter the active architecture.

## Closed ownership map

| Domain | Persistent edit owner | R5 result |
| --- | --- | --- |
| Pattern manual edits | `UndoOwner<1536>` / `UndoKind::Pattern` | closed |
| Song arrangement edits | `UndoOwner<1536>` / `UndoKind::Song` | closed |
| Phrase bank edits | `UndoOwner<1536>` / `UndoKind::Phrase` | closed |
| Phrase -> Song manual write | `UndoOwner<1536>` / `UndoKind::Song` | closed |
| Generation/materialization | specialized generation owner / 0.9.9 boundary | intentionally not migrated |
| Transport/navigation | runtime state | no Undo receipt, no fake revision |
| Save/load | persistence baseline | no fake user mutation |

## Legacy Song history removal

R4 made the page-local `UndoHistory` unreachable from the public Song mutation
path, but the old object still compiled into the page and retained a
`std::vector<UndoCell>`. R5 removes that second owner vocabulary entirely:

- no `UndoActionType`;
- no `UndoCell`;
- no `UndoHistory`;
- no `g_undo_history`;
- no legacy `GROOVEPUTER_APP_EVENT_UNDO` restore branch;
- no transient `old_patterns` before-images whose only consumer was that history.

Dynamic Song area clipboard storage remains. Clipboard data is transfer state,
not retained history, and is deliberately separate from `UndoOwner`.

## Direct revision marks outside the R5 destructive-edit domains

The repository still has domain-local persistent parameter controls (for example
FEEL/GENRE/Sampler/Drum/Tape) that advance the Scene revision directly. R5 does
not silently widen the 0.9.8 destructive Undo product contract to every scalar
parameter in the application. The closure criterion here is narrower and
explicit: Pattern, Song and Phrase manual/destructive edit paths have canonical
before-state receipts; runtime navigation does not dirty; generation activation
stays in 0.9.9.

Any future decision to make scalar parameter edits undoable must add a bounded
receipt/domain contract explicitly rather than introducing another page-local
history mechanism.

## Invariants

- exactly one retained Undo history owner: `UndoOwner<1536>`;
- successful canonical mutation replaces the previous receipt;
- failed/no-op preparation leaves the previous receipt intact;
- Save does not expire Undo;
- navigation does not expire Undo;
- Pattern/Song/Phrase page helpers do not call `markSceneMutated()` themselves;
- generation/pending activation is not pulled into R5.

## Acceptance

Focused R5 reruns all R4 acceptance (which itself reruns R2/R3) and adds source
closure checks for the removed Song owner and current ownership boundaries.
Normal exact-head host, SDL, ADV normal/fixed-DRAM and SEQTRAK MIDI-only gates
remain mandatory before merge.
''', encoding="utf-8")
