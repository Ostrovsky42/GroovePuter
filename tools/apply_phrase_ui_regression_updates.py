#!/usr/bin/env python3
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, found {count}")
    return text.replace(old, new, 1)


root = Path(__file__).resolve().parents[1]
path = root / "tests/test_theme_selection_source_regressions.py"
text = path.read_text()
text = replace_once(
    text,
    '    require("case WorkflowMode::Song: return kArrange;" in workflow,\n            "SONG workflow must resolve to the song editor")',
    '    require("kArrange, kPhrase" in workflow and\n            "case WorkflowMode::Song: return kSongPages[index];" in workflow,\n            "SONG workflow must expose Arrange then Phrase Core")',
    "SONG workflow regression",
)
path.write_text(text)
print("Phrase UI regression updates applied")
