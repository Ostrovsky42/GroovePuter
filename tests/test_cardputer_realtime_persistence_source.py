#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")
impl = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")

# Hardware policy: automatic persistence must never become runnable from the
# Cardputer display update path. The flag remains one byte so the fix does not
# buy responsiveness by increasing an already constrained DRAM footprint.
assert "defined(ARDUINO_M5STACK_CARDPUTER)" in header
assert "class RealtimeSafeAutoPersistenceFlag" in header
assert "operator bool() const { return false; }" in header
assert "sizeof(RealtimeSafeAutoPersistenceFlag) == 1" in header
assert "ui_session_save_pending_{false}" in header
assert "recovery_save_pending_{false}" in header

# Keep the source ordering pinned: these are the two synchronous writes whose
# pending flags are suppressed on Cardputer. Desktop/SDL still executes them.
assert "if (ui_session_save_pending_ && !mini_acid_.isPlaying()" in impl
assert "saveCardputerUiSession(ui_session_)" in impl
assert "if (!recovery_save_pending_ || mini_acid_.isPlaying()" in impl
assert "mini_acid_.autoSaveSceneRecovery()" in impl

# Explicit user-initiated Project persistence remains available and therefore
# this hardware workaround does not turn project storage into a no-op.
assert "mini_acid_.saveSceneAs(name)" in project
assert "mini_acid_.createNewSceneWithName(name)" in project

print("cardputer realtime persistence source regression passed")
