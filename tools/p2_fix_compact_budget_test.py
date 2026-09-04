#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/test_pattern_phrase_p2_pattern_bank.cpp"
text = path.read_text(encoding="utf-8")
old = "static_assert(sizeof(RuntimePatternEventBuffer) <= 162,"
new = "static_assert(sizeof(RuntimePatternEventBuffer) <= 164,"
if text.count(old) != 1:
    raise RuntimeError("expected one old compact-carrier budget assertion")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
print("P2 compact carrier test budget aligned to +2-byte source-step mask")
