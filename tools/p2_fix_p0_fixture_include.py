#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "tests/test_pattern_phrase_p0_runtime.cpp"
text = PATH.read_text(encoding="utf-8")
old = '#include "src/input/musical_event_queue.h"\n'
new = '#include "src/audio/pattern_paging.h"\n#include "src/input/musical_event_queue.h"\n'
count = text.count(old)
if count != 1:
    raise RuntimeError(f"expected one P0 fixture include anchor, got {count}")
PATH.write_text(text.replace(old, new, 1), encoding="utf-8")
print("P2/P0 compatibility fixture paging include applied")
