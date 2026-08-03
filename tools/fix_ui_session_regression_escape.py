#!/usr/bin/env python3
from pathlib import Path

path = Path("tests/test_ui_session_source_regressions.py")
text = path.read_text(encoding="utf-8")
old = 'require("return false;\n}\n\nbool SceneStorageCardputer::hasSceneAuto" in card_storage,'
new = 'require("return false;\\n}\\n\\nbool SceneStorageCardputer::hasSceneAuto" in card_storage,'
if text.count(old) != 1:
    raise RuntimeError("generated recovery assertion anchor missing")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
