#!/usr/bin/env python3
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "src/ui/pages/song_page.cpp"
text = PATH.read_text(encoding="utf-8")
original = text


def sub_once(pattern: str, replacement: str, label: str, flags: int = 0) -> None:
    global text
    updated, count = re.subn(pattern, replacement, text, count=1, flags=flags)
    if count != 1:
        raise RuntimeError(f"R5 patch anchor mismatch for {label}: {count}")
    text = updated


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"R5 patch anchor mismatch for {label}: {count}")
    text = text.replace(old, new, 1)


# Remove the compiled page-local retained history types. Clipboard vectors stay.
sub_once(
    r"\nenum class UndoActionType \{.*?\n\};\n\nstruct UndoCell \{.*?\n\};\n\nstruct UndoHistory \{.*?\n\};\n",
    "\n",
    "legacy UndoHistory type block",
    re.DOTALL,
)
replace_once("UndoHistory g_undo_history;\n", "", "legacy UndoHistory instance")

# Legacy clear helper is still reachable only behind the R4 wrapper, but it must
# no longer allocate or publish a second history owner.
replace_once(
    "    std::vector<int> old_patterns;\n"
    "    old_patterns.reserve((max_row - min_row + 1) * (max_track - min_track + 1));\n",
    "",
    "clearPattern old-pattern vector",
)
replace_once("          old_patterns.push_back(before);\n", "", "clearPattern old-pattern capture")
replace_once(
    "\n    g_undo_history.action_type = UndoActionType::Delete;\n"
    "    g_undo_history.saveArea(min_row, max_row, min_track, max_track, old_patterns);\n",
    "",
    "clearPattern area history publish",
)
replace_once(
    "  \n  // Save undo state\n"
    "  int current_pattern = mini_acid_.songPatternAt(row, track);\n"
    "  g_undo_history.action_type = UndoActionType::Delete;\n"
    "  g_undo_history.saveSingleCell(row, cursorTrack(), current_pattern);\n"
    "  \n",
    "\n",
    "clearPattern single-cell history publish",
)

# CUT keeps clipboard capture but drops the dead legacy history copy.
replace_once(
    "          // Save undo state and copy/clear\n"
    "          std::vector<int> old_patterns;\n"
    "          old_patterns.reserve(rows * tracks);\n"
    "          \n",
    "",
    "CUT area history staging",
)
replace_once("                  old_patterns.push_back(pattern);\n", "", "CUT area history capture")
replace_once(
    "          // Save undo history\n"
    "          g_undo_history.action_type = UndoActionType::Cut;\n"
    "          g_undo_history.saveArea(min_row, max_row, min_track, max_track, old_patterns);\n",
    "",
    "CUT area history publish",
)
replace_once(
    "          // Save undo state\n"
    "          g_undo_history.action_type = UndoActionType::Cut;\n"
    "          g_undo_history.saveSingleCell(row, cursorTrack(), current_pattern);\n"
    "          \n",
    "",
    "CUT single-cell history publish",
)

# PASTE no longer snapshots its previous cells into the old page-local history.
sub_once(
    r"          // Save old patterns for undo\n.*?          withAudioGuard\(\[&\]\(\) \{",
    "          withAudioGuard([&]() {",
    "PASTE area legacy before-image staging",
    re.DOTALL,
)
replace_once(
    "          // Save undo history\n"
    "          g_undo_history.action_type = UndoActionType::Paste;\n"
    "          g_undo_history.saveArea(min_row, max_row, min_track, max_track, old_patterns);\n",
    "",
    "PASTE area history publish",
)
replace_once("          int track_idx = cursorTrack();\n", "", "PASTE single-cell track history index")
replace_once(
    "          // Save old pattern for undo\n"
    "          int old_pattern = mini_acid_.songPatternAt(row, track);\n"
    "          g_undo_history.action_type = UndoActionType::Paste;\n"
    "          g_undo_history.saveSingleCell(row, track_idx, old_pattern);\n"
    "          \n",
    "",
    "PASTE single-cell history publish",
)

# Canonical R4 wrapper owns GROOVEPUTER_APP_EVENT_UNDO; delete the second legacy
# implementation entirely.
sub_once(
    r"      case GROOVEPUTER_APP_EVENT_UNDO: \{\n.*?\n      \}\n      default:",
    "      default:",
    "legacy Song app-event Undo case",
    re.DOTALL,
)

for forbidden in ("UndoHistory", "UndoActionType", "UndoCell", "g_undo_history"):
    if forbidden in text:
        raise RuntimeError(f"R5 cleanup incomplete: {forbidden} remains")
if "std::vector<int> pattern_indices;" not in text:
    raise RuntimeError("R5 cleanup removed Song area clipboard storage")
if text == original:
    raise RuntimeError("R5 cleanup made no changes")

PATH.write_text(text, encoding="utf-8")
print("R5 removed legacy Song retained Undo owner")
