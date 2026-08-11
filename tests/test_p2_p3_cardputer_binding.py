#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
input_header = (ROOT / "src/input/cardputer_input_edges.h").read_text()
display_header = (ROOT / "src/ui/miniacid_display.h").read_text()
runtime = (ROOT / "src/platform/cardputer_p2_p3_audition.cpp").read_text()

# Exact global entry chord: Ctrl+Alt+O only. Alt+H remains normal help.
assert "constexpr uint8_t kHidO = 0x12;" in runtime
assert "ctrl && alt && !shift && !fn && hid == kHidO" in runtime
assert "Alt+H" not in runtime
assert "event.alt" not in runtime

# Mode must own physical/word input while active so ordinary UI writes cannot
# leak through the test harness.
assert "p23AuditionConsumeCardputerHid" in input_header
assert "if (p23AuditionActive()) return false;" in input_header
assert "return true;" in runtime

# Exact fixture selector and exit contract.
assert "hid >= kHid1 && hid <= kHid4" in runtime
assert "hid == kHidEscape" in runtime
assert "P23 AUDITION CTRL+1..4" in runtime
assert "P23 AUDITION OFF" in runtime

# The harness is temporary RAM state, not a persistence path.
for forbidden in ("markSceneMutated", "saveScene", "autoSave", "saveCurrentPage"):
    assert forbidden not in runtime, forbidden
assert "P23AuditionBackup" in runtime
assert "restoreBackup" in runtime

# Cross-bar HOLD must explicitly avoid Song row switching because Stage15 Song
# selection emits AllNotesOff at row boundaries. Only MultiBarNS may use Song.
assert "renderCrossBarHoldProbe" in runtime
assert "repeating it avoids Song's" in runtime
assert "P2P3HardwareAuditionFixture::MultiBarNS" in runtime

# The existing UI toast renderer is reused rather than creating another page
# owner; hardware always has a visible long-lived mode/test label.
public_prefix = display_header.split("private:", 1)[0]
assert "void showToast(const char* msg" in public_prefix

print("P2/P3 Cardputer binding source contract: PASS")
