#!/usr/bin/env python3
"""0.9.23 T13: PERFORM surface text only (no key changes).

- RHYTHM > EUCLID draws a 16-step strip with a running cursor;
- the PERFORM header names the key ("C DORIAN +0") and mode badges
  ([C:..] [A:..] [R:..] [E:p/l] [S:..]) instead of the grey O/C/A/R/E/S/V line;
- the scale toast follows the existing bare LEFT/RIGHT and , . bindings;
  nothing is added to or removed from the key handling.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")
helper = (ROOT / "src/ui/perform_key_header.h").read_text(encoding="utf-8")

assert "PerformKeyHeader::euclidStepHit(" in page
assert "keyboard_.musicalPulseOrdinal() % length" in page
assert "kEuclideanSteps" in page
assert "PerformKeyHeader::formatKeyHeader(" in page
assert "O%+d C:%s A:%s R%u E%u S%u V%u" not in page
for badge in ('" [C:%s]"', '" [A:%s]"', '" [R:%u]"', '" [E:%u/%u]"', '" [S:%u]"'):
    assert badge in helper, badge
assert '"%s %s %+d"' in helper
assert '"SCALE: %s"' in page
# Scale cycling happens only through the toast helper plus the tools-layer row.
assert page.count("keyboard_.cycleScale(") == 1  # the tools-layer SCALE row
assert page.count("cycleScaleWithToast(keyboard_,") == 4  # LEFT, RIGHT, ',', '.'
# No new key bindings: the root is still changed only from the KEY tab.
assert page.count("keyboard_.cycleRoot(") == 1
print("test_perform_t13_source_regressions: OK")
