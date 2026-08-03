#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "tests/test_performance_source_regressions.py"
text = path.read_text(encoding="utf-8")
old = '''    require("requestLoad(path.c_str())" in player_page and
            "requestLoadAndPlay" not in player_page,
            "Enter must load an SMF without creating hidden playback intent")
'''
new = '''    load_path = player_page[
        player_page.index("bool SmfPlayerPage::loadMidiPath(const char* path)"):
        player_page.index("bool SmfPlayerPage::togglePlayerTransport()")]
    require("player_->requestLoad(path)" in load_path and
            "requestLoadAndPlay" not in player_page and
            "EventResult::FileActivated" in player_page and
            "return loadMidiPath(activatedPath);" in player_page,
            "Enter must load the selected shared-browser path without hidden playback intent")
'''
if text.count(old) != 1:
    raise RuntimeError("legacy SMF requestLoad ownership assertion missing")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
