#!/usr/bin/env python3
"""H9 source regression: the D refusal points to FORM; keys.md names the same key."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
page = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
keys = (ROOT / "src/ui/docs/keys.md").read_text(encoding="utf-8")

assert "char genreMessage[48];" in page, "the refusal text fits a 48-byte buffer"
assert "\"%s CAN'T GROW: SONG F FORM\"" in page, "the refusal names the FORM route"
assert "ALT+V GENRE" not in page, "no stale key name in the refusal"
assert "CAN'T GROW: SONG F FORM" in keys, "keys.md shows the same text"
assert "FN+M GENRE" not in keys, "keys.md no longer names FN+M for the refusal"
assert "ALT+V GENRE" not in keys, "keys.md no longer names ALT+V for the refusal"
print("CAN'T GROW source regressions: OK")
